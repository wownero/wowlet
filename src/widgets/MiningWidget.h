// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef WOWLET_MININGWIDGET_H
#define WOWLET_MININGWIDGET_H

#include <QWidget>

class Nodes;
#include <QTimer>
#include <QQuickWidget>

#include "libwalletqt/Wallet.h"

// Context object exposed to mining.qml as `mining`. The QML reads daemonMiningState and reacts to
// the hashrate/syncStatus/uptimeChanged/daemonOutput signals.
class MiningBackend : public QObject {
Q_OBJECT
    Q_PROPERTY(int daemonMiningState READ daemonMiningState NOTIFY daemonMiningStateChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(bool canMine READ canMine NOTIFY canMineChanged)
public:
    explicit MiningBackend(QObject *parent = nullptr) : QObject(parent) {}
    int daemonMiningState() const { return m_state; }
    void setState(int s) { if (m_state != s) { m_state = s; emit daemonMiningStateChanged(); } }
    bool reducedMotion() const { return m_reducedMotion; }
    void setReducedMotion(bool r) { if (m_reducedMotion != r) { m_reducedMotion = r; emit reducedMotionChanged(); } }
    // Solo mining is a start_mining RPC to whichever daemon the wallet is
    // connected to. Against a remote node that is either refused outright or —
    // worse, if the operator left the RPC open — it puts someone else's machine
    // to work on our address. It is only ever ours to ask of our own node.
    bool canMine() const { return m_canMine; }
    void setCanMine(bool c) { if (m_canMine != c) { m_canMine = c; emit canMineChanged(); } }

signals:
    void daemonMiningStateChanged();
    void reducedMotionChanged();
    void canMineChanged();
    void daemonOutput(const QString &line);
    void syncStatus(int from, int to, int pct);
    void uptimeChanged(const QString &uptime);
    void hashrate(const QString &hashrate);

private:
    int m_state = 0;   // 0:idle 1:startup 2:syncing 3:mining
    bool m_reducedMotion = false;
    bool m_canMine = false;
};

// wowlet: the pixel-art mining tab. Hosts mining.qml and wires its pick-axe to the wallet's solo
// miner (RandomWOW via the embedded node), feeding back hashrate + sync status.
class MiningWidget : public QWidget {
Q_OBJECT
public:
    explicit MiningWidget(Wallet *wallet, Nodes *nodes, QWidget *parent = nullptr);

private slots:
    void onStartMining();
    void onStopMining();
    void poll();

private:
    Wallet *m_wallet;
    Nodes *m_nodes;
    QQuickWidget *m_quickWidget;
    MiningBackend *m_backend;
    QTimer *m_timer;
    bool m_mining = false;          // last-seen mining state, to detect start transitions
    qint64 m_miningStartSecs = 0;   // when the current mining run started (for uptime)
};

#endif //WOWLET_MININGWIDGET_H
