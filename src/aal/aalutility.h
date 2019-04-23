/*
 * Copyright (C) 2015 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AALUTILITY_H
#define AALUTILITY_H

#include <QMediaContent>
#include <QUrl>

#include <string>

#include <MediaHub/Player>

struct AalUtility
{
    static QUrl unescape(const QMediaContent &media);
    static std::string unescape_str(const QMediaContent &media);
    static lomiri::MediaHub::Player::Headers extractHeaders(const QNetworkRequest& request);

    AalUtility() = delete;
    AalUtility(const AalUtility&) = delete;
    AalUtility& operator=(const AalUtility&) = delete;
};

#endif // AALUTILITY_H
