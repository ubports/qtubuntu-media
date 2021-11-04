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

#ifndef AALMEDIAPLAYLISTPROVIDER_H
#define AALMEDIAPLAYLISTPROVIDER_H

#include <private/qmediaplaylistprovider_p.h>

#include <MediaHub/Player>
#include <MediaHub/Track>
#include <MediaHub/TrackList>

#include <QScopedPointer>
#include <atomic>
#include <memory>

QT_BEGIN_NAMESPACE

class AalMediaPlaylistControl;

class AalMediaPlaylistProvider : public QMediaPlaylistProvider
{
Q_OBJECT
public:
    friend class AalMediaPlaylistControl;

    AalMediaPlaylistProvider(QObject *parent=0);
    ~AalMediaPlaylistProvider();

    int mediaCount() const;
    QMediaContent media(int index) const;

    bool isReadOnly() const;

    bool addMedia(const QMediaContent &content);
    bool addMedia(const QList<QMediaContent> &contentList);
    bool insertMedia(int index, const QMediaContent &content);
    bool insertMedia(int index, const QList<QMediaContent> &content);
    bool moveMedia(int from, int to);
    bool removeMedia(int pos);
    bool removeMedia(int start, int end);
    bool clear();

    bool isTrackEnd(int index/* TODO const ContainerTrackLut::const_iterator &it*/);

Q_SIGNALS:
    void startMoveTrack(int from, int to);
    void currentIndexChanged();
    // Emitted when removing a range of tracks less than mediaCount()
    // so that AalMediaPlaylistControl can take appropriate action
    void removeTracks(int start, int end);

private:
    void setPlayerSession(const std::shared_ptr<lomiri::MediaHub::Player> &playerSession);
    void connect_signals();
    void disconnect_signals();
    std::shared_ptr<lomiri::MediaHub::Player> m_hubPlayerSession;
    QScopedPointer<lomiri::MediaHub::TrackList> m_hubTrackList;
};

QT_END_NAMESPACE

#endif
