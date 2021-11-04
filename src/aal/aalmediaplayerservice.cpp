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
#include "aalmediaplaylistcontrol.h"
#include "aalmediaplaylistprovider.h"
#include "aalaudiorolecontrol.h"
#include "aalutility.h"

#include <qmediaplaylistcontrol_p.h>

#include <MediaHub/Error>
#include <MediaHub/TrackList>

#include <errno.h>

#include <QAbstractVideoSurface>
#include <QGuiApplication>
#include <QTimerEvent>
#include <QThread>

#include <qtubuntu_media_signals.h>

// Uncomment to re-enable media-hub Player session detach/reattach functionality.
// It is not clear at all whether we should do this or not, as detaching
// is probably something that should be done when the app finishes, not when it
// simply moves to the background.
//#define DO_PLAYER_ATTACH_DETACH

// Defined in aalvideorenderercontrol.h
#ifdef MEASURE_PERFORMANCE
#include <QDateTime>
#endif

#include <QDebug>

namespace media = lomiri::MediaHub;

namespace
{
enum {
    OK          = 0,
    NO_ERROR    = 0,
    BAD_VALUE   = -EINVAL,
};
}

AalMediaPlayerService::AalMediaPlayerService(QObject *parent)
    :
     QMediaService(parent),
     m_hubPlayerSession(nullptr),
     m_mediaPlayerControl(nullptr),
     m_videoOutput(nullptr),
     m_mediaPlaylistControl(nullptr),
     m_mediaPlaylistProvider(nullptr),
     m_audioRoleControl(nullptr),
     m_videoOutputReady(false),
     m_firstPlayback(true),
     m_cachedDuration(0),
     m_mediaPlaylist(nullptr),
     m_bufferPercent(0),
     m_doReattachSession(false)
#ifdef MEASURE_PERFORMANCE
      , m_lastFrameDecodeStart(0)
      , m_currentFrameDecodeStart(0)
      , m_avgCount(0)
      , m_frameDecodeAvg(0)
#endif
{
    constructNewPlayerService();
    // Note: this must be in the constructor and not part of constructNewPlayerService()
    // or it won't successfully connect to the signal
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &AalMediaPlayerService::onApplicationStateChanged);
}

AalMediaPlayerService::~AalMediaPlayerService()
{
    if (m_audioRoleControl)
        deleteAudioRoleControl();

    if (m_videoOutput)
        deleteVideoRendererControl();

    if (m_mediaPlaylistControl)
        deletePlaylistControl();

    if (m_mediaPlayerControl)
        deleteMediaPlayerControl();

    if (m_hubPlayerSession)
        destroyPlayerSession();
}

void AalMediaPlayerService::constructNewPlayerService()
{
    if (!newMediaPlayer())
        qWarning() << "Failed to create a new media player backend. Video playback will not function." << endl;

    if (m_hubPlayerSession == nullptr)
    {
        qWarning() << "Could not finish contructing new AalMediaPlayerService instance since m_hubPlayerSession is NULL";
        return;
    }

    createMediaPlayerControl();
    createVideoRendererControl();
    createAudioRoleControl();

    QObject::connect(m_hubPlayerSession.get(), &media::Player::playbackStatusChanged,
                     this, &AalMediaPlayerService::onPlaybackStatusChanged);

    QObject::connect(m_hubPlayerSession.get(), &media::Player::bufferingChanged, this,
                [this](int bufferingPercent) {
                    m_bufferPercent = bufferingPercent;
                    onBufferingChanged();
                });

    QObject::connect(m_hubPlayerSession.get(), &media::Player::errorOccurred,
                     this, &AalMediaPlayerService::onError);
}

QMediaControl *AalMediaPlayerService::requestControl(const char *name)
{
    if (qstrcmp(name, QMediaPlayerControl_iid) == 0)
    {
        if (not m_mediaPlayerControl)
            createMediaPlayerControl();

        return m_mediaPlayerControl;
    }

    if (qstrcmp(name, QVideoRendererControl_iid) == 0)
    {
        if (not m_videoOutput)
            createVideoRendererControl();

        return m_videoOutput;
    }

    if (qstrcmp(name, QMediaPlaylistControl_iid) == 0)
    {
        if (not m_mediaPlaylistControl)
            createPlaylistControl();

        // Pass on the media-hub Player object to the playlist control
        if (m_hubPlayerSession)
            m_mediaPlaylistControl->setPlayerSession(m_hubPlayerSession);

        return m_mediaPlaylistControl;
    }

    if (qstrcmp(name, QAudioRoleControl_iid) == 0)
    {
        if (not m_audioRoleControl)
            createAudioRoleControl();

        return m_audioRoleControl;
    }

    return nullptr;
}

void AalMediaPlayerService::releaseControl(QMediaControl *control)
{
    if (control == m_videoOutput)
        deleteVideoRendererControl();
}

bool AalMediaPlayerService::newMediaPlayer()
{
    // Only one player session needed
    if (m_hubPlayerSession != nullptr)
        return true;

    m_hubPlayerSession.reset(new media::Player());

    // Get the player session UUID so we can suspend/restore our session when the ApplicationState
    // changes
    m_sessionUuid = m_hubPlayerSession->uuid();
    return true;
}

lomiri::MediaHub::VideoSink &AalMediaPlayerService::createVideoSink(uint32_t texture_id)
{
    m_videoOutputReady = true;
    return m_hubPlayerSession->createGLTextureVideoSink(texture_id);
}

void AalMediaPlayerService::resetVideoSink()
{
    qDebug() << Q_FUNC_INFO;
    Q_EMIT SharedSignal::instance()->sinkReset();
    m_firstPlayback = false;
    if (m_videoOutput != NULL)
        m_videoOutput->playbackComplete();
}

QAudio::Role AalMediaPlayerService::audioRole() const
{
    if (m_audioRoleControl == nullptr)
    {
        qWarning() << "Failed to get audio role, m_audioRoleControl is NULL";
        return QAudio::UnknownRole;
    }

    return m_audioRoleControl->audioRole();
}

void AalMediaPlayerService::setAudioRole(QAudio::Role audioRole)
{
    if (m_audioRoleControl == nullptr)
    {
        qWarning() << "Failed to set audio role, m_audioRoleControl is NULL";
        return;
    }

    m_audioRoleControl->setAudioRole(audioRole);
}

void AalMediaPlayerService::setMediaPlaylist(const QMediaPlaylist &playlist)
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot set playlist without a valid media-hub player session";
        return;
    }
    if (playlist.mediaCount() == 0)
    {
        qWarning() << "Failed to set background playlist, list is empty." << endl;
        return;
    }

    m_mediaPlaylist = &playlist;
}

void AalMediaPlayerService::setMedia(const QUrl &url)
{
    if (m_hubPlayerSession == nullptr)
    {
        qWarning() << "Cannot open uri without a valid media-hub player session";
        return;
    }

    // This is critical to allowing a different video source to be able to play correctly
    // if another video is already playing in the same AalMediaPlayerService instance
    if (m_videoOutput->textureId() > 0)
    {
        m_mediaPlayerControl->stop();
        resetVideoSink();
    }

    qDebug() << "Setting media to: " << url;

    if (m_mediaPlaylistProvider && url.isEmpty())
        m_mediaPlaylistProvider->clear();

    if (m_mediaPlaylistProvider == nullptr || m_mediaPlaylistProvider->mediaCount() == 0)
    {
        // errors are delivered via Player::errorOccurred()
        m_hubPlayerSession->openUri(url);
    }

    m_videoOutput->setupSurface();
}

void AalMediaPlayerService::play()
{
    qDebug() << Q_FUNC_INFO;
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot start playback without a valid media-hub player session";
        return;
    }

    if (m_videoOutput != NULL)
    {
        m_videoOutput->autoPlay(true);

        if (!m_firstPlayback)
            m_videoOutput->setupSurface();
    }

    // If we previously played and hit the end-of-stream, stop will be called which
    // tears down the video sink. We need a new video sink in order to render video again
    if (!m_videoOutputReady && m_videoOutput->textureId() > 0)
    {
        createVideoSink(m_videoOutput->textureId());
    }

    if (m_videoOutputReady || isAudioSource())
    {
        m_mediaPlayerControl->setMediaStatus(QMediaPlayer::LoadedMedia);

        qDebug() << "Actually calling m_hubPlayerSession->play()";
        m_hubPlayerSession->play();

        m_mediaPlayerControl->mediaPrepared();
    }
    else
        Q_EMIT serviceReady();
}

void AalMediaPlayerService::pause()
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot pause playback without a valid media-hub player session";
        return;
    }

    m_hubPlayerSession->pause();
}

void AalMediaPlayerService::stop()
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot stop playback without a valid media-hub player session";
        return;
    }

    m_hubPlayerSession->stop();
    m_videoOutputReady = false;
}

int64_t AalMediaPlayerService::position() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot get current playback position without a valid media-hub player session";
        return 0;
    }

    return m_hubPlayerSession->position() / 1e6;
}

void AalMediaPlayerService::setPosition(int64_t msec)
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot set current playback position without a valid media-hub player session";
        return;
    }
    m_hubPlayerSession->seekTo(msec * 1000);
}

int64_t AalMediaPlayerService::duration()
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot get playback duration without a valid media-hub player session";
        return 0;
    }

    const uint64_t d = m_hubPlayerSession->duration();
    // Make sure that apps get updated if the duration does in fact change
    if (d != m_cachedDuration)
    {
        m_cachedDuration = d;
        m_mediaPlayerControl->emitDurationChanged(d / 1e6);
    }
    return d / 1e6;
}

bool AalMediaPlayerService::isVideoSource() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot check if video source without a valid media-hub player session";
        return false;
    }

    return m_hubPlayerSession->isVideoSource();
}

bool AalMediaPlayerService::isAudioSource() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot if audio source without a valid media-hub player session";
        return false;
    }

    return m_hubPlayerSession->isAudioSource();
}

int AalMediaPlayerService::getVolume() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot get volume without a valid media-hub player session";
        return 0;
    }

    return m_hubPlayerSession->volume();
}

void AalMediaPlayerService::setVolume(int volume)
{
    Q_UNUSED(volume);

    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot set volume without a valid media-hub player session";
        return;
    }
}

void AalMediaPlayerService::createMediaPlayerControl()
{
    if (m_hubPlayerSession == nullptr)
        return;

    m_mediaPlayerControl = new AalMediaPlayerControl(this);
    connectSignals();
}

void AalMediaPlayerService::createVideoRendererControl()
{
    if (m_hubPlayerSession == nullptr)
        return;

    m_videoOutput = new AalVideoRendererControl(this);
}

void AalMediaPlayerService::createPlaylistControl()
{
    m_mediaPlaylistControl = new AalMediaPlaylistControl(this);
    m_mediaPlaylistProvider = new AalMediaPlaylistProvider(this);
    m_mediaPlaylistControl->setPlaylistProvider(m_mediaPlaylistProvider);
}

void AalMediaPlayerService::createAudioRoleControl()
{
    if (m_hubPlayerSession == nullptr)
        return;

    m_audioRoleControl = new AalAudioRoleControl(m_hubPlayerSession);
}

void AalMediaPlayerService::deleteMediaPlayerControl()
{
    if (not m_hubPlayerSession)
        return;

    if (m_mediaPlayerControl)
    {
        delete m_mediaPlayerControl;
        m_mediaPlayerControl = nullptr;
    }
}

void AalMediaPlayerService::destroyPlayerSession()
{
    if (not m_hubPlayerSession)
        return;

    // Invalidates the media-hub player session
    m_sessionUuid.clear();

    // When we arrived here the session is already invalid and we
    // can safely drop the reference.
    m_hubPlayerSession = nullptr;
}

void AalMediaPlayerService::deleteVideoRendererControl()
{
    if (m_videoOutput)
    {
        delete m_videoOutput;
        m_videoOutput = nullptr;
    }
}

void AalMediaPlayerService::deletePlaylistControl()
{
    if (m_mediaPlaylistProvider)
    {
        delete m_mediaPlaylistProvider;
        m_mediaPlaylistProvider = nullptr;
    }
    if (m_mediaPlaylistControl)
    {
        delete m_mediaPlaylistControl;
        m_mediaPlaylistControl = nullptr;
    }
}

void AalMediaPlayerService::deleteAudioRoleControl()
{
    if (m_audioRoleControl)
    {
        delete m_audioRoleControl;
        m_audioRoleControl = nullptr;
    }
}

void AalMediaPlayerService::signalQMediaPlayerError(const media::Error &error)
{
    QMediaPlayer::Error outError = QMediaPlayer::NoError;
    switch (error.code())
    {
        case media::Error::ResourceError:
            outError = QMediaPlayer::ResourceError;
            m_mediaPlayerControl->setMediaStatus(QMediaPlayer::InvalidMedia);
            break;
        case media::Error::FormatError:
            outError = QMediaPlayer::FormatError;
            m_mediaPlayerControl->setMediaStatus(QMediaPlayer::InvalidMedia);
            break;
        case media::Error::NetworkError:
            outError = QMediaPlayer::NetworkError;
            break;
        case media::Error::AccessDeniedError:
            outError = QMediaPlayer::AccessDeniedError;
            m_mediaPlayerControl->setMediaStatus(QMediaPlayer::InvalidMedia);
            break;
        case media::Error::ServiceMissingError:
            outError = QMediaPlayer::ServiceMissingError;
            break;
        default:
            break;
    }

    if (outError != QMediaPlayer::NoError)
        m_mediaPlayerControl->error(outError, error.message());
}

void AalMediaPlayerService::onPlaybackStatusChanged()
{
    // The media player control might have been released prior to this call. For that, we check for
    // null and return early in that case.
    if (m_mediaPlayerControl == nullptr)
        return;

    m_newStatus = m_hubPlayerSession->playbackStatus();
    // If the playback status changes from underneath (e.g. GStreamer or media-hub), make sure
    // the app is notified about this so it can change it's status
    switch (m_newStatus)
    {
        case media::Player::PlaybackStatus::Ready:
        case media::Player::PlaybackStatus::Stopped:
            m_mediaPlayerControl->setState(QMediaPlayer::StoppedState);
            break;
        case media::Player::PlaybackStatus::Paused:
            m_mediaPlayerControl->setState(QMediaPlayer::PausedState);
            break;
        case media::Player::PlaybackStatus::Playing:
            // This is necessary in case duration == 0 right after calling play(). At this point,
            // the pipeline should be 100% prepared and playing.
            Q_EMIT m_mediaPlayerControl->durationChanged(duration());
            m_mediaPlayerControl->setState(QMediaPlayer::PlayingState);
            break;
        default:
            qWarning() << "Unknown PlaybackStatus: " << m_newStatus;
    }

    qDebug() << "PlaybackStatus changed to: " << playbackStatusStr(m_newStatus);
}

void AalMediaPlayerService::onApplicationStateChanged(Qt::ApplicationState state)
{
    try {
        switch (state)
        {
            case Qt::ApplicationSuspended:
                qDebug() << "** Application has been suspended";
                break;
            case Qt::ApplicationHidden:
                qDebug() << "** Application is now hidden";
                break;
            case Qt::ApplicationInactive:
                qDebug() << "** Application is now inactive";
#ifdef DO_PLAYER_ATTACH_DETACH
                disconnectSignals();
                m_hubService->detach_session(m_sessionUuid, media::Player::Client::default_configuration());
                m_doReattachSession = true;
#endif
                break;
            case Qt::ApplicationActive:
                qDebug() << "** Application is now active";
#ifdef DO_PLAYER_ATTACH_DETACH
                // Avoid doing this for when the client application first loads as this
                // will break video playback
                if (m_doReattachSession)
                {
                    m_hubPlayerSession = m_hubService->
                        reattach_session(m_sessionUuid,
                                         media::Player::Client::default_configuration());
                    // Make sure the client's status (position, duraiton, state, etc) are all
                    // correct when reattaching to the media-hub Player session
                    updateClientSignals();
                    connectSignals();
                }
#endif
                break;
            default:
                qDebug() << "Unknown ApplicationState";
                break;
        }
    } catch (const std::runtime_error &e) {
        qWarning() << "Failed to respond to ApplicationState change: " << e.what();
    }
}

void AalMediaPlayerService::onServiceDisconnected()
{
    qDebug() << Q_FUNC_INFO;
    m_mediaPlayerControl->setState(QMediaPlayer::StoppedState);
    m_mediaPlayerControl->setMediaStatus(QMediaPlayer::NoMedia);
}

void AalMediaPlayerService::onServiceReconnected()
{
    qDebug() << Q_FUNC_INFO;
    const QString errStr = "Player session is no longer valid since the service restarted.";
    m_mediaPlayerControl->error(QMediaPlayer::ServiceMissingError, errStr);
}

void AalMediaPlayerService::onBufferingChanged()
{
    Q_EMIT m_mediaPlayerControl->bufferStatusChanged(m_bufferPercent);
}

void AalMediaPlayerService::updateClientSignals()
{
    qDebug() << Q_FUNC_INFO;
    if (m_mediaPlayerControl == nullptr)
        return;

    Q_EMIT m_mediaPlayerControl->durationChanged(duration());
    Q_EMIT m_mediaPlayerControl->positionChanged(position());
    switch (m_newStatus)
    {
        case media::Player::PlaybackStatus::Ready:
        case media::Player::PlaybackStatus::Stopped:
            m_mediaPlayerControl->setState(QMediaPlayer::StoppedState);
            break;
        case media::Player::PlaybackStatus::Paused:
            m_mediaPlayerControl->setState(QMediaPlayer::PausedState);
            break;
        case media::Player::PlaybackStatus::Playing:
            m_mediaPlayerControl->setState(QMediaPlayer::PlayingState);
            break;
        default:
            qWarning() << "Unknown PlaybackStatus: " << m_newStatus;
    }
}

void AalMediaPlayerService::connectSignals()
{
    QObject::connect(m_hubPlayerSession.get(), &media::Player::endOfStream,
                     this, [this]()
        {
            m_firstPlayback = false;
            Q_EMIT playbackComplete();
        });

    QObject::connect(m_hubPlayerSession.get(), &media::Player::serviceDisconnected,
                     this, &AalMediaPlayerService::onServiceReconnected);
    QObject::connect(m_hubPlayerSession.get(), &media::Player::serviceReconnected,
                     this, &AalMediaPlayerService::onServiceReconnected);
}

void AalMediaPlayerService::disconnectSignals()
{
    QObject::disconnect(m_hubPlayerSession.get(), nullptr, this, nullptr);
}

void AalMediaPlayerService::onError(const media::Error &error)
{
    qWarning() << "** Media playback error: " << error.message();
    signalQMediaPlayerError(error);
}

QString AalMediaPlayerService::playbackStatusStr(const media::Player::PlaybackStatus &status)
{
    switch (status)
    {
        case media::Player::PlaybackStatus::Ready:
            return "ready";
        case media::Player::PlaybackStatus::Stopped:
            return "stopped";
        case media::Player::PlaybackStatus::Paused:
            return "paused";
        case media::Player::PlaybackStatus::Playing:
            return "playing";
        default:
            qWarning() << "Unknown PlaybackStatus: " << status;
            return QString();
    }
}

void AalMediaPlayerService::setPlayer(const std::shared_ptr<media::Player> &player)
{
    m_hubPlayerSession = player;

    createMediaPlayerControl();
    createVideoRendererControl();

    QObject::connect(m_hubPlayerSession.get(), &media::Player::playbackStatusChanged,
                     this, &AalMediaPlayerService::onPlaybackStatusChanged);
}

#ifdef MEASURE_PERFORMANCE
void AalMediaPlayerService::measurePerformance()
{
    m_currentFrameDecodeStart = QDateTime::currentMSecsSinceEpoch();
    const qint64 delta = m_currentFrameDecodeStart - m_lastFrameDecodeStart;
    if (m_currentFrameDecodeStart != m_lastFrameDecodeStart) {
        m_lastFrameDecodeStart = QDateTime::currentMSecsSinceEpoch();
        if (delta > 0) {
            m_frameDecodeAvg += delta;
            qDebug() << "Frame-to-frame decode delta (ms): " << delta;
        }
    }

    if (m_avgCount == 30) {
        // Ideally if playing a video that was recorded at 30 fps, the average for
        // playback should be close to 30 fps too
        qDebug() << "Frame-to-frame decode average (ms) (30 frames times counted): " << (m_frameDecodeAvg / 30);
        m_avgCount = m_frameDecodeAvg = 0;
    }
    else
        ++m_avgCount;
}
#endif
