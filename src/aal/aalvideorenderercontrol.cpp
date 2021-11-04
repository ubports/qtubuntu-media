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

#include "aalvideorenderercontrol.h"
#include "aalmediaplayercontrol.h"
#include "aalmediaplayerservice.h"

#include <qtubuntu_media_signals.h>

#include <QAbstractVideoBuffer>
#include <QAbstractVideoSurface>
#include <QDebug>
#include <QGuiApplication>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVideoSurfaceFormat>
#include <QWindow>

#include <QThread>

#ifdef MEASURE_PERFORMANCE
#include <QDateTime>
#endif

#include <qgl.h>

namespace media = lomiri::MediaHub;
using namespace std::placeholders;

class AalGLTextureBuffer : public QAbstractVideoBuffer
{
public:
    AalGLTextureBuffer(GLuint textureId) :
        QAbstractVideoBuffer(QAbstractVideoBuffer::GLTextureHandle),
        m_textureId(textureId)
    {
    }

    MapMode mapMode() const { return NotMapped; }
    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        qDebug() << Q_FUNC_INFO;
        Q_UNUSED(mode);
        Q_UNUSED(numBytes);
        Q_UNUSED(bytesPerLine);

        return NULL;
    }

    void unmap()
    {
        qDebug() << Q_FUNC_INFO;
    }

    QVariant handle() const
    {
        return QVariant::fromValue<unsigned int>(m_textureId);
    }

    GLuint textureId() { return m_textureId; }

private:
    GLuint m_textureId;
};

AalVideoRendererControl::AalVideoRendererControl(AalMediaPlayerService *service, QObject *parent)
   : QVideoRendererControl(parent),
     m_surface(0),
     m_service(service),
     m_textureBuffer(0),
     m_textureId(0),
     m_orientation(media::Player::Orientation::Rotate0),
     m_height(0),
     m_width(0),
     m_autoPlay(false),
     m_surfaceStarted(false),
     m_flipped(false),
     m_doRendering(false),
     m_firstFrame(true),
     m_secondFrame(false)
#ifdef MEASURE_PERFORMANCE
     , m_lastFrameRenderStart(0)
     , m_currentFrameRenderStart(0)
     , m_avgCount(0)
     , m_frameRenderAvg(0)
#endif
{
    // Get notified when qtvideo-node creates a GL texture
    connect(SharedSignal::instance(), SIGNAL(textureCreated(unsigned int)),
            this, SLOT(onTextureCreated(unsigned int)));
    connect(SharedSignal::instance(), SIGNAL(glConsumerSet()), this, SLOT(onGLConsumerSet()));
    connect(m_service, SIGNAL(playbackComplete()), this, SLOT(playbackComplete()));
}

AalVideoRendererControl::~AalVideoRendererControl()
{
    if (m_textureBuffer) {
        GLuint textureId = m_textureBuffer->handle().toUInt();
        if (textureId > 0)
            glDeleteTextures(1, &textureId);

        delete m_textureBuffer;
    }
}

QAbstractVideoSurface *AalVideoRendererControl::surface() const
{
    return m_surface;
}

void AalVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface != surface) {
        m_surface = surface;
        Q_EMIT surfaceChanged(surface);
    }
}

GLuint AalVideoRendererControl::textureId() const
{
    return m_textureId;
}

uint32_t AalVideoRendererControl::height() const
{
    return m_height;
}

uint32_t AalVideoRendererControl::width() const
{
    return m_width;
}

void AalVideoRendererControl::autoPlay(bool doAutoPlay)
{
    m_autoPlay = doAutoPlay;
}

void AalVideoRendererControl::playbackComplete()
{
    qDebug() << Q_FUNC_INFO;
    // Make updateVideoTexture basically a no-op while this is set to false
    m_doRendering = false;

    // Reset key member variables so that we can start playback of the same video
    // another time if the user presses play again
    m_firstFrame = true;
    m_secondFrame = false;
    m_textureId = 0;
}

void AalVideoRendererControl::setupSurface()
{
    media::Player *player = m_service->getPlayer().get();
    QObject::connect(player, &media::Player::videoDimensionChanged,
                     this, &AalVideoRendererControl::onVideoDimensionChanged);

    // When orientation changes during playback, cache a copy here
    QObject::connect(player, &media::Player::orientationChanged,
                     this, [this, player]()
    {
        m_orientation = player->orientation();
    });

    if (!m_textureBuffer)
        m_textureBuffer = new AalGLTextureBuffer(m_textureId);

    // Enable rendering by enabling the logic in updateVideoTexture
    m_doRendering = true;

    updateVideoTexture();
}

void AalVideoRendererControl::onVideoDimensionChanged(const QSize &dimensions)
{
    qDebug() << Q_FUNC_INFO << dimensions;
    QSize frameSize;

    // Make sure that X & Y remain flipped between multiple playbacks in the
    // same session
    if ((m_orientation == media::Player::Orientation::Rotate90 ||
         m_orientation == media::Player::Orientation::Rotate270) && !m_flipped) {
        frameSize = dimensions.transposed();
        m_flipped = true;
    } else {
        frameSize = dimensions;
    }

    m_width = frameSize.width();
    m_height = frameSize.height();
    Q_EMIT SharedSignal::instance()->setOrientation(static_cast<SharedSignal::Orientation>(m_orientation), frameSize);
}

void AalVideoRendererControl::onFrameAvailable()
{
#ifdef MEASURE_PERFORMANCE
    s->measurePerformance();
#endif
    QMetaObject::invokeMethod(this, "updateVideoTexture", Qt::QueuedConnection);
}

void AalVideoRendererControl::updateVideoTexture()
{
    // Only render frames when explicitly desired
    if (!m_doRendering)
    {
        qWarning() << "Rendering not enabled, returning without presenting frame";
        return;
    }

    if (!m_surface) {
        qWarning() << "m_surface is NULL, can't update video texture" << endl;
        return;
    }

    if (!m_textureBuffer) {
        qWarning() << "m_textureBuffer is NULL, can't update video texture" << endl;
        return;
    }

    // If this is the first video frame being rendered, it's ok that m_textureId == 0.
    // This is necessary so that a ShaderVideoNode instance from qtvideo-node gets created,
    // as it is responsible for creating and returning a new texture and ID respectively.
    if (m_textureId == 0 && !m_firstFrame) {
        qWarning() << "m_textureId == 0, can't update video texture" << endl;
        return;
    }

    QVideoFrame frame(new AalGLTextureBuffer(m_textureId), QSize(m_width, m_height), QVideoFrame::Format_RGB32);
    if (!frame.isValid()) {
        qWarning() << "Frame is invalid, not presenting." << endl;
        return;
    }

    if (m_firstFrame) {
        // If this is the first video frame, we need to get a texture id from qtvideo-node.
        // Sending an empty frame triggers qtvideo-node to generate a texture id
        m_firstFrame = false;
        m_secondFrame = true;
    }
    else if (m_secondFrame) {
        frame.setMetaData("GLVideoSink", QVariant::fromValue(m_videoSink));
        m_secondFrame = false;
    }

    presentVideoFrame(frame);
}

void AalVideoRendererControl::onTextureCreated(unsigned int textureID)
{
    if (m_textureId == 0) {
        m_textureId = static_cast<GLuint>(textureID);
        // Remove old instance first (assignment first creates the new object,
        // then removes the old one, but we need the resources from the old
        // object to create the new one, so we force the right order with an
        // initial pointer reset).
        m_videoSink = &m_service->createVideoSink(textureID);

        // Connect callback so that frames are rendered after decoding
        QObject::connect(m_videoSink, &media::VideoSink::frameAvailable,
                         this, &AalVideoRendererControl::onFrameAvailable);

        // This call will make sure the video sink gets set on qtvideo-node
        updateVideoTexture();
    }
    else
        qDebug() << "Already have a texture id and video sink, not creating a new one";
}

void AalVideoRendererControl::onGLConsumerSet()
{
    qDebug() << Q_FUNC_INFO;
    // Only cause playback to start if QMediaPlayerControl::play() was already called.
    // See AalMediaPlayerService::play()
    if (m_autoPlay)
        m_service->play();
}

void AalVideoRendererControl::presentVideoFrame(const QVideoFrame &frame, bool empty)
{
    Q_UNUSED(empty);
    Q_ASSERT(m_surface != NULL);

    // Wait until we have a height and width to start m_surface
    if (!m_surface->isActive() || (m_height != 0 && m_width != 0)) {
        if (!m_surfaceStarted) {
            qDebug() << "Setting up surface with height: " << m_height << " width: " << m_width;
            QVideoSurfaceFormat format(frame.size(), frame.pixelFormat(), frame.handleType());

            if (!m_surface->start(format)) {
                qWarning() << "Failed to start video surface with format:" << format;
            }
            // Make sure we don't create any more new QVideoSurfaceFormat instances
            // after we have a proper height and width set
            if (m_height != 0 && m_width != 0)
                m_surfaceStarted = true;
        }
    }

#ifdef MEASURE_PERFORMANCE
    m_currentFrameRenderStart = QDateTime::currentMSecsSinceEpoch();
    const qint64 delta = m_currentFrameRenderStart - m_lastFrameRenderStart;
    if (m_currentFrameRenderStart != m_lastFrameRenderStart) {
        m_lastFrameRenderStart = QDateTime::currentMSecsSinceEpoch();
        if (delta > 0)
        {
            m_frameRenderAvg += delta;
            qDebug() << "-------------------------------------------------------------------";
            qDebug() << "Frame-to-frame delta (ms): " << delta << "(thread id: " << QThread::currentThreadId() << ")";
        }
    }

    if (m_avgCount == 30) {
        // Ideally if playing a video that was recorded at 30 fps, the average for
        // playback should be close to 30 fps too
        qDebug() << "Frame-to-frame average (ms) (30 frames times counted): " << (m_frameRenderAvg / 30);
        m_avgCount = m_frameRenderAvg = 0;
    }
    else
        ++m_avgCount;
#endif

    if (m_surface->isActive()) {
        m_surface->present(frame);
    }
}
