// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "SplashDialog.h"
#include "ui_SplashDialog.h"

#include <QDir>
#include <QMovie>
#include <QRandomGenerator>

#include "utils/Icons.h"

SplashDialog::SplashDialog(QWidget *parent)
        : WindowModalDialog(parent)
        , ui(new Ui::SplashDialog)
{
    ui->setupUi(this);

    this->setWindowIcon(icons()->icon("appicon/64x64"));

    // wowlet: a random silly loading GIF (wow-lite-wallet tradition) instead of
    // a static key icon. Auto-discovers whatever loaders are bundled under
    // :/silly so the set can grow without touching this code; falls back to the
    // key pixmap if none are present.
    const QStringList loaders = QDir(":/silly").entryList(QStringList() << "*.gif", QDir::Files);
    if (!loaders.isEmpty()) {
        const QString pick = ":/silly/" + loaders.at(QRandomGenerator::global()->bounded(loaders.size()));
        auto *movie = new QMovie(pick, QByteArray(), this);
        movie->jumpToFrame(0);
        const QSize frame = movie->currentImage().size();
        const int h = 56;
        if (frame.height() > 0)
            movie->setScaledSize(QSize(frame.width() * h / frame.height(), h));
        ui->icon->setMovie(movie);
        movie->start();
    } else {
        QPixmap pixmap = QPixmap(":/assets/images/key.png");
        ui->icon->setPixmap(pixmap.scaledToWidth(32, Qt::SmoothTransformation));
    }

    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    this->adjustSize();
}

void SplashDialog::setMessage(const QString &message) {
    ui->label_message->setText(message);
}

void SplashDialog::setIcon(const QPixmap &icon) {
    ui->icon->setPixmap(icon.scaledToWidth(32, Qt::SmoothTransformation));
}

SplashDialog::~SplashDialog() = default;