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

#ifndef AALVIDEORENDERERCONTROL_H
#define AALVIDEORENDERERCONTROL_H

#include <MediaHub/Player>
#include <MediaHub/VideoSink>

#include <QImage>
#include <QVideoFrame>
#include <QVideoRendererControl>

// Uncomment to measure video frame rendering timing and performance. Will display
// metrics to stdout
//#define MEASURE_PERFORMANCE

class AalMediaPlayerService;
class AalGLTextureBuffer;

// Avoids a clash between Qt5's opengl headers and the platform GLES
// headers
typedef unsigned int GLuint;

class AalVideoRendererControl : public QVideoRendererControl
{
    Q_OBJECT

    friend class AalMediaPlayerService;

public:
    AalVideoRendererControl(AalMediaPlayerService *service, QObject *parent = 0);
    ~AalVideoRendererControl();

    QAbstractVideoSurface *surface() const;
    void setSurface(QAbstractVideoSurface *surface);

    GLuint textureId() const;

    uint32_t height() const;
    uint32_t width() const;

    // Whether QMediaPlayerService::play() will be called from onGLConsumerSet or not
    void autoPlay(bool doAutoPlay = true);

    // Callbacks
    static void updateVideoTextureCb(void *context);

public Q_SLOTS:
    void setupSurface();
    void playbackComplete();

Q_SIGNALS:
    void surfaceChanged(QAbstractVideoSurface *surface);

private Q_SLOTS:
    void updateVideoTexture();
    void onTextureCreated(unsigned int textureID);
    void onGLConsumerSet();

private:
    void onVideoDimensionChanged(const QSize &dimensions);
    void onFrameAvailable();
    void presentVideoFrame(const QVideoFrame &frame, bool empty = false);

    QAbstractVideoSurface *m_surface;
    AalMediaPlayerService *m_service;
    lomiri::MediaHub::VideoSink *m_videoSink;
    AalGLTextureBuffer *m_textureBuffer;
    GLuint m_textureId;

    lomiri::MediaHub::Player::Orientation m_orientation;
    uint32_t m_height;
    uint32_t m_width;
    bool m_autoPlay;
    bool m_surfaceStarted;
    bool m_flipped;
    bool m_doRendering;

    bool m_firstFrame;
    bool m_secondFrame;

#ifdef MEASURE_PERFORMANCE
    qint64 m_lastFrameRenderStart;
    qint64 m_currentFrameRenderStart;
    qint16 m_avgCount;
    qint64 m_frameRenderAvg;
#endif
};

#endif
