// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef WOWLET_DAEMONMANAGER_H
#define WOWLET_DAEMONMANAGER_H

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>

// Manages an embedded wownerod node: unpacks the bundled binary, spawns it, monitors it, and stops
// it gracefully on exit. Modeled on Feather's TorManager (same bundle-binary + QProcess-lifecycle
// pattern), but wownerod owns an LMDB blockchain database, so shutdown is a graceful terminate (the
// daemon must flush), never a hard kill.
//
// wowlet runs this by default (the headline feature): the wallet connects to this local node at
// 127.0.0.1, and the embedded miner mines to it. Opt-out is a deliberate setting.
class DaemonManager : public QObject {
Q_OBJECT

public:
    explicit DaemonManager(QObject *parent = nullptr);
    static DaemonManager* instance();
    ~DaemonManager() override;

    void init();
    void start();
    void stop();
    bool unpackBins();

    bool isStarted() const { return m_started; }
    bool isAlreadyRunning() const { return m_alreadyRunning; }
    bool isSynced() const { return m_synced; }

    QString rpcAddress() const;   // "127.0.0.1:<rpcPort>" for the wallet to connect to

    QString daemonPath;
    QString daemonDir;            // where the unpacked wownerod binary lives
    QString blockchainDir;        // --data-dir (blockchain + p2pstate)
    QString rpcHost = "127.0.0.1";
    quint16 rpcPort = 34568;      // wownero default restricted-RPC port
    quint16 p2pPort = 34567;      // wownero default p2p port

    QStringList logs;
    QString errorMsg;

signals:
    void daemonStateChanged(bool running);
    void syncStatusChanged(quint64 height, quint64 targetHeight);
    void synced();
    void statusChanged(const QString &reason);
    void logsUpdated();

private slots:
    void onStateChanged(QProcess::ProcessState state);
    void handleProcessOutput();
    void handleProcessError(QProcess::ProcessError error);

private:
    bool shouldStartDaemon();
    void setErrorMessage(const QString &msg);

    QProcess *m_process;
    bool m_started = false;
    bool m_alreadyRunning = false;
    bool m_unpacked = false;
    bool m_synced = false;
    bool m_stopping = false;
    int m_restarts = 0;

    static QPointer<DaemonManager> m_instance;
};

inline DaemonManager* daemonManager()
{
    return DaemonManager::instance();
}

#endif //WOWLET_DAEMONMANAGER_H
