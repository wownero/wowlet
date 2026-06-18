// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "AppData.h"

#include <QCoreApplication>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "config.h"
#include "WebsocketNotifier.h"
#include "Networking.h"

AppData::AppData(QObject *parent)
    : QObject(parent)
{
    this->initRestoreHeights();

    auto genesis_timestamp = this->restoreHeights[NetworkType::Type::MAINNET]->data.firstKey();
    this->txFiatHistory = new TxFiatHistory(genesis_timestamp, Config::defaultConfigDir().path(), this);

    connect(websocketNotifier()->websocketClient, &WebsocketClient::connectionEstablished, this->txFiatHistory, &TxFiatHistory::onUpdateDatabase);
    connect(this->txFiatHistory, &TxFiatHistory::requestYear, [](int year){
        QByteArray data = QString(R"({"cmd": "txFiatHistory", "data": {"year": %1}})").arg(year).toUtf8();
        websocketNotifier()->websocketClient->sendMsg(data);
    });

    // wownero: crypto prices come from neroswap (HTTP poll below), not feather's websocket (no WOW).
    // connect(websocketNotifier(), &WebsocketNotifier::CryptoRatesReceived, &this->prices, &Prices::cryptoPricesReceived);
    connect(websocketNotifier(), &WebsocketNotifier::FiatRatesReceived, &this->prices, &Prices::fiatPricesReceived);
    connect(websocketNotifier(), &WebsocketNotifier::TxFiatHistoryReceived, this->txFiatHistory, &TxFiatHistory::onWSData);
    connect(websocketNotifier(), &WebsocketNotifier::BlockHeightsReceived, this, &AppData::onBlockHeightsReceived);

    // wownero: poll WOW (+ BTC/XMR/etc.) USD prices from neroswap every 2 minutes.
    m_network = new Networking(this);
    auto fetchNeroswapPrices = [this] {
        QNetworkReply *reply = m_network->getJson(this, "https://prices.neroswap.com/v1/prices");
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError)
                return;
            QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            this->prices.neroswapPricesReceived(obj);
        });
    };
    fetchNeroswapPrices();
    auto *priceTimer = new QTimer(this);
    connect(priceTimer, &QTimer::timeout, this, fetchNeroswapPrices);
    priceTimer->start(120 * 1000);
}

QPointer<AppData> AppData::m_instance(nullptr);

void AppData::onBlockHeightsReceived(int mainnet, int stagenet) {
    this->heights[NetworkType::MAINNET] = mainnet;
    this->heights[NetworkType::STAGENET] = stagenet;
}

void AppData::initRestoreHeights() {
    restoreHeights[NetworkType::TESTNET] = new RestoreHeightLookup(NetworkType::TESTNET);
    restoreHeights[NetworkType::STAGENET] = RestoreHeightLookup::fromFile(":/assets/restore_heights_monero_stagenet.txt", NetworkType::STAGENET);
    restoreHeights[NetworkType::MAINNET] = RestoreHeightLookup::fromFile(":/assets/restore_heights_monero_mainnet.txt", NetworkType::MAINNET);
}

AppData* AppData::instance()
{
    if (!m_instance) {
        m_instance = new AppData(QCoreApplication::instance());
    }

    return m_instance;
}