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

#include "tst_mediaplaylist.h"
#include "../../../src/aal/aalutility.h"

#include <thread>
#include <unistd.h>

#include <QMediaPlayer>
#include <QMediaPlaylist>

#include <QtTest/QtTest>

void tst_MediaPlaylist::initTestCase()
{
}

void tst_MediaPlaylist::cleanupTestCase()
{
}

void tst_MediaPlaylist::init()
{
    // NOTE: This sleep is currently needed in order to give media-hub a bit of time
    // between our different tests to cleanup and come back in a state where it can
    // respond to our requests.
    sleep(1);
}

void tst_MediaPlaylist::constructDestroyRepeat()
{
    for (int i=0; i<25; i++)
    {
        QMediaPlayer *player = new QMediaPlayer;
        QMediaPlaylist *playlist = new QMediaPlaylist;
        player->setPlaylist(playlist);

        delete playlist;
        delete player;
    }
}

void tst_MediaPlaylist::addTwoTracksAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    playlist->addMedia(QUrl(QFINDTESTDATA("testdata/testfile.ogg")));
    playlist->addMedia(QUrl(QFINDTESTDATA("testdata/testfile.mp4")));

    QCOMPARE(playlist->mediaCount(), 2);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::insertTracksAtPositionAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QElapsedTimer timer;
    timer.start();
    playlist->addMedia(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    waitTrackInserted(playlist);
    playlist->addMedia(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    waitTrackInserted(playlist);
    playlist->addMedia(QUrl("file://" + QFINDTESTDATA("testdata/testfile1.ogg")));
    waitTrackInserted(playlist);
    playlist->addMedia(QUrl("file://" + QFINDTESTDATA("testdata/testfile2.ogg")));
    waitTrackInserted(playlist);
    playlist->addMedia(QUrl("file://" + QFINDTESTDATA("testdata/testfile3.ogg")));
    waitTrackInserted(playlist);
    qDebug() << "** addMedia took" << timer.elapsed() << "milliseconds";

    QCOMPARE(playlist->mediaCount(), 5);

    const QMediaContent insertedTrack(QUrl("file://" + QFINDTESTDATA("testdata/testfile4.ogg")));
    playlist->insertMedia(2, insertedTrack);
    waitTrackInserted(playlist);

    qDebug() << "playlist->media(2):" << playlist->media(2).canonicalUrl();
    qDebug() << "insertedTrack:" << insertedTrack.canonicalUrl();
    QCOMPARE(playlist->media(2), insertedTrack);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::moveTrackAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QElapsedTimer timer;
    timer.start();
    QList<QMediaContent> content;
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile1.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile2.ogg")));
    const QMediaContent newLastTrack(QUrl("file://" + QFINDTESTDATA("testdata/testfile3.ogg")));
    content.push_back(newLastTrack);
    const QMediaContent insertedTrack(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    content.push_back(insertedTrack);
    playlist->addMedia(content);
    waitTrackInserted(playlist);
    qDebug() << "** addMedia took" << timer.elapsed() << "milliseconds";

    QCOMPARE(playlist->mediaCount(), 6);

    connectSignal(playlist, Signals::MediaRemoved);
    connectSignal(playlist, Signals::MediaInserted);
    playlist->moveMedia(5, 2);

    Q_ASSERT(m_signalsDeque.pop_front() == Signals::MediaRemoved);
    Q_ASSERT(m_signalsDeque.pop_front() == Signals::MediaInserted);
    QCOMPARE(playlist->mediaCount(), 6);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::movePlayingTrackAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QElapsedTimer timer;
    timer.start();
    QList<QMediaContent> content;
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile1.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile2.ogg")));
    const QMediaContent newLastTrack(QUrl("file://" + QFINDTESTDATA("testdata/testfile3.ogg")));
    content.push_back(newLastTrack);
    const QMediaContent insertedTrack(QUrl("file://" + QFINDTESTDATA("testdata/Ubuntu.ogg")));
    content.push_back(insertedTrack);
    playlist->addMedia(content);
    waitTrackInserted(playlist);
    qDebug() << "** addMedia took" << timer.elapsed() << "milliseconds";

    playlist->setCurrentIndex(5);
    waitCurrentIndexChange(playlist);
    QCOMPARE(playlist->currentIndex(), 5);

    player->play();

    QCOMPARE(playlist->mediaCount(), 6);

    sleep(2);

    connectSignal(playlist, Signals::MediaRemoved);
    connectSignal(playlist, Signals::MediaInserted);
    playlist->moveMedia(5, 2);

    Q_ASSERT(m_signalsDeque.pop_front() == Signals::MediaRemoved);
    qDebug() << "Verifying the presence of MediaInserted signal in deque";
    Q_ASSERT(m_signalsDeque.pop_front() == Signals::MediaInserted);
    QCOMPARE(playlist->mediaCount(), 6);

    waitCurrentIndexChange(playlist);

    QCOMPARE(player->state(), QMediaPlayer::State::PlayingState);
    QCOMPARE(playlist->currentIndex(), 2);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::addListOfTracksAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content;
    content.push_back(QUrl(QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl(QFINDTESTDATA("testdata/testfile.mp4")));

    playlist->addMedia(content);

    QCOMPARE(playlist->mediaCount(), 2);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::addLargeListOfTracksAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    // Total number of tracks added will be iterations * 5
    const uint16_t iterations = 20;
    QElapsedTimer timer;
    timer.start();
    for (uint16_t i=0; i<iterations; i++)
    {
        playlist->addMedia(QUrl(QFINDTESTDATA("testdata/testfile.mp4")));
        waitTrackInserted(playlist);
        playlist->addMedia(QUrl(QFINDTESTDATA("testdata/testfile.ogg")));
        waitTrackInserted(playlist);
        playlist->addMedia(QUrl(QFINDTESTDATA("testdata/testfile1.ogg")));
        waitTrackInserted(playlist);
        playlist->addMedia(QUrl(QFINDTESTDATA("testdata/testfile2.ogg")));
        waitTrackInserted(playlist);
        playlist->addMedia(QUrl(QFINDTESTDATA("testdata/testfile3.ogg")));
        waitTrackInserted(playlist);
    }
    qDebug() << "** addMedia loop took" << timer.elapsed() << "milliseconds";

    QCOMPARE(playlist->mediaCount(), iterations * 5);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::addLargeListOfTracksAtOnceAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content;
    int i;
    for (i=0; i<20; i++)
    {
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile.mp4")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile1.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile2.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile3.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile4.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile1.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile2.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile3.ogg")));
        content.push_back(QUrl(QFINDTESTDATA("testdata/testfile4.ogg")));
    }

    QElapsedTimer timer;
    timer.start();
    playlist->addMedia(content);
    qDebug() << "** addMedia(QList) took" << timer.elapsed() << "milliseconds";

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), i * 10);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::addTwoListsOfTracksAtOnceAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content1;
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile.mp4")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile1.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile2.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile3.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile4.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile1.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile2.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile3.ogg")));
    content1.push_back(QUrl(QFINDTESTDATA("testdata/testfile4.ogg")));

    QList<QMediaContent> content2;
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile4.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile3.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile2.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile1.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile.mp4")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile1.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile2.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile3.ogg")));
    content2.push_back(QUrl(QFINDTESTDATA("testdata/testfile4.ogg")));

    QElapsedTimer timer;
    timer.start();
    playlist->addMedia(content1);
    qDebug() << "** First list addMedia(QList) took" << timer.elapsed() << "milliseconds";

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 10);

    timer.invalidate();
    timer.start();
    playlist->addMedia(content2);
    qDebug() << "** Second list addMedia(QList) took" << timer.elapsed() << "milliseconds";

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 20);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::goToNextTrack()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    const QUrl audio(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    const QUrl video(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    qDebug() << "audio URL: " << audio.toString();
    qDebug() << "video URL: " << video.toString();
    playlist->addMedia(audio);
    playlist->addMedia(video);

    QCOMPARE(playlist->mediaCount(), 2);

    player->play();

    QCoreApplication::processEvents();

    const QUrl audioToVerify(playlist->currentMedia().canonicalUrl());
    QCOMPARE(audioToVerify, audio);

    playlist->next();

    QCoreApplication::processEvents();

    const QUrl videoToVerify(playlist->currentMedia().canonicalUrl());
    QCOMPARE(videoToVerify, video);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::goToPreviousTrack()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    const QUrl audio1(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    const QUrl audio2(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    playlist->addMedia(audio1);
    playlist->addMedia(audio2);

    QCOMPARE(playlist->mediaCount(), 2);
    playlist->setCurrentIndex(1);

    player->play();

    QCoreApplication::processEvents();

    const QUrl audio2ToVerify(playlist->currentMedia().canonicalUrl());
    QCOMPARE(audio2ToVerify, audio2);

    playlist->previous();

    QCoreApplication::processEvents();

    const QUrl audio1ToVerify(playlist->currentMedia().canonicalUrl());
    QCOMPARE(audio2ToVerify, audio1);
    QCOMPARE(playlist->currentIndex(), 0);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::verifyMedia()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    const QUrl audio(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    const QUrl video(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    qDebug() << "audio URL: " << audio.toString();
    qDebug() << "video URL: " << video.toString();
    playlist->addMedia(audio);
    playlist->addMedia(video);

    QCOMPARE(playlist->mediaCount(), 2);

    const QUrl audioToVerify(playlist->media(0).canonicalUrl());
    QCOMPARE(audioToVerify, audio);

    const QUrl videoToVerify(playlist->media(1).canonicalUrl());
    QCOMPARE(videoToVerify, video);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::removeTrackAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    playlist->addMedia(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    const QUrl video(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    playlist->addMedia(video);

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 2);

    playlist->removeMedia(0);

    QCOMPARE(playlist->mediaCount(), 1);

    const QUrl videoToVerify(playlist->media(0).canonicalUrl());
    QCOMPARE(videoToVerify, video);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::removeCurrentNonPlayingTrackAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content1;
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile1.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile2.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile3.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile4.ogg")));
    playlist->addMedia(content1);
    const QUrl track(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    playlist->addMedia(track);

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 6);

    playlist->setCurrentIndex(2);
    // Wait for the currentMediaChanged signal to be emited
    waitTrackChange(playlist);
    // We should not be automatically playing
    QCOMPARE(player->state(), QMediaPlayer::State::StoppedState);
    QCOMPARE(playlist->currentIndex(), 2);

    playlist->removeMedia(2);

    // We should still not be playing
    QCOMPARE(player->state(), QMediaPlayer::State::StoppedState);

    QCOMPARE(playlist->mediaCount(), 5);

    const QUrl trackToVerify(playlist->media(4).canonicalUrl());
    QCOMPARE(trackToVerify, track);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::removeCurrentPlayingTrackAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content1;
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile1.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile2.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile3.ogg")));
    content1.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile4.ogg")));
    playlist->addMedia(content1);
    const QUrl track(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    playlist->addMedia(track);

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 6);

    player->play();

    playlist->setCurrentIndex(2);
    // Wait for the currentMediaChanged signal to be emited
    waitTrackChange(playlist);
    // We be  playing
    QCOMPARE(player->state(), QMediaPlayer::State::PlayingState);
    QCOMPARE(playlist->currentIndex(), 2);

    playlist->removeMedia(2);

    // We should still be playing
    QCOMPARE(player->state(), QMediaPlayer::State::PlayingState);

    QCOMPARE(playlist->mediaCount(), 5);

    const QUrl trackToVerify(playlist->media(4).canonicalUrl());
    QCOMPARE(trackToVerify, track);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::removeLastCurrentPlayingTrackAndVerify()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    const QUrl track(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    playlist->addMedia(track);

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 1);

    player->play();

    playlist->setCurrentIndex(0);
    qDebug() << "Waiting for playback status to change to playing";
    // Wait for the currentMediaChanged signal to be emited
    waitTrackChange(playlist);
    // We be  playing
    QCOMPARE(player->state(), QMediaPlayer::State::PlayingState);

    qDebug() << "Removing track index 0";
    playlist->removeMedia(0);

    waitTrackRemoved(playlist);
    // We should no longer be playing
    QCOMPARE(player->state(), QMediaPlayer::State::StoppedState);

    QCOMPARE(playlist->mediaCount(), 0);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::verifyCurrentIndex()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content;
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    playlist->addMedia(content);

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 3);

    qDebug() << "Setting current index to be 1";
    playlist->setCurrentIndex(1);

    // Wait for the currentMediaChanged signal to be emited
    waitTrackChange(playlist);

    qDebug() << "Checking if current index is 1";
    QCOMPARE(playlist->currentIndex(), 1);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::verifyNextIndex()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content;
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    playlist->addMedia(content);

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 6);

    QCOMPARE(playlist->nextIndex(1), 1);
    QCOMPARE(playlist->nextIndex(4), 4);
    QCOMPARE(playlist->nextIndex(6), 0);
    QCOMPARE(playlist->nextIndex(7), 1);
    QCOMPARE(playlist->nextIndex(11), 5);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::verifyPreviousIndex()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QList<QMediaContent> content;
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));
    playlist->addMedia(content);

    waitTrackInserted(playlist);
    QCOMPARE(playlist->mediaCount(), 6);

    QCOMPARE(playlist->previousIndex(1), 5);
    QCOMPARE(playlist->previousIndex(4), 2);
    QCOMPARE(playlist->previousIndex(6), 0);
    QCOMPARE(playlist->previousIndex(11), 1);
    QCOMPARE(playlist->previousIndex(21), 3);
    QCOMPARE(playlist->previousIndex(19), 5);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::verifyPlaybackModeCurrentItemInLoop()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    connectSignal(playlist, Signals::MediaInserted);

    QList<QMediaContent> content;
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    playlist->addMedia(content);

    // Wait until the first track is set as the current one
    waitTrackChange(playlist);
    Q_ASSERT(m_signalsDeque.pop_front() == Signals::MediaInserted);
    QCOMPARE(playlist->mediaCount(), 2);

    waitPlaybackModeChange(playlist, [playlist]()
        {
            playlist->setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
        });

    player->play();

    // Wait for the currentMediaChanged signal to be emited
    waitTrackChange(playlist);

    QCOMPARE(playlist->currentIndex(), 0);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::verifyPlaybackModeSequential()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    connectSignal(playlist, Signals::MediaInserted);

    QList<QMediaContent> content;
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    content.push_back(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    playlist->addMedia(content);

    // Wait until the first track is set as the current one
    waitTrackChange(playlist);
    Q_ASSERT(m_signalsDeque.pop_front() == Signals::MediaInserted);
    QCOMPARE(playlist->mediaCount(), 2);

    waitPlaybackModeChange(playlist, [playlist]()
        {
            playlist->setPlaybackMode(QMediaPlaylist::Sequential);
        });

    player->play();

    // Wait until the second track is selected
    waitTrackChange(playlist);

    QCOMPARE(playlist->currentIndex(), 1);

    delete playlist;
    delete player;
}

void tst_MediaPlaylist::playReusePlayTrackList()
{
    QMediaPlayer *player = new QMediaPlayer;
    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    const QUrl audio(QUrl("file://" + QFINDTESTDATA("testdata/testfile.ogg")));
    const QUrl video(QUrl("file://" + QFINDTESTDATA("testdata/testfile.mp4")));

    for (int i = 0; i < 5; ++i) {
        playlist->addMedia(audio);
        waitTrackInserted(playlist);
        playlist->addMedia(video);
        waitTrackInserted(playlist);
        playlist->addMedia(audio);
        waitTrackInserted(playlist);
        QCOMPARE(playlist->mediaCount(), 3);

        player->play();

        const QUrl audioToVerify(playlist->currentMedia().canonicalUrl());
        QCOMPARE(audioToVerify, audio);

        player->stop();

        connectSignal(playlist, Signals::MediaRemoved);
        playlist->clear();

        Q_ASSERT(m_signalsDeque.pop_front() == Signals::MediaRemoved);

        QCOMPARE(playlist->mediaCount(), 0);
    }

    delete playlist;
    delete player;
}

template<typename R>
void tst_MediaPlaylist::wait_for_signal(std::future<R> const& f)
{
    while (!is_ready<R>(f))
    {
        // Make sure we don't block the main QEventLoop, which
        // would hinder receiving the currentMediaChanged event above
        QCoreApplication::processEvents();
        std::this_thread::yield();
    }
}

void tst_MediaPlaylist::waitTrackChange(QMediaPlaylist *playlist)
{
    QMediaContent current_media;
    std::promise<QMediaContent> promise;
    std::future<QMediaContent> future{promise.get_future()};

    QMetaObject::Connection c = connect(playlist, &QMediaPlaylist::currentMediaChanged,
        [&](const QMediaContent& content)
        {
            qDebug() << "currentMediaChanged to: " << content.canonicalUrl().toString();
            current_media = content;
            promise.set_value(current_media);
            // Make sure the promise is not fulfilled twice
            QObject::disconnect(c);
        });

    wait_for_signal(future);
}

void tst_MediaPlaylist::waitTrackInserted(QMediaPlaylist *playlist)
{
    int index = 0;
    std::promise<int> promise;
    std::future<int> future{promise.get_future()};

    QMetaObject::Connection c = connect(playlist, &QMediaPlaylist::mediaInserted,
        [&](int start, int end)
        {
            qDebug() << "mediaInserted start: " << start << ", end: " << end;
            index = end;
            promise.set_value(index);
            // Make sure the promise is not fulfilled twice
            QObject::disconnect(c);
        });

    wait_for_signal(future);
}

void tst_MediaPlaylist::waitTrackRemoved(QMediaPlaylist *playlist)
{
    int index = 0;
    std::promise<int> promise;
    std::future<int> future{promise.get_future()};

    QMetaObject::Connection c = connect(playlist, &QMediaPlaylist::mediaRemoved,
        [&](int start, int end)
        {
            qDebug() << "mediaRemoved start: " << start << ", end: " << end;
            index = end;
            promise.set_value(index);
            // Make sure the promise is not fulfilled twice
            QObject::disconnect(c);
        });

    wait_for_signal(future);
}

void tst_MediaPlaylist::waitPlaybackModeChange(QMediaPlaylist *playlist,
                                               const std::function<void()>& action)
{
    QMediaPlaylist::PlaybackMode current_mode;
    std::promise<QMediaPlaylist::PlaybackMode> promise;
    std::future<QMediaPlaylist::PlaybackMode> future{promise.get_future()};

    QMetaObject::Connection c = connect(playlist, &QMediaPlaylist::playbackModeChanged,
        [&](QMediaPlaylist::PlaybackMode mode)
        {
            qDebug() << "playbackModeChanged to: " << mode;
            current_mode = mode;
            promise.set_value(current_mode);
            // Make sure the promise is not fulfilled twice
            QObject::disconnect(c);
        });

    action();

    wait_for_signal(future);
}

void tst_MediaPlaylist::waitCurrentIndexChange(QMediaPlaylist *playlist)
{
    int index = 0;
    std::promise<int> promise;
    std::future<int> future{promise.get_future()};

    QMetaObject::Connection c = connect(playlist, &QMediaPlaylist::currentIndexChanged,
        [&](int i)
        {
            qDebug() << "currentIndexChanged index: " << i;
            index = i;
            promise.set_value(index);
            // Make sure the promise is not fulfilled twice
            QObject::disconnect(c);
        });

    wait_for_signal(future);
}

void tst_MediaPlaylist::connectSignal(QMediaPlaylist *playlist, Signals signal)
{
    switch (signal)
    {
        case Signals::Unknown:
            break;
        case Signals::CurrentMediaChanged:
        {
            connect(playlist, &QMediaPlaylist::currentMediaChanged, [&](const QMediaContent& content)
            {
                (void) content;
                qDebug() << "Pushing CurrentMediaChanged onto m_signalsDeque";
                m_signalsDeque.push_back(signal);
            });
            break;
        }
        case Signals::MediaInserted:
        {
            connect(playlist, &QMediaPlaylist::mediaInserted, [&](int start, int end)
            {
                (void) start;
                (void) end;
                qDebug() << "Pushing MediaInserted onto m_signalsDeque";
                m_signalsDeque.push_back(signal);
            });
            break;
        }
        case Signals::MediaRemoved:
        {
            connect(playlist, &QMediaPlaylist::mediaRemoved, [&](int index)
            {
                (void) index;
                qDebug() << "Pushing MediaRemoved onto m_signalsDeque";
                m_signalsDeque.push_back(signal);
            });
            break;
        }
        default:
            qWarning() << "Unknown signal type, can't add to queue:" << signal;
    }
}

QTEST_GUILESS_MAIN(tst_MediaPlaylist)
