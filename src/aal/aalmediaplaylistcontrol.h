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

#ifndef AALMEDIAPLAYLISTCONTROL_H
#define AALMEDIAPLAYLISTCONTROL_H

#include <qmediaplaylistcontrol_p.h>

#include <core/media/player.h>
#include <core/media/track_list.h>

#include <core/connection.h>

#include <memory>

QT_BEGIN_NAMESPACE

class QMediaPlaylistProvider;
class AalMediaPlaylistProvider;

class AalMediaPlaylistControl : public QMediaPlaylistControl
{
Q_OBJECT
public:
    AalMediaPlaylistControl(QObject *parent=0);
    ~AalMediaPlaylistControl();

    QMediaPlaylistProvider* playlistProvider() const;
    bool setPlaylistProvider(QMediaPlaylistProvider *playlist);

    int currentIndex() const;
    void setCurrentIndex(int position);
    int nextIndex(int steps) const;
    int previousIndex(int steps) const;

    void next();
    void previous();

    QMediaPlaylist::PlaybackMode playbackMode() const;
    void setPlaybackMode(QMediaPlaylist::PlaybackMode mode);

    void setPlayerSession(const std::shared_ptr<core::ubuntu::media::Player>& playerSession);

Q_SIGNALS:
    void playlistProviderChanged();
    void currentIndexChanged(int position);
    void currentMediaChanged(const QMediaContent&);
    void playbackModeChanged(QMediaPlaylist::PlaybackMode mode);

private Q_SLOTS:
    void onApplicationStateChanged(Qt::ApplicationState state);
    void onTrackChanged(const core::ubuntu::media::Track::Id& id);

private:
    void connect_signals();
    void disconnect_signals();
    inline AalMediaPlaylistProvider* aalMediaPlaylistProvider();

    std::shared_ptr<core::ubuntu::media::Player> m_hubPlayerSession;
    std::shared_ptr<core::ubuntu::media::TrackList> m_hubTrackList;
    QMediaPlaylistProvider *m_playlistProvider;

    int m_currentIndex;

    core::Connection m_trackChangedConnection;
};

QT_END_NAMESPACE

#endif
