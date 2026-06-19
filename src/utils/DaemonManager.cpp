// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "utils/DaemonManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

#include "utils/config.h"
#include "utils/Utils.h"

DaemonManager::DaemonManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    // The embedded wownerod is unpacked here (writable, outside any read-only app bundle).
    this->daemonDir = Config::defaultConfigDir().filePath("node");
    this->blockchainDir = Config::defaultConfigDir().filePath("node/blockchain");

    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &DaemonManager::handleProcessOutput);
    connect(m_process, &QProcess::errorOccurred, this, &DaemonManager::handleProcessError);
    connect(m_process, &QProcess::stateChanged, this, &DaemonManager::onStateChanged);
}

QPointer<DaemonManager> DaemonManager::m_instance(nullptr);

QString DaemonManager::rpcAddress() const {
    return QString("%1:%2").arg(this->rpcHost, QString::number(this->rpcPort));
}

void DaemonManager::init() {
    // Make sure the data directory exists before wownerod tries to write to it.
    QDir().mkpath(this->blockchainDir);
}

void DaemonManager::start() {
    if (!shouldStartDaemon())
        return;

    auto state = m_process->state();
    if (state == QProcess::ProcessState::Running || state == QProcess::ProcessState::Starting)
        return;

    if (!this->unpackBins()) {
        this->setErrorMessage("Unable to unpack the embedded wownerod binary.");
        return;
    }

    if (Utils::portOpen(this->rpcHost, this->rpcPort)) {
        // Something is already listening on our RPC port; assume it is a usable local node.
        m_alreadyRunning = true;
        emit daemonStateChanged(true);
        return;
    }

    m_restarts += 1;
    if (m_restarts > 4) {
        this->setErrorMessage("wownerod failed to start: maximum retries exceeded");
        return;
    }

    QStringList arguments;
    arguments << "--data-dir" << this->blockchainDir;
    arguments << "--rpc-bind-ip" << this->rpcHost;
    arguments << "--rpc-bind-port" << QString::number(this->rpcPort);
    arguments << "--p2p-bind-port" << QString::number(this->p2pPort);
    arguments << "--non-interactive";
    arguments << "--no-igd";                 // no UPnP port mapping
    arguments << "--out-peers" << "16";
    arguments << "--log-file" << QDir(this->blockchainDir).filePath("wownerod.log");

    // Full archival by default (wownero's chain is small); prune is an explicit opt-in.
    if (conf()->get(Config::pruneBlockchain).toBool())
        arguments << "--prune-blockchain";

    // wowlet: broadcast our OWN txs through Tor — hides the originating IP (the embedded node already
    // gives us scan/receive privacy; this closes the send side). Sync stays on clearnet for speed.
    // wownerod queues local txs until the proxy is reachable, so it tolerates Tor still bootstrapping.
    // Only when Tor is actually bundled (HAS_TOR_BIN) — --tx-proxy is exclusive, so without a running
    // Tor it would silently stall broadcasts. Builds without embedded Tor fall back to clearnet relay.
#if defined(HAS_TOR_BIN)
    if (conf()->get(Config::broadcastOverTor).toBool()) {
        quint16 torPort = conf()->get(Config::useLocalTor).toBool()
                            ? conf()->get(Config::socks5Port).toString().toUShort()
                            : conf()->get(Config::torManagedPort).toString().toUShort();
        arguments << "--tx-proxy" << QString("tor,127.0.0.1:%1,16").arg(torPort);
    }
#endif

    qDebug() << QString("Starting wownerod: %1 %2").arg(this->daemonPath, arguments.join(" "));

    m_process->start(this->daemonPath, arguments);
    m_started = true;
}

void DaemonManager::stop() {
    m_stopping = true;
    if (m_process->state() == QProcess::ProcessState::NotRunning) {
        m_started = false;
        return;
    }

    // wownerod owns an LMDB database. Send SIGTERM so it flushes and exits cleanly; only hard-kill
    // if it refuses to shut down, to avoid blockchain database corruption.
    m_process->terminate();
    if (!m_process->waitForFinished(30000)) {
        qWarning() << "wownerod did not exit gracefully within 30s; killing";
        m_process->kill();
        m_process->waitForFinished(5000);
    }
    m_started = false;
}

void DaemonManager::onStateChanged(QProcess::ProcessState state) {
    if (state == QProcess::ProcessState::Running) {
        this->setErrorMessage("");
        emit daemonStateChanged(true);
        qWarning() << "wownerod started";
    }
    else if (state == QProcess::ProcessState::NotRunning) {
        emit daemonStateChanged(false);
        if (m_stopping)
            return;

        // Unexpected exit: try to bring it back up shortly.
        QTimer::singleShot(2000, this, [this] { this->start(); });
    }
}

void DaemonManager::handleProcessOutput() {
    QByteArray output = m_process->readAllStandardOutput();
    this->logs.append(Utils::barrayToString(output));
    emit logsUpdated();

    // wownerod reports sync progress as "Synced <height>/<target>". Surface it for the UI.
    static const QRegularExpression syncRe(R"(Synced (\d+)\/(\d+))");
    QRegularExpressionMatch m = syncRe.match(QString::fromUtf8(output));
    if (m.hasMatch()) {
        quint64 height = m.captured(1).toULongLong();
        quint64 target = m.captured(2).toULongLong();
        emit syncStatusChanged(height, target);
        if (target > 0 && height >= target && !m_synced) {
            m_synced = true;
            emit synced();
        }
    }
}

void DaemonManager::handleProcessError(QProcess::ProcessError error) {
    if (error == QProcess::ProcessError::Crashed)
        qWarning() << "wownerod crashed or was killed";
    else if (error == QProcess::ProcessError::FailedToStart)
        this->setErrorMessage("wownerod binary failed to start: " + this->daemonPath);
}

bool DaemonManager::unpackBins() {
    QString daemonBin = "wownerod";
#if defined(Q_OS_WIN)
    daemonBin += ".exe";
#endif

    // 1. Explicit path the user configured (or already-running node they point us at).
    QString configured = conf()->get(Config::wownerodPath).toString();
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) {
        this->daemonPath = configured;
        return true;
    }

    // 2. Shipped next to the wowlet executable — the packaging default (monero-GUI ships monerod
    //    the same way). Avoids bloating the app binary by embedding a ~15 MB daemon in resources.
    QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(daemonBin);
    if (QFileInfo(sibling).isFile()) {
        this->daemonPath = sibling;
        return true;
    }

    // 3. Unpack the embedded copy into the (writable) data dir — the single-file default, all platforms.
    this->daemonPath = QDir(this->daemonDir).filePath(daemonBin);

    if (m_unpacked)
        return true;

    if (QFileInfo(this->daemonPath).isFile()) {
        m_unpacked = true;
        return true;
    }

    QDir().mkpath(this->daemonDir);
    QDirIterator it(":/assets/wownerod", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString assetFile = it.next();
        QFileInfo info(assetFile);
        QFile f(assetFile);
        f.copy(QDir(this->daemonDir).filePath(info.fileName()));
        f.close();
    }

#if defined(Q_OS_UNIX)
    QFile bin(this->daemonPath);
    bin.setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther
                       | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
#endif

    if (!QFileInfo(this->daemonPath).isFile()) {
        qWarning() << "wownerod was not unpacked (built without embedded daemon?)";
        return false;
    }

    m_unpacked = true;
    return true;
}

bool DaemonManager::shouldStartDaemon() {
    m_alreadyRunning = false;

    // The headline feature: run our own node by default. Opt-out is an explicit setting.
    if (!conf()->get(Config::runLocalNode).toBool())
        return false;

    return true;
}

void DaemonManager::setErrorMessage(const QString &msg) {
    this->errorMsg = msg;
    if (!msg.isEmpty())
        qWarning() << "DaemonManager:" << msg;
    emit statusChanged(msg);
}

DaemonManager* DaemonManager::instance()
{
    if (!m_instance) {
        m_instance = new DaemonManager(QCoreApplication::instance());
    }
    return m_instance;
}

DaemonManager::~DaemonManager() {
    this->stop();
}
