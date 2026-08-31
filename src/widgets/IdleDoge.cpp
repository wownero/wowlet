// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "IdleDoge.h"

#include <QEvent>
#include <QMovie>

#include "utils/config.h"

IdleDoge::IdleDoge(QWidget *parent, int height)
    : QLabel(parent)
    , m_movie(new QMovie(":/assets/images/dog_sitting.gif", QByteArray(), this))
{
    m_movie->jumpToFrame(0);
    const QSize frame = m_movie->currentImage().size();
    if (frame.height() > 0)
        m_movie->setScaledSize(QSize(frame.width() * height / frame.height(), height));
    this->setMovie(m_movie);
    this->setAttribute(Qt::WA_TransparentForMouseEvents);   // never block the form
    this->adjustSize();

    if (conf()->get(Config::reducedMotion).toBool())
        m_movie->jumpToFrame(0);   // still frame
    else
        m_movie->start();

    if (parent) {
        parent->installEventFilter(this);
        this->reposition();
        this->raise();
    }
}

void IdleDoge::reposition() {
    if (auto *p = parentWidget())
        this->move(p->width() - this->width() - 14, p->height() - this->height() - 14);
}

bool IdleDoge::eventFilter(QObject *o, QEvent *e) {
    if (o == parentWidget() && (e->type() == QEvent::Resize || e->type() == QEvent::Show)) {
        this->reposition();
        this->raise();
    }
    return QLabel::eventFilter(o, e);
}
