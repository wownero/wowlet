// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "utils/TorManager.h"

#include <QCoreApplication>
#include <QDirIterator>

#include "utils/ChildProcessGuard.h"   // wowlet: OS-level die-with-parent for bundled tor
#include "utils/config.h"
#include "utils/Utils.h"
#include "utils/os/tails.h"
#include "utils/os/whonix.h"

TorManager::TorManager(QObject *parent)
    : QObject(parent)
    , m_checkConnectionTimer(new QTimer(this))
    , m_process(new QProcess(this))
{
    connect(m_checkConnectionTimer, &QTimer::timeout, this, &TorManager::checkConnection);

    this->torDir = Config::defaultConfigDir().filePath("tor");
#if defined(TOR_INSTALLED)
    // When installed, use directory relative to application path.
    this->torDir = QDir(Utils::applicationPath()).filePath("tor");
#endif
    if (QString(FEATHER_TARGET_TRIPLET) == "arm64-apple-darwin" || QString(FEATHER_TARGET_TRIPLET) == "x86_64-apple-darwin") {
        QString featherBinaryPath = QCoreApplication::applicationDirPath();
        QDir appBinaryDir(featherBinaryPath);
        appBinaryDir.cd("..");
        this->torDir = appBinaryDir.filePath("bin");
    }

    this->torDataPath = Config::defaultConfigDir().filePath("tor/data");

    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TorManager::handleProcessOutput);
    connect(m_process, &QProcess::errorOccurred, this, &TorManager::handleProcessError);
    connect(m_process, &QProcess::stateChanged, this, &TorManager::stateChanged);

    ChildProcessGuard::installPreStart(m_process);   // wowlet: SIGKILL tor if wowlet dies (Unix)
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]{ this->stop(); });   // wowlet
}

QPointer<TorManager> TorManager::m_instance(nullptr);

void TorManager::init() {
    // wowlet: a live proxy/useLocalTor toggle re-runs init(). Reset failure/retry state so switching
    // system->bundled Tor after a prior failure (Linux unpack, or a FailedToStart that latched
    // m_stopRetries in handleProcessError) can actually start again instead of staying silently dead.
    m_restarts = 0;
    m_stopRetries = false;
    m_unpacked = false;   // re-evaluate the unpack on the next start()

    m_localTor = !shouldStartTorDaemon();

    auto state = m_process->state();
    if (m_localTor && (state == QProcess::ProcessState::Running || state == QProcess::ProcessState::Starting)) {
        m_stopRetries = true;   // wowlet: intentional kill (switching to system Tor); don't auto-restart it
        m_process->kill();
        m_started = false;
    }

    featherTorPort = conf()->get(Config::torManagedPort).toString().toUShort();
}

void TorManager::stop() {
    m_stopRetries = true;            // wowlet: intentional stop — suppress the stateChanged() auto-restart
    m_checkConnectionTimer->stop();  // wowlet: stop probing the connection after an intentional stop
    m_process->kill();
    m_started = false;
}

void TorManager::start() {
    m_checkConnectionTimer->start(5000);

    if (m_localTor) {
        this->checkConnection();
        return;
    }

    auto state = m_process->state();
    if (state == QProcess::ProcessState::Running || state == QProcess::ProcessState::Starting) {
        return;
    }

    if (Utils::portOpen(featherTorHost, featherTorPort)) {
        this->setErrorMessage(QString("Unable to start Tor on %1:%2. Port already in use.").arg(featherTorHost, QString::number(featherTorPort)));
        return;
    }

    QFile torFile{this->torPath};
    QString alternativeTorFile = QCoreApplication::applicationDirPath() + "/tor";
    if (!torFile.exists() && QFileInfo(alternativeTorFile).isFile()) {
        this->torPath = alternativeTorFile;
    }

    // wowlet: never hand QProcess a non-existent path (the Linux "empty folder" symptom). If the bundled
    // binary isn't on disk, degrade to local/external-Tor mode + surface an error instead of burning the
    // 4-retry budget on FailedToStart. With clearnet sync, node sync is unaffected; only broadcast-over-Tor
    // is lost, which DaemonManager tolerates via clearnet relay.
    if (!QFileInfo(this->torPath).isFile()) {
        this->setErrorMessage("Bundled Tor binary not found; using external/local Tor if available: " + this->torPath);
        m_localTor = true;
        this->checkConnection();
        return;
    }

    qDebug() << QString("Start process: %1").arg(this->torPath);

    m_restarts += 1;
    if (m_restarts > 4) {
        this->setErrorMessage("Tor failed to start: maximum retries exceeded");
        return;
    }

    QStringList arguments;

    arguments << "--ignore-missing-torrc";
    arguments << "--SocksPort" << QString("%1:%2").arg(featherTorHost, QString::number(featherTorPort));
    arguments << "--TruncateLogFile" << "1";
    arguments << "--DataDirectory" << this->torDataPath;
    arguments << "--Log" << "notice";
    arguments << "--pidfile" << QDir(this->torDataPath).filePath("tor.pid");

    qDebug() << QString("%1 %2").arg(this->torPath, arguments.join(" "));

    m_process->start(this->torPath, arguments);
    ChildProcessGuard::adoptAfterStart(m_process);   // wowlet: bind to kill-on-crash job (Windows)
    m_started = true;
}

void TorManager::checkConnection() {
    // We might not be able to connect to localhost if torsocks is used to start feather
    if (Utils::isTorsocks()) {
        this->setConnectionState(true);
    }

    else if (WhonixOS::detect()) {
        this->setConnectionState(true);
    }

    else if (TailsOS::detect()) {
        QStringList args = QStringList() << "--quiet" << "is-active" << "tails-tor-has-bootstrapped.target";
        int code = QProcess::execute("/bin/systemctl", args);

        this->setConnectionState(code == 0);
    }

    else if (conf()->get(Config::proxy).toInt() != Config::Proxy::Tor) {
        this->setConnectionState(false);
    }

    else if (m_localTor && !m_alreadyRunning) {
        QString host = conf()->get(Config::socks5Host).toString();
        quint16 port = conf()->get(Config::socks5Port).toString().toUShort();
        this->setConnectionState(Utils::portOpen(host, port));
    }

    else {
        this->setConnectionState(Utils::portOpen(featherTorHost, featherTorPort));
    }
}

void TorManager::setConnectionState(bool connected) {
    this->torConnected = connected;
    emit connectionStateChanged(connected);
}

void TorManager::stateChanged(QProcess::ProcessState state) {
    if (state == QProcess::ProcessState::Running) {
        this->setErrorMessage("");
        qWarning() << "Tor started, awaiting bootstrap";
    }
    else if (state == QProcess::ProcessState::NotRunning) {
        this->setConnectionState(false);

        if (m_stopRetries)
            return;

        QTimer::singleShot(1000, [=] {
            this->start();
        });
    }
}

void TorManager::handleProcessOutput() {
    QByteArray output = m_process->readAllStandardOutput();
    this->torLogs.append(Utils::barrayToString(output));
    emit logsUpdated();
    if(output.contains(QByteArray("Bootstrapped 100%"))) {
        qDebug() << "Tor OK";
        this->setConnectionState(true);
    }

    qDebug() << output;
}

void TorManager::handleProcessError(QProcess::ProcessError error) {
    if (error == QProcess::ProcessError::Crashed)
        qWarning() << "Tor crashed or killed";
    else if (error == QProcess::ProcessError::FailedToStart) {
        this->setErrorMessage("Tor binary failed to start: " + this->torPath);
        this->m_stopRetries = true;
    }
}

bool TorManager::unpackBins() {
    if (m_unpacked) {
        return true;
    }

    QString torBin = "tor";
#if defined(Q_OS_WIN)
   torBin += ".exe";
#endif

    this->torPath = QDir(this->torDir).filePath(torBin);

#if defined(TOR_INSTALLED)
    // We don't need to unpack if Tor was installed using the installer
    return true;
#endif

    if (QString(FEATHER_TARGET_TRIPLET) == "arm64-apple-darwin" || QString(FEATHER_TARGET_TRIPLET) == "x86_64-apple-darwin") {
        return true;
    }

    // wowlet: QFile::copy never creates parent dirs; without this the copy loop can silently no-op on
    // a fresh profile, leaving an empty tor/ dir and "tor failed to start" (Lucas/Fedora). main.cpp
    // also mkpaths this, but make unpackBins() self-sufficient. Mirrors DaemonManager::unpackBins().
    QDir().mkpath(this->torDir);

    // wowlet: unpack when the binary is MISSING, not only when a version compare says "newer". Overwrite
    // only when the embedded build is a VALID newer version (getVersion() of a non-existent file returns
    // an invalid SemanticVersion, so it must not gate the first unpack).
    bool present = QFileInfo(this->torPath).isFile();
    SemanticVersion embeddedVersion = SemanticVersion::fromString(QString(TOR_VERSION));
    SemanticVersion filesystemVersion = present ? this->getVersion(this->torPath) : SemanticVersion();
    qDebug() << QString("Tor versions: embedded %1, filesystem %2").arg(embeddedVersion.toString(), filesystemVersion.toString());
    bool needsOverwrite = present && SemanticVersion::isValid(filesystemVersion)
                          && (embeddedVersion > filesystemVersion);

    if (!present || needsOverwrite) {
        if (needsOverwrite) {
            qInfo() << "Embedded Tor is newer, overwriting.";
            QFile::setPermissions(this->torPath, QFile::ReadOther | QFile::WriteOther);
            QFile::remove(this->torPath);
        }
        QDirIterator it(":/assets/tor", QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString assetFile = it.next();
            QString filePath = QDir(this->torDir).filePath(QFileInfo(assetFile).fileName());
            QFile f(assetFile);
            f.copy(filePath);
            f.close();
        }
        qInfo() << "Wrote Tor binaries to:" << this->torDir;
    }

#if defined(Q_OS_UNIX)
    // wowlet: only chmod if the binary actually landed.
    if (QFileInfo(this->torPath).isFile()) {
        QFile tor(this->torPath);
        tor.setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther
                           | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
    }
#endif

    // wowlet: do NOT report success if nothing landed — return false so shouldStartTorDaemon() falls
    // back to --use-local-tor cleanly (see :297-303) instead of start()ing a missing binary.
    if (!QFileInfo(this->torPath).isFile()) {
        qWarning() << "Tor was not unpacked (no embedded Tor, or copy failed):" << this->torPath;
        return false;
    }

    m_unpacked = true;
    return true;
}

bool TorManager::isLocalTor() {
    return m_localTor;
}

bool TorManager::isStarted() {
    return m_started;
}

bool TorManager::isAlreadyRunning() {
    return m_alreadyRunning;
}

bool TorManager::shouldStartTorDaemon() {
    QString torHost = conf()->get(Config::socks5Host).toString();
    quint16 torPort = conf()->get(Config::socks5Port).toString().toUShort();
    QString torHostPort = QString("%1:%2").arg(torHost, QString::number(torPort));
    m_alreadyRunning = false;

    // Don't start a Tor daemon if Feather is run with Torsocks
    if (Utils::isTorsocks()) {
        return false;
    }

    // Don't start a Tor daemon on privacy OSes
    if (TailsOS::detect() || WhonixOS::detect()) {
        return false;
    }

    // Don't start a Tor daemon if we don't have one
#if !defined(HAS_TOR_BIN) && !defined(TOR_INSTALLED)
    qWarning() << "Feather built without embedded Tor. Assuming --use-local-tor";
    return false;
#endif

    // Don't start a Tor daemon if our proxy config isn't set to Tor
    if (conf()->get(Config::proxy).toInt() != Config::Proxy::Tor) {
        return false;
    }

    // Don't start a Tor daemon if --use-local-tor is specified
    if (conf()->get(Config::useLocalTor).toBool()) {
        return false;
    }

    if (m_started) {
        return true;
    }

    // Don't start a Tor daemon if one is already running
    if (Utils::portOpen(torHost, torPort)) {
        return false;
    }

    bool unpacked = this->unpackBins();
    if (!unpacked) {
        // Don't try to start a Tor daemon if unpacking failed
        qWarning() << "Error unpacking embedded Tor. Assuming --use-local-tor";
        this->setErrorMessage("Error unpacking embedded Tor. Assuming --use-local-tor");
        return false;
    }

    // Tor daemon (or other service) is already running on our port (19450)

    if (Utils::portOpen(featherTorHost, featherTorPort)) {
        m_alreadyRunning = true;
        return false;
    }

    return true;
}

SemanticVersion TorManager::getVersion(const QString &fileName) {
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(fileName, QStringList() << "--version");   // wowlet: use the passed path, not this->torPath
    process.waitForFinished(-1);
    QString output = process.readAllStandardOutput();

    if(output.isEmpty()) {
        qWarning() << "Could not grab Tor version";
        return SemanticVersion();
    }

    return SemanticVersion::fromString(output);
}

void TorManager::setErrorMessage(const QString &msg) {
    this->errorMsg = msg;
    emit statusChanged(msg);
}

TorManager* TorManager::instance()
{
    if (!m_instance) {
        m_instance = new TorManager(QCoreApplication::instance());
    }

    return m_instance;
}

TorManager::~TorManager() = default;
