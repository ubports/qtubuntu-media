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

#include <memory>

#include <QObject>
#include <QMediaPlayerControl>

class AalMediaPlayerService;
class AalMediaPlayerControl;

class tst_MediaPlayerPlugin : public QObject
{
    Q_OBJECT

    AalMediaPlayerService *m_service;
    AalMediaPlayerControl *m_mediaPlayerControl;
    QMediaControl *m_playerControl;
    QMediaControl *m_rendererControl;
    std::shared_ptr<lomiri::MediaHub::Player> m_player;
    std::shared_ptr<lomiri::MediaHub::Service> m_hubService;

private Q_SLOTS:
    // We want the setup to be run prior to every test case to
    // ensure correct test isolation, see http://qt-project.org/doc/qt-5/qtest-overview.html.
    void init();

    void tst_requestRelease();
    void tst_newMediaPlayer();
    void tst_setMedia();
    void tst_unescape();
    void tst_play();
    void tst_pause();
    void tst_stop();
    void tst_position();
    void tst_duration();
    void tst_isAudioSource();
    void tst_isVideoSource();
    void tst_volume();
};
