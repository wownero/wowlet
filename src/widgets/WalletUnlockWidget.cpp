// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "WalletUnlockWidget.h"
#include "ui_WalletUnlockWidget.h"

#include <QKeyEvent>
#include <QPushButton>

#include "utils/Utils.h"
#include "libwalletqt/Wallet.h"
#include "widgets/SceneWidget.h"

WalletUnlockWidget::WalletUnlockWidget(QWidget *parent, Wallet *wallet)
        : QWidget(parent)
        , ui(new Ui::WalletUnlockWidget)
        , m_wallet(wallet)
{
    ui->setupUi(this);
    this->reset();

    // wowlet: the lock screen shows the full ambient scene above the unlock form
    // (this is the idle "screensaver" — the wallet auto-locks here on inactivity).
    // It freezes into a still when "Prefer reduced motion" is set.
    auto *scene = new SceneWidget(this);
    scene->setMinimumHeight(200);
    scene->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->verticalLayout->insertWidget(0, scene, 1);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setAutoDefault(true);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &WalletUnlockWidget::tryUnlock);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &WalletUnlockWidget::closeWallet);

    ui->frame_sync->hide();
    if (m_wallet) {
        connect(m_wallet, &Wallet::syncStatus, [this](quint64 height, quint64 target, bool daemonSync){
            ui->frame_sync->show();
            ui->label_sync->setText(Utils::formatSyncStatus(height, target, daemonSync));
        });
    }
}

void WalletUnlockWidget::setWalletName(const QString &walletName) {
    ui->label_fileName->setText(walletName);
}

void WalletUnlockWidget::reset() {
    ui->label_incorrectPassword->hide();
    ui->line_password->setText("");
    ui->line_password->setFocus();
}

void WalletUnlockWidget::incorrectPassword() {
    ui->label_incorrectPassword->show();
    ui->line_password->clear();
}

void WalletUnlockWidget::tryUnlock() {
    emit unlockWallet(ui->line_password->text());
}

void WalletUnlockWidget::keyPressEvent(QKeyEvent *e) {
    switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            ui->buttonBox->accepted();
            e->ignore();
            break;
        case Qt::Key_Escape:
            ui->buttonBox->rejected();
            e->ignore();
            break;
        default:
            e->ignore();
    }
}

WalletUnlockWidget::~WalletUnlockWidget() = default;