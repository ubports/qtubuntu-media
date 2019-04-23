/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
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

#include "aalmediaplayercontrol.h"
#include "aalmediaplayerservice.h"
#include "aalvideorenderercontrol.h"
#include "aalutility.h"

#include <media/media_compatibility_layer.h>

#include <QMediaPlaylist>
#include <QDebug>
#include <QApplication>
#include <QTimer>

AalMediaPlayerControl::AalMediaPlayerControl(AalMediaPlayerService *service, QObject *parent)
   : QMediaPlayerControl(parent),
    m_service(service),
    m_state(QMediaPlayer::StoppedState),
    m_status(QMediaPlayer::NoMedia),
    m_cachedDuration(0),
    m_applicationActive(true),
    m_allowSeek(true)
{
    m_cachedVolume = volume();

    QApplication::instance()->installEventFilter(this);

    connect(m_service, SIGNAL(playbackComplete()), this, SLOT(playbackComplete()));
}

AalMediaPlayerControl::~AalMediaPlayerControl()
{
    m_state = QMediaPlayer::StoppedState;
    m_status = QMediaPlayer::NoMedia;
    m_cachedVolume = 0;
}

QMediaPlayer::State AalMediaPlayerControl::state() const
{
    qDebug() << __PRETTY_FUNCTION__ << endl;
    return m_state;
}

QMediaPlayer::MediaStatus AalMediaPlayerControl::mediaStatus() const
{
    qDebug() << __PRETTY_FUNCTION__ << endl;
    return m_status;
}

QAudio::Role AalMediaPlayerControl::audioRole() const
{
    return m_service->audioRole();
}

void AalMediaPlayerControl::setAudioRole(QAudio::Role audioRole)
{
    qDebug() << __PRETTY_FUNCTION__;
    m_service->setAudioRole(audioRole);
}

qint64 AalMediaPlayerControl::duration() const
{
    const qint64 d = m_service->duration();
    m_cachedDuration = d;
    return d;
}

qint64 AalMediaPlayerControl::position() const
{
    return m_service->position();
}

void AalMediaPlayerControl::debounceSeek()
{
    m_allowSeek = true;
}

void AalMediaPlayerControl::setPosition(qint64 msec)
{
    // The actual debouncing
    if (!m_allowSeek)
        return;

    // Make sure we have a non-zero duration
    if (m_cachedDuration == 0)
        updateCachedDuration(duration());

    // Debounce seek requests with this single shot timer every 250 ms period
    QTimer::singleShot(250, this, SLOT(debounceSeek()));

    if (msec == m_cachedDuration)
        return playbackComplete();

    m_service->setPosition(msec);
    Q_EMIT positionChanged(msec);

    // Protect from another setPosition until the timer expires
    m_allowSeek = false;
}

int AalMediaPlayerControl::volume() const
{
    return m_service->getVolume();
}

void AalMediaPlayerControl::setVolume(int volume)
{
    m_cachedVolume = volume;
    m_service->setVolume(volume);
    Q_EMIT volumeChanged(m_cachedVolume);
}

bool AalMediaPlayerControl::isMuted() const
{
    return (volume() == 0);
}

void AalMediaPlayerControl::setMuted(bool muted)
{
    if (muted)
    {
        m_cachedVolume = volume();
        setVolume(0);
    }
    else
    {
        setVolume(m_cachedVolume);
    }

    Q_EMIT mutedChanged(muted);
}

int AalMediaPlayerControl::bufferStatus() const
{
    return m_service->bufferStatus();
}

bool AalMediaPlayerControl::isAudioAvailable() const
{
    return m_service->isAudioSource();
}

bool AalMediaPlayerControl::isVideoAvailable() const
{
    return m_service->isVideoSource();
}

bool AalMediaPlayerControl::isSeekable() const
{
    return true;
}

QMediaTimeRange AalMediaPlayerControl::availablePlaybackRanges() const
{
    // TODO: this will need to change once we can play networked sources
    return QMediaTimeRange(0, duration());
}

qreal AalMediaPlayerControl::playbackRate() const
{
    return 1.0;
}

void AalMediaPlayerControl::setPlaybackRate(qreal rate)
{
    Q_UNUSED(rate);
}

QMediaContent AalMediaPlayerControl::media() const
{
    return m_mediaContent;
}

const QIODevice* AalMediaPlayerControl::mediaStream() const
{
    // This is only valid if a stream was passed into setMedia()
    return NULL;
}

void AalMediaPlayerControl::setMedia(const QMediaContent& media, QIODevice* stream)
{
    Q_UNUSED(stream);
    qDebug() << __PRETTY_FUNCTION__ << endl;

    const QUrl mediaUrl =
            AalUtility::unescape(media);
    const lomiri::MediaHub::Player::Headers headers =
            AalUtility::extractHeaders(media.canonicalRequest());

    qDebug() << "setMedia() media: " << mediaUrl;
    qDebug() << "setMedia() headers empty: " << headers.empty();

    if (m_mediaContent == media) {
        qDebug() << "Same media as current";
        return;
    }

    m_mediaContent = media;

    QMediaPlayer::MediaStatus priorStatus = mediaStatus();

    if (!media.isNull())
        setMediaStatus(QMediaPlayer::LoadingMedia);

    // If there is no media this cleans up the play list
    m_service->setMedia(mediaUrl, headers);

    // This is important to do for QMediaPlaylist instances that
    // are set to loop. Without this, such a playlist will only
    // play once
    if (priorStatus == QMediaPlayer::EndOfMedia)
        stop();

    Q_EMIT mediaChanged(m_mediaContent);
}

void AalMediaPlayerControl::play()
{
    qDebug() << __PRETTY_FUNCTION__ << endl;
    m_service->play();

    // FIXME: Why are these setState needed? State is changed also when signals
    // from media-hub are received, which seems the right way to track it.
    setState(QMediaPlayer::PlayingState);
}

void AalMediaPlayerControl::pause()
{
    qDebug() << __PRETTY_FUNCTION__ << endl;
    m_service->pause();

    setState(QMediaPlayer::PausedState);
}

void AalMediaPlayerControl::stop()
{
    qDebug() << __PRETTY_FUNCTION__ << endl;
    m_service->stop();

    setState(QMediaPlayer::StoppedState);
}

void AalMediaPlayerControl::playbackComplete()
{
    qDebug() << __PRETTY_FUNCTION__ << endl;
    // The order of these lines is very important to keep music-app,
    // mediaplayer-app and the QMediaPlaylist loop cases all happy
    setMediaStatus(QMediaPlayer::EndOfMedia);
    // Stop the Player if no media has been loaded as result of EOS
    if (m_status == QMediaPlayer::EndOfMedia)
        stop();
    m_service->setPosition(0);
    Q_EMIT positionChanged(position());
    if (isVideoAvailable())
        m_service->resetVideoSink();
}

void AalMediaPlayerControl::mediaPrepared()
{
    Q_EMIT positionChanged(position());
}

void AalMediaPlayerControl::emitDurationChanged(qint64 duration)
{
    Q_EMIT durationChanged(duration);
}

void AalMediaPlayerControl::updateCachedDuration(qint64 duration)
{
    m_cachedDuration = duration;
    if (duration > 0)
        emitDurationChanged(duration);
}

void AalMediaPlayerControl::setMediaStatus(QMediaPlayer::MediaStatus status)
{
    if (status != m_status)
    {
        m_status = status;
        Q_EMIT mediaStatusChanged(m_status);
    }
}

void AalMediaPlayerControl::setState(QMediaPlayer::State state)
{
    // Because QMediaPlayer doesn't have a Ready state and GStreamer does, make sure the
    // stateChanged() signal is emited when either Stopped or Ready is passed in.
    bool do_state_changed = (state == QMediaPlayer::StoppedState && m_state == QMediaPlayer::StoppedState);
    if (state != m_state || do_state_changed)
    {
        m_state = state;
        Q_EMIT stateChanged(m_state);
    }
}
