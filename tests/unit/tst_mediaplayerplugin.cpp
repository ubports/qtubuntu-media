/*
 * Copyright (C) 2013 Canonical, Ltd.
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
#include "aalutility.h"
#include "tst_mediaplayerplugin.h"
#include "tst_mediaplaylistcontrol.h"

#include <memory>

#include <QVideoRendererControl>
#include <QtTest/QtTest>

#define private public
#include "aalmediaplayercontrol.h"

using namespace std;
using namespace lomiri::MediaHub;

void tst_MediaPlayerPlugin::init()
{
    m_service = new AalMediaPlayerService(this);
    m_playerControl = m_service->requestControl(QMediaPlayerControl_iid);
    m_mediaPlayerControl = m_service->mediaPlayerControl();
    QVERIFY(m_playerControl != NULL);
    m_rendererControl = m_service->requestControl(QVideoRendererControl_iid);
    QVERIFY(m_rendererControl != NULL);
}

void tst_MediaPlayerPlugin::tst_requestRelease()
{
    QMediaControl *mpControl = NULL;
    QMediaControl *rendererControl = NULL;

    // Request a new reference to the media player control and verify that
    // it's the same pointer as the original reference.
    mpControl = m_service->requestControl(QMediaPlayerControl_iid);
    QVERIFY(mpControl == m_playerControl);
    rendererControl = m_service->requestControl(QVideoRendererControl_iid);
    QVERIFY(rendererControl == m_rendererControl);

    // Now release the reference and make sure that the original media player
    // control reference still exists.
    m_service->releaseControl(mpControl);
    QVERIFY(mpControl == m_playerControl);
    m_service->releaseControl(rendererControl);
    QVERIFY(rendererControl == m_rendererControl);
}

void tst_MediaPlayerPlugin::tst_newMediaPlayer()
{
    bool ret = m_service->newMediaPlayer();
    QVERIFY(ret == true);
}

void tst_MediaPlayerPlugin::tst_setMedia()
{
    QUrl url("blah.mp4");
    QMediaContent media(url);
    m_mediaPlayerControl->setMedia(media, NULL);
    QVERIFY(m_mediaPlayerControl->mediaStatus() == QMediaPlayer::LoadingMedia);
    QVERIFY(m_mediaPlayerControl->media() == media);
}

void tst_MediaPlayerPlugin::tst_unescape()
{
    QString uri_str("file:///home/phablet/Videos/sintel[REC].mp4");
    QUrl uri("file:///home/phablet/Videos/sintel%5BREC%5D.mp4");
    QVERIFY(AalUtility::unescape(QMediaContent(uri)).toString() == uri_str);

    uri_str = "https://www.youtube.com/watch?v=ESua4zGyo2Y&webm=1";
    uri = "https://www.youtube.com/watch?v=ESua4zGyo2Y&webm=1";
    QVERIFY(AalUtility::unescape(QMediaContent(uri)).toString() == uri_str);
}

void tst_MediaPlayerPlugin::tst_play()
{
    m_mediaPlayerControl->play();
    QVERIFY(m_mediaPlayerControl->mediaStatus() == QMediaPlayer::LoadedMedia);
}

void tst_MediaPlayerPlugin::tst_pause()
{
    m_mediaPlayerControl->pause();
    QVERIFY(m_mediaPlayerControl->state() == QMediaPlayer::PausedState);
}

void tst_MediaPlayerPlugin::tst_stop()
{
    m_mediaPlayerControl->stop();
    QVERIFY(m_mediaPlayerControl->state() == QMediaPlayer::StoppedState);
}

void tst_MediaPlayerPlugin::tst_position()
{
    m_mediaPlayerControl->setPosition(1e6);
    qDebug() << "position: " << m_mediaPlayerControl->position();
    QCOMPARE(m_mediaPlayerControl->position(), 1e3);
}

void tst_MediaPlayerPlugin::tst_duration()
{
    QCOMPARE(m_mediaPlayerControl->duration(), 1);
}

void tst_MediaPlayerPlugin::tst_isAudioSource()
{
    QVERIFY(m_mediaPlayerControl->isAudioAvailable());
}

void tst_MediaPlayerPlugin::tst_isVideoSource()
{
    QVERIFY(m_mediaPlayerControl->isVideoAvailable());
}

void tst_MediaPlayerPlugin::tst_volume()
{
    QVERIFY(m_mediaPlayerControl->volume() == 1);
}

int main(int argc, char **argv)
{
    // Create a GUI-less unit test standalone app
    QCoreApplication app(argc, argv);
    tst_MediaPlayerPlugin mpp;
    tst_MediaPlaylistControl mpc;
    return (QTest::qExec(&mpp, argc, argv) && QTest::qExec(&mpc, argc, argv));
}
