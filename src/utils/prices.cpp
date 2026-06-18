// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "prices.h"

#include <QJsonArray>
#include <QJsonObject>

#include "config.h"
#include "constants.h"

Prices::Prices(QObject *parent)
    : QObject(parent)
{
}

void Prices::cryptoPricesReceived(const QJsonArray &data) {
    this->markets.clear();

    for (const auto &entry : data) {
        QJsonObject obj = entry.toObject();
        marketStruct ms;
        ms.symbol = obj.value("symbol").toString();
        ms.image = obj.value("image").toString();
        ms.name = obj.value("name").toString();
        ms.price_usd = obj.value("current_price").toDouble();
        ms.price_usd_change_pct_24h = obj.value("price_change_percentage_24h").toDouble();
        if (ms.price_usd <= 0)
            continue;

        this->markets.insert(ms.symbol.toUpper(), ms);
    }

    emit cryptoPricesUpdated();
}

void Prices::neroswapPricesReceived(const QJsonObject &data) {
    // neroswap returns {"rates": {"WOW": usd, "WOW": usd, ...}}. It is wowlet's sole crypto-price
    // source (feather's websocket server does not carry WOW), so repopulate markets from it.
    QJsonObject ratesData = data.value("rates").toObject();
    if (ratesData.isEmpty())
        return;

    this->markets.clear();
    for (const QString &sym : ratesData.keys()) {
        double usd = ratesData.value(sym).toDouble();
        if (usd <= 0)
            continue;
        marketStruct ms;
        ms.symbol = sym.toUpper();
        ms.name = sym;
        ms.price_usd = usd;
        ms.price_usd_change_pct_24h = 0.0;  // neroswap does not provide 24h change
        this->markets.insert(sym.toUpper(), ms);
    }

    emit cryptoPricesUpdated();
}

void Prices::fiatPricesReceived(const QJsonObject &data) {
    QJsonObject ratesData = data.value("rates").toObject();
    for (const auto &currency : ratesData.keys()) {
        this->rates.insert(currency, ratesData.value(currency).toDouble());
    }
    emit fiatPricesUpdated();
}

double Prices::convert(QString symbolFrom, QString symbolTo, double amount) {
    if (symbolFrom == symbolTo)
        return amount;
    if (amount <= 0.0)
        return 0.0;

    symbolFrom = symbolFrom.toUpper();
    symbolTo = symbolTo.toUpper();

    double usdPrice;
    if (this->markets.contains(symbolFrom)) {
        usdPrice = this->markets[symbolFrom].price_usd * amount;
    }
    else if (this->rates.contains(symbolFrom)) {
        if (symbolFrom == "USD") {
            usdPrice = amount;
        } else {
            usdPrice = amount / this->rates[symbolFrom];
        }
    }
    else {
        return 0.0;
    }

    if (symbolTo == "USD")
        return usdPrice;

    if (this->markets.contains(symbolTo))
        return usdPrice / this->markets[symbolTo].price_usd;
    else if (this->rates.contains(symbolTo))
        return usdPrice * this->rates[symbolTo];

    return 0.0;
}

QString Prices::atomicUnitsToPreferredFiatString(quint64 amount, bool wrapInParens) {
    QString fiatCurrency = conf()->get(Config::preferredFiatCurrency).toString();
    double fiatAmount = convert("WOW", fiatCurrency, amount / constants::cdiv);
    QString currencyString = Utils::amountToCurrencyString(fiatAmount, fiatCurrency);

    if (wrapInParens) {
        return QString("(%1)").arg(currencyString);
    }
    return currencyString;
}
