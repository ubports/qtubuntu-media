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

#ifndef TST_AALMEDIAPLAYLISTCONTROL_H
#define TST_AALMEDIAPLAYLISTCONTROL_H

#include "player.h"

#include <memory>

#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QObject>

class AalMediaPlayerService;
class AalMediaPlaylistControl;
class QMediaControl;
class QMediaPlayer;
class QMediaPlaylistControl;

class tst_MediaPlaylistControl : public QObject
{
    Q_OBJECT

    AalMediaPlayerService *m_service;
    AalMediaPlaylistControl *m_mediaPlaylistControl;
    QMediaControl *m_playlistControl;
    std::shared_ptr<lomiri::MediaHub::Player> m_player;
    std::shared_ptr<lomiri::MediaHub::Service> m_hubService;

Q_SIGNALS:

private Q_SLOTS:
    // We want the setup to be run prior to every test case to
    // ensure correct test isolation, see http://qt-project.org/doc/qt-5/qtest-overview.html.
    void initTestCase();
    void cleanupTestCase();

    void construction();
    void setAndVerifyCurrentIndex();

private:
    QMediaPlaylistControl* playlistControl();
};

#endif // TST_AALMEDIAPLAYLISTCONTROL_H
