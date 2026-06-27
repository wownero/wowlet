// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "utils/DaemonManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include "constants.h"
#include "utils/AppData.h"
#include "utils/ChildProcessGuard.h"   // wowlet: OS-level die-with-parent for the embedded node
#include "utils/config.h"
#include "utils/networktype.h"
#include "utils/TorManager.h"
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

    // wowlet: SIGKILL wownerod if wowlet dies (Unix); Windows binds it to a kill-on-close job post-start.
    ChildProcessGuard::installPreStart(m_process);
    // wowlet: graceful stop on any clean quit, not just the last-window-closed path (flushes LMDB).
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]{ this->stop(); });
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

    // wowlet: a deliberate (re)start clears the stop latch, so a later UNEXPECTED exit can auto-restart
    // again. Without this, m_stopping stays true after the first node toggle-off and the crash
    // auto-restart is silently dead for the rest of the session.
    m_stopping = false;

    if (!this->unpackBins()) {
        this->setErrorMessage("Unable to unpack the embedded wownerod binary.");
        return;
    }

    // wowlet: don't trust a bare open port. Before adopting whatever is listening on our RPC port,
    // verify via get_info that it's a healthy wownero-mainnet wownerod (right nettype, sane height,
    // making progress) — not a stale/stuck orphan from a prior crash or an unrelated squatter.
    if (Utils::portOpen(this->rpcHost, this->rpcPort)) {
        if (this->verifyAdoptableNode()) {
            qDebug() << "Adopting existing wownerod on" << this->rpcAddress();
            m_alreadyRunning = true;
            emit daemonStateChanged(true);
            return;
        }
        qWarning() << "Refusing to adopt unverified listener on" << this->rpcAddress()
                   << "- will spawn a managed wownerod instead";
        this->setErrorMessage(
            QString("Found an unrecognized process on %1; starting a fresh node.").arg(this->rpcAddress()));
        // fall through to spawn; if the port/data-dir is genuinely held, the spawn fails to bind
        // and surfaces a clear error via handleProcessError().
    }

    // wowlet: the retry cap guards against a rapid crash-loop, not legit restarts. If the node had been
    // up a while before this (re)start, clear the budget — otherwise a few node toggles over a session
    // trip "maximum retries exceeded" and it won't start again until wowlet is relaunched.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastSpawnMs != 0 && (nowMs - m_lastSpawnMs) > 60000)
        m_restarts = 0;
    m_lastSpawnMs = nowMs;

    m_restarts += 1;
    if (m_restarts > 4) {
        this->setErrorMessage("wownerod failed to start: maximum retries exceeded");
        return;
    }

    // wowlet: allow pointing the embedded node at an existing chain (e.g. ~/.wownero) to skip a full
    // re-sync. Empty = the managed, isolated default. LMDB is single-writer, so the chosen dir must not
    // be in use by another running wownerod.
    QString dataDir = conf()->get(Config::nodeDataDir).toString().trimmed();
    if (dataDir.isEmpty())
        dataDir = this->blockchainDir;
    QDir().mkpath(dataDir);

    QStringList arguments;
    arguments << "--data-dir" << dataDir;
    arguments << "--rpc-bind-ip" << this->rpcHost;
    arguments << "--rpc-bind-port" << QString::number(this->rpcPort);
    arguments << "--p2p-bind-port" << QString::number(this->p2pPort);
    arguments << "--non-interactive";
    arguments << "--no-igd";                 // no UPnP port mapping
    arguments << "--out-peers" << "16";
    arguments << "--log-file" << QDir(dataDir).filePath("wownerod.log");
    // wowlet: disable LMDB batch write mode so every block is committed in its own transaction.
    // Without this, wownero accumulates blocks in a single large write transaction that is ABORTED
    // (not committed) when the daemon shuts down mid-sync — causing all progress since the last
    // batch boundary to be silently lost. This is visible in wownerod.log: sessions consistently
    // revert to a stale height despite a full graceful shutdown sequence. Using "safe" mode is
    // slower for initial sync but is the only correct choice for an embedded node that starts and
    // stops with the wallet; the wownero chain is small enough that the overhead is acceptable.
    arguments << "--db-sync-mode" << "safe";

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
        // Use the Tor that's actually running, not the stored preference: a managed Tor listens on
        // torManagedPort, an external/system Tor on socks5Port. (Reading Config::useLocalTor here picked
        // the wrong port whenever wowlet fell back to a system Tor, so broadcasts silently stalled.)
        quint16 torPort = torManager()->isLocalTor()
                            ? conf()->get(Config::socks5Port).toString().toUShort()
                            : conf()->get(Config::torManagedPort).toString().toUShort();
        arguments << "--tx-proxy" << QString("tor,127.0.0.1:%1,16").arg(torPort);
    }
#endif

    qDebug() << QString("Starting wownerod: %1 %2").arg(this->daemonPath, arguments.join(" "));

    m_process->start(this->daemonPath, arguments);
    ChildProcessGuard::adoptAfterStart(m_process);   // wowlet: bind to kill-on-crash job (Windows)
    m_started = true;
}

void DaemonManager::stop(bool wait) {
    m_stopping = true;
    if (m_process->state() == QProcess::ProcessState::NotRunning) {
        m_started = false;
        return;
    }

    // wownerod owns an LMDB database. Send SIGTERM so it flushes and exits cleanly; only hard-kill
    // if it refuses to shut down, to avoid blockchain database corruption.
    m_process->terminate();
    m_started = false;
    if (!wait)
        return;   // toggle/live use: let it flush + exit in the background instead of freezing the UI thread.
    if (!m_process->waitForFinished(30000)) {
        qWarning() << "wownerod did not exit gracefully within 30s; killing";
        m_process->kill();
        m_process->waitForFinished(5000);
    }
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

// wowlet: synchronous, bounded get_info probe used by start() to decide whether an already-open RPC
// port is a node we can safely adopt. Returns true ONLY for a healthy wownero-mainnet wownerod.
// Self-contained (own QNetworkAccessManager + local QEventLoop with a hard timeout) so it can run
// inside the synchronous start() at boot, before any wallet/DaemonRpc exists. Mirrors TorManager's
// blocking getVersion() probe idiom.
bool DaemonManager::verifyAdoptableNode() {
    if (conf()->get(Config::offlineMode).toBool())
        return false;

    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(QString("http://%1/get_info").arg(this->rpcAddress())));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QNetworkReply *reply = nam.post(req, QByteArray("{}"));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timeout.start(1500);          // hard cap on UI-thread block
    loop.exec();

    if (!reply->isFinished()) {   // timed out
        reply->abort();
        reply->deleteLater();
        qWarning() << "get_info verify: no response from" << this->rpcAddress() << "within 1.5s";
        return false;
    }

    bool netOk = reply->error() == QNetworkReply::NoError;
    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (!netOk || data.isEmpty() || !Utils::validateJSON(data)) {
        qWarning() << "get_info verify: not a JSON-RPC daemon on" << this->rpcAddress();
        return false;
    }

    QJsonObject obj = QJsonDocument::fromJson(data).object();

    if (obj.value("status").toString() != "OK") {           // 1. real wownerod RPC
        qWarning() << "get_info verify: bad status" << obj.value("status").toString();
        return false;
    }

    const QString wantNet = constants::networkType == NetworkType::MAINNET   ? "mainnet"   // 2. nettype
                          : constants::networkType == NetworkType::TESTNET   ? "testnet"
                                                                             : "stagenet";
    if (obj.value("nettype").toString() != wantNet) {
        qWarning() << "get_info verify: wrong nettype" << obj.value("nettype").toString()
                   << "(want" << wantNet << ")";
        return false;
    }

    const quint64 height = obj.value("height").toVariant().toULongLong();   // 3. height sanity
    if (height == 0) {
        qWarning() << "get_info verify: node reports height 0";
        return false;
    }
    quint64 knownHeight = 0;
    if (appData()->heights.contains(constants::networkType))
        knownHeight = static_cast<quint64>(appData()->heights[constants::networkType]);

    const quint64 peers = obj.value("outgoing_connections_count").toVariant().toULongLong()
                        + obj.value("incoming_connections_count").toVariant().toULongLong();
    const bool progressing = obj.value("busy_syncing").toBool() || obj.value("synchronized").toBool();

    if (knownHeight > 0) {
        const quint64 slack = 720;   // ~1 day of wownero blocks; live-but-lagging is still adoptable
        if (height + slack < knownHeight) {
            if (peers == 0 && !progressing) {   // 4. stuck-orphan signature: behind AND peerless AND idle
                qWarning() << "get_info verify: stale/stuck node — height" << height
                           << "<< known" << knownHeight << "with 0 peers, not syncing";
                return false;
            }
            qWarning() << "get_info verify: node behind (" << height << "/" << knownHeight
                       << ") but live; adopting and letting it catch up";
        }
    }

    if (peers == 0)   // soft: a freshly-spawned-but-orphaned node sits at 0 peers for minutes
        qWarning() << "get_info verify: adopting node with 0 peers (likely still bootstrapping)";

    qDebug() << "get_info verify: OK — nettype" << wantNet << "height" << height << "peers" << peers;
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
