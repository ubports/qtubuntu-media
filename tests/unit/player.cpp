/*
 * Copyright © 2021 UBports Foundation.
 *
 * Contact: Alberto Mardegan <mardy@users.sourceforge.net>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "player.h"

#include <QDebug>
#include <MediaHub/VideoSink>

using namespace lomiri::MediaHub;

namespace lomiri {
namespace MediaHub {

class NullVideoSink: public VideoSink
{
    Q_OBJECT

public:
    NullVideoSink(): VideoSink(nullptr) {}

    bool swapBuffers() override { return true; }
};


class PlayerPrivate
{
    Q_DECLARE_PUBLIC(Player)

public:
    PlayerPrivate(Player *q);
    ~PlayerPrivate();

private:
    bool m_canPlay = false;
    bool m_canPause = false;
    bool m_canSeek = false;
    bool m_canGoPrevious = false;
    bool m_canGoNext = false;
    bool m_isVideoSource = true;
    bool m_isAudioSource = true;
    bool m_shuffle = false;
    Player::Volume m_volume = 1.0;
    Player::Orientation m_orientation = Player::Rotate0;
    Track::MetaData m_metaData;

    Player::PlaybackRate m_playbackRate = 1.0;
    Player::PlaybackRate m_minimumPlaybackRate = 1.0;
    Player::PlaybackRate m_maximumPlaybackRate = 1.0;

    quint64 m_position = 0;
    quint64 m_duration = 1e6;

    Player::PlaybackStatus m_playbackStatus = Player::Null;

    Player::AudioStreamRole m_audioStreamRole = Player::MultimediaRole;
    QString m_uuid;
    TrackList *m_trackList = nullptr;
    NullVideoSink m_videoSink;
    Player *q_ptr;
};

} // namespace MediaHub
} // namespace lomiri

PlayerPrivate::PlayerPrivate(Player *q):
    q_ptr(q)
{
}

PlayerPrivate::~PlayerPrivate()
{
}

Player::Player(QObject *parent):
    QObject(parent),
    d_ptr(new PlayerPrivate(this))
{
}

Player::~Player() = default;

QString Player::uuid() const
{
    Q_D(const Player);
    return d->m_uuid;
}

void Player::setTrackList(TrackList *trackList)
{
    Q_D(Player);
    d->m_trackList = trackList;
}

TrackList *Player::trackList() const
{
    Q_D(const Player);
    return d->m_trackList;
}

VideoSink &Player::createGLTextureVideoSink(uint32_t textureId)
{
    Q_D(Player);
    return d->m_videoSink;
}

void Player::openUri(const QUrl &uri, const Headers &headers)
{
}

void Player::goToNext()
{
}

void Player::goToPrevious()
{
}

void Player::play()
{
}

void Player::pause()
{
}

void Player::stop()
{
}

void Player::seekTo(uint64_t microseconds)
{
    Q_D(Player);
    d->m_position = microseconds;
}

bool Player::canPlay() const
{
    Q_D(const Player);
    return d->m_canPlay;
}

bool Player::canPause() const
{
    Q_D(const Player);
    return d->m_canPause;
}

bool Player::canSeek() const
{
    Q_D(const Player);
    return d->m_canSeek;
}

bool Player::canGoPrevious() const
{
    Q_D(const Player);
    return d->m_canGoPrevious;
}

bool Player::canGoNext() const
{
    Q_D(const Player);
    return d->m_canGoNext;
}

bool Player::isVideoSource() const
{
    Q_D(const Player);
    return d->m_isVideoSource;
}

bool Player::isAudioSource() const
{
    Q_D(const Player);
    return d->m_isAudioSource;
}

Player::PlaybackStatus Player::playbackStatus() const
{
    Q_D(const Player);
    return d->m_playbackStatus;
}

void Player::setPlaybackRate(PlaybackRate rate)
{
}

Player::PlaybackRate Player::playbackRate() const
{
    Q_D(const Player);
    return d->m_playbackRate;
}

void Player::setShuffle(bool shuffle)
{
}

bool Player::shuffle() const
{
    Q_D(const Player);
    return d->m_shuffle;
}

void Player::setVolume(Volume volume)
{
}

Player::Volume Player::volume() const
{
    Q_D(const Player);
    return d->m_volume;
}

Track::MetaData Player::metaDataForCurrentTrack() const
{
    Q_D(const Player);
    return d->m_metaData;
}

Player::PlaybackRate Player::minimumPlaybackRate() const
{
    Q_D(const Player);
    return d->m_minimumPlaybackRate;
}

Player::PlaybackRate Player::maximumPlaybackRate() const
{
    Q_D(const Player);
    return d->m_maximumPlaybackRate;
}

quint64 Player::position() const
{
    Q_D(const Player);
    return d->m_position;
}

quint64 Player::duration() const
{
    Q_D(const Player);
    return d->m_duration;
}

Player::Orientation Player::orientation() const
{
    Q_D(const Player);
    return d->m_orientation;
}

void Player::setLoopStatus(LoopStatus loopStatus)
{
}

Player::LoopStatus Player::loopStatus() const
{
    Q_D(const Player);
    return Player::LoopNone;
}

void Player::setAudioStreamRole(AudioStreamRole role)
{
}

Player::AudioStreamRole Player::audioStreamRole() const
{
    Q_D(const Player);
    return d->m_audioStreamRole;
}

#include "player.moc"
