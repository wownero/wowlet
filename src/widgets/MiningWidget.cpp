// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "MiningWidget.h"

#include <algorithm>

#include <QVBoxLayout>
#include <QQmlContext>
#include <QQuickItem>
#include <QThread>

MiningWidget::MiningWidget(Wallet *wallet, QWidget *parent)
    : QWidget(parent)
    , m_wallet(wallet)
    , m_quickWidget(new QQuickWidget(this))
    , m_backend(new MiningBackend(this))
    , m_timer(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quickWidget);

    m_quickWidget->rootContext()->setContextProperty("mining", m_backend);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->setSource(QUrl("qrc:/qml/mining.qml"));

    if (auto *root = m_quickWidget->rootObject()) {
        connect(root, SIGNAL(startMining()), this, SLOT(onStartMining()));
        connect(root, SIGNAL(stopMining()), this, SLOT(onStopMining()));
    }

    connect(m_timer, &QTimer::timeout, this, &MiningWidget::poll);
    m_timer->start(2000);
}

void MiningWidget::onStartMining() {
    int threads = std::max(1, QThread::idealThreadCount() / 2);
    if (m_wallet->startMining(threads))
        m_backend->setState(3);
}

void MiningWidget::onStopMining() {
    m_wallet->stopMining();
    m_backend->setState(0);
}

void MiningWidget::poll() {
    bool mining = m_wallet->isMining();
    m_backend->setState(mining ? 3 : 0);

    if (mining) {
        double hr = m_wallet->miningHashRate();
        QString hrStr = hr >= 1000 ? QString("%1 kH/s").arg(hr / 1000.0, 0, 'f', 2)
                                   : QString("%1 H/s").arg(hr, 0, 'f', 0);
        emit m_backend->hashrate(hrStr);
    }

    quint64 from = m_wallet->blockChainHeight();
    quint64 to = m_wallet->daemonBlockChainTargetHeight();
    if (to > 0) {
        int pct = static_cast<int>(100.0 * static_cast<double>(from) / static_cast<double>(to));
        emit m_backend->syncStatus(static_cast<int>(from), static_cast<int>(to), pct);
    }
}
