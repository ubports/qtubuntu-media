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

#include "aalmediaplaylistcontrol.h"
#include "aalmediaplaylistprovider.h"

#include <QEventLoop>
#include <QMediaPlaylist>

#include <QDebug>

// Uncomment for more verbose debugging to stdout/err
//#define VERBOSE_DEBUG

namespace media = lomiri::MediaHub;

QT_BEGIN_NAMESPACE

AalMediaPlaylistControl::AalMediaPlaylistControl(QObject *parent)
    : QMediaPlaylistControl(parent),
      m_playlistProvider(nullptr),
      m_currentIndex(0)
{
}

AalMediaPlaylistControl::~AalMediaPlaylistControl()
{
}

QMediaPlaylistProvider* AalMediaPlaylistControl::playlistProvider() const
{
    return m_playlistProvider;
}

bool AalMediaPlaylistControl::setPlaylistProvider(QMediaPlaylistProvider *playlist)
{
    m_playlistProvider = playlist;
    connect(playlist, SIGNAL(currentIndexChanged()), this, SLOT(onCurrentIndexChanged()));
    Q_EMIT playlistProviderChanged();
    return true;
}

int AalMediaPlaylistControl::currentIndex() const
{
    return m_currentIndex;
}

void AalMediaPlaylistControl::setCurrentIndex(int position)
{
    qDebug() << Q_FUNC_INFO;
    const auto mediaCount = m_playlistProvider->mediaCount();
    qDebug() << "position: " << position << ", mediaCount: " << mediaCount;

    if (position < 0 || position >= mediaCount)
        return;

    qDebug() << "Going to position: " << position;

    m_hubTrackList->goTo(position);
}

int AalMediaPlaylistControl::nextIndex(int steps) const
{
    const int x = m_currentIndex + steps;
    const int tracklistSize = m_playlistProvider->mediaCount();
#ifdef VERBOSE_DEBUG
    qDebug() << "m_currentIndex: " << m_currentIndex;
    qDebug() << "steps: " << steps;
    qDebug() << "tracklistSize: " << tracklistSize;
    qDebug() << "------------------------";
#endif
    if (x < tracklistSize)
        return x;
    else
        // algo: |(currentIndex + steps) - tracklist_size|
        return std::abs(x - tracklistSize);
}

int AalMediaPlaylistControl::previousIndex(int steps) const
{
    const int x  = m_currentIndex - steps;
    const int tracklistSize = m_playlistProvider->mediaCount();
    //const int reducedSteps = steps - ((steps / tracklistSize) * tracklistSize);
    // Calculate how many of x are in tracklistSize to reduce the calculation
    // to only wrap around the list one time
#ifdef VERBOSE_DEBUG
    const uint16_t m = (uint16_t)std::abs(x) / (uint16_t)tracklistSize; // 3
    qDebug() << "m_currentIndex: " << m_currentIndex;
    qDebug() << "steps: " << steps;
    qDebug() << "tracklistSize: " << tracklistSize;
    qDebug() << "x: " << x;
    qDebug() << "m: " << m;
    qDebug() << "------------------------";
#endif
    if (x >= 0)
        return x;
    else if (std::abs(x) > tracklistSize)
    {
        uint16_t i = m_currentIndex, stepCount = 0;
        bool doExit = false;
        while (!doExit)
        {
            if (i == 0)
                i = tracklistSize - 1;
            else
                --i;

            ++stepCount;
            if (stepCount == steps)
                doExit = true;
        }

        return i;
    }
    else
        return tracklistSize - std::abs(x);
}

void AalMediaPlaylistControl::next()
{
    qDebug() << Q_FUNC_INFO;

    m_hubPlayerSession->goToNext();
}

void AalMediaPlaylistControl::previous()
{
    qDebug() << Q_FUNC_INFO;

    m_hubPlayerSession->goToPrevious();
}

QMediaPlaylist::PlaybackMode AalMediaPlaylistControl::playbackMode() const
{
    QMediaPlaylist::PlaybackMode currentMode = QMediaPlaylist::Sequential;
    media::Player::LoopStatus loopStatus = media::Player::LoopStatus::LoopNone;
    loopStatus = m_hubPlayerSession->loopStatus();
    switch (loopStatus)
    {
        case media::Player::LoopStatus::LoopNone:
            currentMode = QMediaPlaylist::Sequential;
            break;
        case media::Player::LoopStatus::LoopTrack:
            currentMode = QMediaPlaylist::CurrentItemInLoop;
            break;
        case media::Player::LoopStatus::LoopPlaylist:
            currentMode = QMediaPlaylist::Loop;
            break;
        default:
            qWarning() << "Unknown loop status: " << loopStatus;
    }

    // Shuffle overrides loopStatus since in the media-hub API random is not part of loop_status
    // like it's all one for QMediaPlaylist::PlaybackMode
    if (m_hubPlayerSession->shuffle())
        currentMode = QMediaPlaylist::Random;

    return currentMode;
}

void AalMediaPlaylistControl::setPlaybackMode(QMediaPlaylist::PlaybackMode mode)
{
    qDebug() << Q_FUNC_INFO;
    switch (mode)
    {
        case QMediaPlaylist::CurrentItemOnce:
            qDebug() << "PlaybackMode: CurrentItemOnce";
            m_hubPlayerSession->setShuffle(false);
            qWarning() << "No media-hub equivalent for QMediaPlaylist::CurrentItemOnce";
            break;
        case QMediaPlaylist::CurrentItemInLoop:
            qDebug() << "PlaybackMode: CurrentItemInLoop";
            m_hubPlayerSession->setShuffle(false);
            m_hubPlayerSession->setLoopStatus(media::Player::LoopStatus::LoopTrack);
            break;
        case QMediaPlaylist::Sequential:
            qDebug() << "PlaybackMode: Sequential";
            m_hubPlayerSession->setShuffle(false);
            m_hubPlayerSession->setLoopStatus(media::Player::LoopStatus::LoopNone);
            break;
        case QMediaPlaylist::Loop:
            qDebug() << "PlaybackMode: Loop";
            m_hubPlayerSession->setShuffle(false);
            m_hubPlayerSession->setLoopStatus(media::Player::LoopStatus::LoopPlaylist);
            break;
        case QMediaPlaylist::Random:
            qDebug() << "PlaybackMode: Random";
            m_hubPlayerSession->setShuffle(true);
                // FIXME: Until pad.lv/1518157 (RandomAndLoop playbackMode) is
                // fixed set Random to be always looping due to pad.lv/1531296
            m_hubPlayerSession->setLoopStatus(media::Player::LoopStatus::LoopPlaylist);
            break;
        default:
            qWarning() << "Unknown playback mode: " << mode;
            m_hubPlayerSession->setShuffle(false);
    }

    Q_EMIT playbackModeChanged(mode);
}

void AalMediaPlaylistControl::setPlayerSession(const std::shared_ptr<lomiri::MediaHub::Player>& playerSession)
{
    m_hubPlayerSession = playerSession;
    aalMediaPlaylistProvider()->setPlayerSession(playerSession);

    m_hubTrackList = m_hubPlayerSession->trackList();
    if (!m_hubTrackList) {
        qWarning() << "FATAL: Failed to retrieve the current player session TrackList";
    }

    connect_signals();
}

void AalMediaPlaylistControl::onTrackChanged()
{
    m_currentIndex = m_hubTrackList->currentTrack();
    qDebug() << "m_currentIndex updated to: " << m_currentIndex;
    const QMediaContent content = playlistProvider()->media(m_currentIndex);
    Q_EMIT currentMediaChanged(content);
    Q_EMIT currentIndexChanged(m_currentIndex);
}

void AalMediaPlaylistControl::onMediaRemoved(int start, int end)
{
    Q_UNUSED(start);
    Q_UNUSED(end);

    // If the entire playlist is cleared, we need to reset the currentIndex
    // to just before the beginning of the list, otherwise if the user selects
    // a random track in the tracklist for a second time, track 0 is always
    // selected instead of the desired track index
    if (aalMediaPlaylistProvider()->mediaCount() == 0)
    {
        qDebug() << "Tracklist was cleared, resetting m_currentIndex to 0";
        m_currentIndex = 0;
    }
}

void AalMediaPlaylistControl::onRemoveTracks(int start, int end)
{
    // If the current track and everything after has been removed
    // then we need to set the currentIndex to 0 otherwise it is
    // left at the position it was before removing
    if (start <= m_currentIndex
            and m_currentIndex <= end
            and (end + 1) == m_playlistProvider->mediaCount()
            and start != 0)
    {
        m_currentIndex = 0;
        setCurrentIndex(0);

        // When repeat is off we have reached the end of playback so stop
        if (playbackMode() == QMediaPlaylist::Sequential)
        {
            qDebug() << "Repeat is off, so stopping playback";
            try {
                m_hubPlayerSession->stop();
            } catch (std::runtime_error &e) {
                qWarning() << "FATAL: Failed to stop playback:" << e.what();
            }
        }
    }
}

void AalMediaPlaylistControl::onCurrentIndexChanged()
{
    const int index = m_hubTrackList->currentTrack();
    if (index != m_currentIndex) {
        qDebug() << "Index changed to" << index;
        m_currentIndex = index;
        Q_EMIT currentIndexChanged(m_currentIndex);
    }
}

void AalMediaPlaylistControl::connect_signals()
{
    // Avoid duplicated subscriptions
    disconnect_signals();

    if (!m_hubTrackList) {
        qWarning() << "Can't connect to track list signals as it doesn't exist";
        return;
    }

    QObject::connect(m_hubTrackList, &media::TrackList::currentTrackChanged,
                     this, &AalMediaPlaylistControl::onTrackChanged);

    connect(aalMediaPlaylistProvider(), &AalMediaPlaylistProvider::mediaRemoved,
            this, &AalMediaPlaylistControl::onMediaRemoved);

    connect(aalMediaPlaylistProvider(), &AalMediaPlaylistProvider::removeTracks,
            this, &AalMediaPlaylistControl::onRemoveTracks);
}

void AalMediaPlaylistControl::disconnect_signals()
{
    QObject::disconnect(m_hubTrackList, nullptr, this, nullptr);
}

AalMediaPlaylistProvider* AalMediaPlaylistControl::aalMediaPlaylistProvider()
{
    return static_cast<AalMediaPlaylistProvider*>(m_playlistProvider);
}

QT_END_NAMESPACE
