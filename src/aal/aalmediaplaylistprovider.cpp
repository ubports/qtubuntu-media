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

#include "aalmediaplaylistprovider.h"
#include "aalutility.h"

#include <string>
#include <sstream>

#include <QDebug>

// Uncomment for more verbose debugging to stdout/err
//#define VERBOSE_DEBUG

namespace media = lomiri::MediaHub;

QT_BEGIN_NAMESPACE

AalMediaPlaylistProvider::AalMediaPlaylistProvider(QObject *parent):
    QMediaPlaylistProvider(parent)
{
}

AalMediaPlaylistProvider::~AalMediaPlaylistProvider()
{
    disconnect_signals();
}

int AalMediaPlaylistProvider::mediaCount() const
{
    if (!m_hubTrackList) {
        qWarning() << "Tracklist doesn't exist";
        return 0;
    }

    return m_hubTrackList->tracks().count();
}

QMediaContent AalMediaPlaylistProvider::media(int index) const
{
    int trackCount = mediaCount();
    if (index < 0 || index >= trackCount)
        return QMediaContent();

    const media::Track &track = m_hubTrackList->tracks()[index];
    return QMediaContent(track.uri());
}

bool AalMediaPlaylistProvider::isReadOnly() const
{
    if (!m_hubTrackList) {
        qWarning() << "Track list does not exist!";
        return false;
    }

    return !m_hubTrackList->canEditTracks();
}

bool AalMediaPlaylistProvider::addMedia(const QMediaContent &content)
{
    qDebug() << Q_FUNC_INFO;

    if (!m_hubTrackList) {
        qWarning() << "Track list does not exist so can't add a new track";
        return false;
    }

    const QUrl url = content.canonicalUrl();

    static const bool make_current = false;

    const int newIndex = mediaCount();
    Q_EMIT mediaAboutToBeInserted(newIndex, newIndex);
    qDebug() << "Adding track " << url;
    m_hubTrackList->addTrackWithUriAt(url, -1, make_current);

    return true;
}

bool AalMediaPlaylistProvider::addMedia(const QList<QMediaContent> &contentList)
{
    qDebug() << Q_FUNC_INFO << " num " << contentList.size();

    if (contentList.empty())
        return false;

    if (!m_hubTrackList) {
        qWarning() << "Track list does not exist so can't add new tracks";
        return false;
    }

    QVector<QUrl> uris;
    uris.reserve(contentList.count());
    for (const auto mediaContent : contentList) {
#ifdef VERBOSE_DEBUG
        qDebug() << "Adding track " << mediaContent.canonicalUrl().toString().toStdString();
#endif
        uris.append(mediaContent.canonicalUrl());
    }

    const int newIndex = mediaCount();
    Q_EMIT mediaAboutToBeInserted(newIndex, newIndex + contentList.size() - 1);
    m_hubTrackList->addTracksWithUriAt(uris, -1);

    return true;
}

bool AalMediaPlaylistProvider::insertMedia(int index, const QMediaContent &content)
{
    int trackCount = mediaCount();
    if (index < 0 or index >= trackCount) {
        qWarning() << Q_FUNC_INFO << "index is out of valid range";
        return false;
    }

    const QUrl url = content.canonicalUrl();
    qDebug() << "after_this_track:" << index;

    static const bool make_current = false;
    Q_EMIT mediaAboutToBeInserted(index, index);
    m_hubTrackList->addTrackWithUriAt(url, index, make_current);

    return true;
}

bool AalMediaPlaylistProvider::insertMedia(int index, const QList<QMediaContent> &content)
{
    (void) index;
    (void) content;

    qWarning() << Q_FUNC_INFO << " - Not yet implemented";

    return false;
}

bool AalMediaPlaylistProvider::moveMedia(int from, int to)
{
    int trackCount = mediaCount();
    if (from < 0 or from >= trackCount) {
        qWarning() << "Failed to moveMedia(), index 'from' is out of valid range";
        return false;
    }

    if (to < 0 or to >= trackCount) {
        qWarning() << "Failed to moveMedia(), index 'to' is out of valid range";
        return false;
    }

    if (from == to)
        return true;

    // This must be emitted before the move_track occurs or things in AalMediaPlaylistControl
    // such as m_currentId won't be accurate
    Q_EMIT startMoveTrack(from, to);

    qDebug() << "************ New track move:" << from << "to" << to;
    Q_EMIT mediaAboutToBeRemoved(from, from);
    int insertedIndex = to > from ? (to - 1) : to;
    Q_EMIT mediaAboutToBeInserted(insertedIndex, insertedIndex);

    m_hubTrackList->moveTrack(from, to);

    return true;
}

bool AalMediaPlaylistProvider::removeMedia(int pos)
{
    int trackCount = mediaCount();
    if (pos < 0 or pos >= trackCount) {
        qWarning() << Q_FUNC_INFO << "index is out of valid range";
        return false;
    }

    Q_EMIT mediaAboutToBeRemoved(pos, pos);
    m_hubTrackList->removeTrack(pos);

    return true;
}

bool AalMediaPlaylistProvider::removeMedia(int start, int end)
{
    // If we are removing everything then just use clear()
    if (start == 0 and (end + 1) == mediaCount())
    {
        return clear();
    }
    else
    {
        // Signal AalMediaPlaylistControl
        Q_EMIT removeTracks(start, end);

        // It's important that we remove tracks from end to start as removing tracks can
        // change the relative index value in track_index_lut relative to the Track::Id
        for (int i=end; i>=start; i--)
        {
            if (!removeMedia(i))
            {
                qWarning() << "Failed to remove the full range of tracks requested";
                return false;
            }
        }
    }

    return true;
}

bool AalMediaPlaylistProvider::clear()
{
    int trackCount = mediaCount();
    if (trackCount == 0) {
        qWarning() << "Track list doesn't exist so can't clear it!";
        return false;
    }

    Q_EMIT mediaAboutToBeRemoved(0, trackCount - 1);
    m_hubTrackList->reset();

    // We do not wait for the TrackListReset signal to empty the lut to
    // avoid sync problems.
    // TODO: Do the same in other calls if possible, as these calls are
    // considered to be synchronous, and the job can be considered finished
    // when the DBus call returns without error. If we need some information
    // from the signal we should block until it arrives.
    Q_EMIT mediaRemoved(0, trackCount - 1);

    return true;
}

bool AalMediaPlaylistProvider::isTrackEnd(int index)
{
    int trackCount = mediaCount();
    return index == trackCount - 1;
}

void AalMediaPlaylistProvider::setPlayerSession(const std::shared_ptr<lomiri::MediaHub::Player> &playerSession)
{
    m_hubPlayerSession = playerSession;

    m_hubTrackList.reset(new media::TrackList);
    m_hubPlayerSession->setTrackList(m_hubTrackList.get());

    /* Disconnect first to avoid duplicated calls */
    disconnect_signals();
    connect_signals();
}

void AalMediaPlaylistProvider::connect_signals()
{
    if (!m_hubTrackList) {
        qWarning() << "Can't connect to track list signals as it doesn't exist";
        return;
    }

    qDebug() << Q_FUNC_INFO;

    QObject::connect(m_hubTrackList.get(), &media::TrackList::tracksAdded,
                     this, [this](int start, int end)
    {
        qDebug() << "mediaInserted, first_index: " << start << " last_index: " << end;
        Q_EMIT mediaInserted(start, end);
        Q_EMIT currentIndexChanged();
    });

    QObject::connect(m_hubTrackList.get(), &media::TrackList::trackRemoved,
                     this, [this](int index)
    {
        qDebug() << "*** Removing track with index " << index;

        // Removed one track, so start and end are the same index values
        Q_EMIT mediaRemoved(index, index);
        Q_EMIT currentIndexChanged();
    });

    QObject::connect(m_hubTrackList.get(), &media::TrackList::trackMoved,
                     this, [this](int from, int to)
    {
        qDebug() << "Track moved from" << from << "to" << to;

        Q_EMIT mediaRemoved(from, from);
        int insertedIndex = to > from ? (to - 1) : to;
        Q_EMIT mediaInserted(insertedIndex, insertedIndex);
        Q_EMIT currentIndexChanged();
    });

    QObject::connect(m_hubTrackList.get(), &media::TrackList::trackListReset,
                     this, [this]()
    {
        qDebug() << "TrackListReset signal received";
    });
}

void AalMediaPlaylistProvider::disconnect_signals()
{
    qDebug() << Q_FUNC_INFO;

    QObject::disconnect(m_hubTrackList.get(), nullptr, this, nullptr);
}

QT_END_NAMESPACE
