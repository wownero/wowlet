// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef FEATHER_CONSTANTS_H
#define FEATHER_CONSTANTS_H

#include <QString>

#include "networktype.h"

namespace constants
{
    extern NetworkType::Type networkType; // TODO: not really a const

    // coin constants
    const std::string coinName = "wownero";
    const qreal cdiv = 1e11;            // wownero: 11 decimals (monero is 12)
    const quint32 mixin = 21;           // wownero: ring size 22 (mixin 21); monero is 10
    const quint64 kdfRounds = 1;

    const QString seedLanguage = "English"; // todo: move me
}

#endif //FEATHER_CONSTANTS_H
