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

#include <core/media/service.h>
#include <core/media/track_list.h>

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

namespace media = core::ubuntu::media;

using namespace std::placeholders;

namespace
{
enum {
    OK          = 0,
    NO_ERROR    = 0,
    BAD_VALUE   = -EINVAL,
};

core::Signal<void> the_void;
}

class EmptySink : public core::ubuntu::media::video::Sink
{
public:
    EmptySink(std::uint32_t gl_texture)
    {
        Q_UNUSED(gl_texture);
    }

    const core::Signal<void>& frame_available() const override
    {
        static const core::Signal<void> frame_available;
        return frame_available;
    }

    bool transformation_matrix(float* matrix) const override
    {
        Q_UNUSED(matrix);
        return false;
    }

    bool swap_buffers() const override
    {
        return false;
    }
};


AalMediaPlayerService::AalMediaPlayerService(QObject *parent)
    :
     QMediaService(parent),
     m_hubPlayerSession(nullptr),
     m_playbackStatusChangedConnection(the_void.connect([](){})),
     m_errorConnection(the_void.connect([](){})),
     m_endOfStreamConnection(the_void.connect([](){})),
     m_serviceDisconnectedConnection(the_void.connect([](){})),
     m_serviceReconnectedConnection(the_void.connect([](){})),
     m_bufferingStatusChangedConnection(the_void.connect([](){})),
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

AalMediaPlayerService::AalMediaPlayerService
    (const std::shared_ptr<core::ubuntu::media::Service> &service, QObject *parent)
    :
      QMediaService(parent),
      m_hubService(service),
      m_hubPlayerSession(nullptr),
      m_playbackStatusChangedConnection(the_void.connect([](){})),
      m_errorConnection(the_void.connect([](){})),
      m_endOfStreamConnection(the_void.connect([](){})),
      m_serviceDisconnectedConnection(the_void.connect([](){})),
      m_serviceReconnectedConnection(the_void.connect([](){})),
      m_bufferingStatusChangedConnection(the_void.connect([](){})),
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
    m_errorConnection.disconnect();
    m_playbackStatusChangedConnection.disconnect();
    m_serviceDisconnectedConnection.disconnect();
    m_serviceReconnectedConnection.disconnect();

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
    if (not m_hubService.get())
        m_hubService = media::Service::Client::instance();

    // As core::Connection doesn't allow us to start with a disconnected connection
    // instance we have to connect it first with a dummy signal and then disconnect
    // it again. If we don't do this connectSignals() will never be able to attach
    // to the relevant signals.
    m_endOfStreamConnection.disconnect();
    m_serviceDisconnectedConnection.disconnect();
    m_serviceReconnectedConnection.disconnect();

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

    m_playbackStatusChangedConnection = m_hubPlayerSession->playback_status_changed().connect(
        [this](const media::Player::PlaybackStatus &status) {
            m_newStatus = status;
            QMetaObject::invokeMethod(this, "onPlaybackStatusChanged", Qt::QueuedConnection);
        });

    m_bufferingStatusChangedConnection = m_hubPlayerSession->buffering_changed().connect(
                [this](int bufferingPercent) {
                    m_bufferPercent = bufferingPercent;
                    QMetaObject::invokeMethod(this, "onBufferingChanged", Qt::DirectConnection);
                });

    m_errorConnection = m_hubPlayerSession->error().connect(
            std::bind(&AalMediaPlayerService::onError, this, _1));
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

    if (m_hubService == nullptr)
    {
        qWarning() << "Cannot create new media player instance without a valid media-hub service instance";
        return false;
    }

    try {
        m_hubPlayerSession = m_hubService->create_session(media::Player::Client::default_configuration());
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to start a new media-hub player session: " << e.what();
        return false;
    }

    try {
        // Get the player session UUID so we can suspend/restore our session when the ApplicationState
        // changes
        m_sessionUuid = m_hubPlayerSession->uuid();
    } catch (const std::runtime_error &e) {
        qWarning() << "Failed to retrieve the current player's uuid: " << e.what();
        return false;
    }

    return true;
}

std::shared_ptr<core::ubuntu::media::video::Sink> AalMediaPlayerService::createVideoSink(uint32_t texture_id)
{
    if (m_hubPlayerSession == nullptr)
        throw std::runtime_error
        {
            "Cannot create a video sink without a valid media-hub player session"
        };

    std::shared_ptr<core::ubuntu::media::video::Sink> sink;
    try {
        sink = m_hubPlayerSession->create_gl_texture_video_sink(texture_id);
    } catch (const std::runtime_error &e) {
        qWarning() << "Failed to create a new video sink:" << e.what();
        return core::ubuntu::media::video::Sink::Ptr{new EmptySink(0)};
    }
    m_videoOutputReady = true;

    return sink;
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
        try {
            m_hubPlayerSession->open_uri(url.toString().toStdString());
        }
        catch (const media::Player::Errors::InsufficientAppArmorPermissions &e) {
            qWarning() << e.what();
            signalQMediaPlayerError(media::Player::Error::resource_error);
            return;
        }
        catch (const media::Player::Errors::UriNotFound &e) {
            qWarning() << e.what();
            signalQMediaPlayerError(media::Player::Error::resource_error);
            return;
        }
        catch (const std::runtime_error &e) {
            qWarning() << "Failed to set media " << url << ": " << e.what();
            return;
        }
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
        try {
            m_mediaPlayerControl->setMediaStatus(QMediaPlayer::LoadedMedia);

            qDebug() << "Actually calling m_hubPlayerSession->play()";
            m_hubPlayerSession->play();

            m_mediaPlayerControl->mediaPrepared();
        }
        catch (const std::runtime_error &e) {
            qWarning() << "Failed to start playback: " << e.what();
            return;
        }
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

    try {
        m_hubPlayerSession->pause();
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to pause playback: " << e.what();
        return;
    }
}

void AalMediaPlayerService::stop()
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot stop playback without a valid media-hub player session";
        return;
    }

    try {
        m_hubPlayerSession->stop();
        m_videoOutputReady = false;
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to stop playback: " << e.what();
        return;
    }
}

int64_t AalMediaPlayerService::position() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot get current playback position without a valid media-hub player session";
        return 0;
    }

    try {
        return m_hubPlayerSession->position() / 1e6;
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to get current playback position: " << e.what();
        return 0;
    }
}

void AalMediaPlayerService::setPosition(int64_t msec)
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot set current playback position without a valid media-hub player session";
        return;
    }
    try {
        m_hubPlayerSession->seek_to(std::chrono::microseconds{msec * 1000});
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to set position to " << msec << ": " << e.what();
    }
}

int64_t AalMediaPlayerService::duration()
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot get playback duration without a valid media-hub player session";
        return 0;
    }

    try {
        const int64_t d = m_hubPlayerSession->duration();
        // Make sure that apps get updated if the duration does in fact change
        if (d != m_cachedDuration)
        {
            m_cachedDuration = d;
            m_mediaPlayerControl->emitDurationChanged(d / 1e6);
        }
        return d / 1e6;
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to get current playback duration: " << e.what();
        return 0;
    }
}

bool AalMediaPlayerService::isVideoSource() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot check if video source without a valid media-hub player session";
        return false;
    }

    try {
        return m_hubPlayerSession->is_video_source();
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to check if source is video: " << e.what();
        return false;
    }
}

bool AalMediaPlayerService::isAudioSource() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot if audio source without a valid media-hub player session";
        return false;
    }

    try {
        return m_hubPlayerSession->is_audio_source();
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to check if source is video: " << e.what();
        return false;
    }
}

int AalMediaPlayerService::getVolume() const
{
    if (m_hubPlayerSession == NULL)
    {
        qWarning() << "Cannot get volume without a valid media-hub player session";
        return 0;
    }

    try {
        return m_hubPlayerSession->volume();
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to get current volume level: " << e.what();
        return 0;
    }
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

    try {
        // Invalidates the media-hub player session
        m_hubService->destroy_session(m_sessionUuid, media::Player::Client::default_configuration());
        m_sessionUuid.clear();

        // When we arrived here the session is already invalid and we
        // can safely drop the reference.
        m_hubPlayerSession = nullptr;
    }
    catch (const std::runtime_error &e) {
        qWarning() << "Failed to destroy existing media-hub player session: " << e.what();
    }
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

void AalMediaPlayerService::signalQMediaPlayerError(const media::Player::Error &error)
{
    QMediaPlayer::Error outError = QMediaPlayer::NoError;
    QString outErrorStr;
    switch (error)
    {
        case media::Player::Error::resource_error:
            outError = QMediaPlayer::ResourceError;
            outErrorStr = "A media resource couldn't be resolved.";
            m_mediaPlayerControl->setMediaStatus(QMediaPlayer::InvalidMedia);
            break;
        case media::Player::Error::format_error:
            outError = QMediaPlayer::FormatError;
            outErrorStr = "The media format type is not playable due to a missing codec.";
            m_mediaPlayerControl->setMediaStatus(QMediaPlayer::InvalidMedia);
            break;
        case media::Player::Error::network_error:
            outError = QMediaPlayer::NetworkError;
            outErrorStr = "A network error occurred.";
            break;
        case media::Player::Error::access_denied_error:
            outError = QMediaPlayer::AccessDeniedError;
            outErrorStr = "Insufficient privileges to play that media.";
            m_mediaPlayerControl->setMediaStatus(QMediaPlayer::InvalidMedia);
            break;
        case media::Player::Error::service_missing_error:
            outError = QMediaPlayer::ServiceMissingError;
            outErrorStr = "A valid playback service was not found, playback cannot proceed.";
            break;
        default:
            break;
    }

    if (outError != QMediaPlayer::NoError)
        m_mediaPlayerControl->error(outError, outErrorStr);
}

void AalMediaPlayerService::onPlaybackStatusChanged()
{
    // The media player control might have been released prior to this call. For that, we check for
    // null and return early in that case.
    if (m_mediaPlayerControl == nullptr)
        return;

    // If the playback status changes from underneath (e.g. GStreamer or media-hub), make sure
    // the app is notified about this so it can change it's status
    switch (m_newStatus)
    {
        case media::Player::PlaybackStatus::ready:
        case media::Player::PlaybackStatus::stopped:
            m_mediaPlayerControl->setState(QMediaPlayer::StoppedState);
            break;
        case media::Player::PlaybackStatus::paused:
            m_mediaPlayerControl->setState(QMediaPlayer::PausedState);
            break;
        case media::Player::PlaybackStatus::playing:
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
        case media::Player::PlaybackStatus::ready:
        case media::Player::PlaybackStatus::stopped:
            m_mediaPlayerControl->setState(QMediaPlayer::StoppedState);
            break;
        case media::Player::PlaybackStatus::paused:
            m_mediaPlayerControl->setState(QMediaPlayer::PausedState);
            break;
        case media::Player::PlaybackStatus::playing:
            m_mediaPlayerControl->setState(QMediaPlayer::PlayingState);
            break;
        default:
            qWarning() << "Unknown PlaybackStatus: " << m_newStatus;
    }
}

void AalMediaPlayerService::connectSignals()
{
    if (!m_endOfStreamConnection.is_connected())
    {
        m_endOfStreamConnection = m_hubPlayerSession->end_of_stream().connect([this]()
        {
            m_firstPlayback = false;
            Q_EMIT playbackComplete();
        });
    }

    if (!m_serviceDisconnectedConnection.is_connected())
    {
        m_serviceDisconnectedConnection = m_hubService->service_disconnected().connect([this]()
        {
            QMetaObject::invokeMethod(this, "onServiceDisconnected", Qt::QueuedConnection);
        });
    }

    if (!m_serviceReconnectedConnection.is_connected())
    {
        m_serviceReconnectedConnection = m_hubService->service_reconnected().connect([this]()
        {
            QMetaObject::invokeMethod(this, "onServiceReconnected", Qt::QueuedConnection);
        });
    }
}

void AalMediaPlayerService::disconnectSignals()
{
    if (m_endOfStreamConnection.is_connected())
        m_endOfStreamConnection.disconnect();
}

void AalMediaPlayerService::onError(const media::Player::Error &error)
{
    qWarning() << "** Media playback error: " << error;
    signalQMediaPlayerError(error);
}

QString AalMediaPlayerService::playbackStatusStr(const media::Player::PlaybackStatus &status)
{
    switch (status)
    {
        case media::Player::PlaybackStatus::ready:
            return "ready";
        case media::Player::PlaybackStatus::stopped:
            return "stopped";
        case media::Player::PlaybackStatus::paused:
            return "paused";
        case media::Player::PlaybackStatus::playing:
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

    if (!m_playbackStatusChangedConnection.is_connected())
    {
        m_playbackStatusChangedConnection = m_hubPlayerSession->playback_status_changed().connect(
            [this](const media::Player::PlaybackStatus &status) {
                m_newStatus = status;
                QMetaObject::invokeMethod(this, "onPlaybackStatusChanged", Qt::QueuedConnection);
            });
    }
}

void AalMediaPlayerService::setService(const std::shared_ptr<media::Service> &service)
{
    m_hubService = service;
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
