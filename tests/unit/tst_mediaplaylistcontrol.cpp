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

#include "player.h"
#include "aalmediaplayerservice.h"
#include "aalmediaplaylistcontrol.h"
#include "tst_mediaplayerplugin.h"
#include "tst_mediaplaylistcontrol.h"

#include <QObject>
#include <QtTest/QtTest>

using namespace lomiri::MediaHub;

void tst_MediaPlaylistControl::initTestCase()
{
    m_service = new AalMediaPlayerService(this);
}

void tst_MediaPlaylistControl::cleanupTestCase()
{
}

void tst_MediaPlaylistControl::construction()
{
    QMediaPlayer player(this);
    QMediaPlaylist playlist(&player);

    player.setPlaylist(&playlist);
    m_playlistControl = m_service->requestControl(QMediaPlaylistControl_iid);
    m_mediaPlaylistControl = m_service->mediaPlaylistControl();
    QVERIFY(m_playlistControl != nullptr);
}

void tst_MediaPlaylistControl::setAndVerifyCurrentIndex()
{
    const int index = -1;
    playlistControl()->setCurrentIndex(index);
    QVERIFY(playlistControl()->currentIndex() == index);
}

QMediaPlaylistControl* tst_MediaPlaylistControl::playlistControl()
{
    return static_cast<QMediaPlaylistControl*>(m_mediaPlaylistControl);
}
