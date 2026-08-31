// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef WOWLET_IDLEDOGE_H
#define WOWLET_IDLEDOGE_H

#include <QLabel>

class QMovie;

// wowlet: a small sitting doge that floats, click-through, in the bottom-right
// corner of its parent — the "most quiet" ambient idle for task screens (Send,
// Receive). Freezes to a still frame when "Prefer reduced motion" is set.
class IdleDoge : public QLabel {
Q_OBJECT
public:
    explicit IdleDoge(QWidget *parent, int height = 44);
protected:
    bool eventFilter(QObject *o, QEvent *e) override;
private:
    void reposition();
    QMovie *m_movie;
};

#endif //WOWLET_IDLEDOGE_H
