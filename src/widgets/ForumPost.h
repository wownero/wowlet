// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2021, The Monero Project.

#ifndef WOWLET_FORUMPOST_H
#define WOWLET_FORUMPOST_H

#include <QString>

struct ForumPost {
    ForumPost(const QString &title, const QString &author, const QString &permalink, const QString date_added)
            : title(title), author(author), permalink(permalink), date_added(date_added){};

    QString title;
    QString author;
    QString permalink;
    QString date_added;
};

#endif //WOWLET_FORUMPOST_H
