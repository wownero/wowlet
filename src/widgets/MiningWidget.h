// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef WOWLET_MININGWIDGET_H
#define WOWLET_MININGWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QQuickWidget>

#include "libwalletqt/Wallet.h"

// Context object exposed to mining.qml as `mining`. The QML reads daemonMiningState and reacts to
// the hashrate/syncStatus/uptimeChanged/daemonOutput signals.
class MiningBackend : public QObject {
Q_OBJECT
    Q_PROPERTY(int daemonMiningState READ daemonMiningState NOTIFY daemonMiningStateChanged)
public:
    explicit MiningBackend(QObject *parent = nullptr) : QObject(parent) {}
    int daemonMiningState() const { return m_state; }
    void setState(int s) { if (m_state != s) { m_state = s; emit daemonMiningStateChanged(); } }

signals:
    void daemonMiningStateChanged();
    void daemonOutput(const QString &line);
    void syncStatus(int from, int to, int pct);
    void uptimeChanged(const QString &uptime);
    void hashrate(const QString &hashrate);

private:
    int m_state = 0;   // 0:idle 1:startup 2:syncing 3:mining
};

// wowlet: the pixel-art mining tab. Hosts mining.qml and wires its pick-axe to the wallet's solo
// miner (RandomWOW via the embedded node), feeding back hashrate + sync status.
class MiningWidget : public QWidget {
Q_OBJECT
public:
    explicit MiningWidget(Wallet *wallet, QWidget *parent = nullptr);

private slots:
    void onStartMining();
    void onStopMining();
    void poll();

private:
    Wallet *m_wallet;
    QQuickWidget *m_quickWidget;
    MiningBackend *m_backend;
    QTimer *m_timer;
};

#endif //WOWLET_MININGWIDGET_H
