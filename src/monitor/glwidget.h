/*
 * Copyright (c) 2011-2014 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QSemaphore>
#include <QQuickView>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QMutex>
#include <QThread>
#include <QRect>

#include "scopes/sharedframe.h"
#include "definitions.h"

class QOpenGLFunctions_3_2_Core;
//class QmlFilter;
//class QmlMetadata;

namespace Mlt
{
class Filter;
}

class RenderThread;
class FrameRenderer;

typedef void *(*thread_function_t)(void *);

class GLWidget : public QQuickView, protected QOpenGLFunctions
{
    Q_OBJECT
    Q_PROPERTY(QRect rect READ rect NOTIFY rectChanged)
    Q_PROPERTY(float zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(QPoint offset READ offset NOTIFY offsetChanged)

public:
    GLWidget(int id, QObject *parent = nullptr);
    ~GLWidget();

    void createThread(RenderThread **thread, thread_function_t function, void *data);
    void startGlsl();
    void stopGlsl();
    void clear();
    /** @brief Update producer, should ONLY be called from renderer.cpp */
    int setProducer(Mlt::Producer *producer);
    int reconfigureMulti(const QString &params, const QString &path, Mlt::Profile *profile);
    void stopCapture();
    int reconfigure(Mlt::Profile *profile = nullptr);

    int displayWidth() const
    {
        return m_rect.width();
    }
    void updateAudioForAnalysis();
    int displayHeight() const
    {
        return m_rect.height();
    }

    QObject *videoWidget()
    {
        return this;
    }
    Mlt::Filter *glslManager() const
    {
        return m_glslManager;
    }
    QRect rect() const
    {
        return m_rect;
    }
    QRect effectRect() const
    {
        return m_effectRect;
    }
    float zoom() const;
    float scale() const;
    QPoint offset() const;
    Mlt::Consumer *consumer();
    Mlt::Producer *producer();
    QSize profileSize() const;
    QRect displayRect() const;
    /** @brief set to true if we want to emit a QImage of the frame for analysis */
    bool sendFrameForAnalysis;
    void updateGamma();
    Mlt::Profile *profile();
    void resetProfile(const MltVideoProfile &profile);
    void reloadProfile(Mlt::Profile &profile);
    void lockMonitor();
    void releaseMonitor();
    int realTime() const;
    void setAudioThumb(int channels = 0, const QVariantList &audioCache = QList<QVariant>());
    int droppedFrames() const;
    void resetDrops();

protected:
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

public slots:
    void setZoom(float zoom);
    void setOffsetX(int x, int max);
    void setOffsetY(int y, int max);
    void slotSwitchAudioOverlay(bool enable);
    void slotZoomScene(double value);
    void initializeGL();
    void releaseAnalyse();

signals:
    void frameDisplayed(const SharedFrame &frame);
    void textureUpdated();
    void dragStarted();
    void seekTo(int x);
    void gpuNotSupported();
    void started();
    void paused();
    void playing();
    void rectChanged();
    void zoomChanged();
    void offsetChanged();
    void monitorPlay();
    void switchFullScreen(bool minimizeOnly = false);
    void mouseSeek(int eventDelta, int modifiers);
    void startDrag();
    void analyseFrame(const QImage&);
    void audioSamplesSignal(const audioShortVector &, int, int, int);
    void showContextMenu(const QPoint &);
    void lockMonitor(bool);
    void passKeyEvent(QKeyEvent *);
    void panView(const QPoint &diff);

private:
    int m_id;
    QRect m_rect;
    QRect m_effectRect;
    GLuint m_texture[3];
    QOpenGLShaderProgram *m_shader;
    QPoint m_panStart;
    QPoint m_dragStart;
    Mlt::Filter *m_glslManager;
    Mlt::Consumer *m_consumer;
    Mlt::Producer *m_producer;
    QSemaphore m_initSem;
    QSemaphore m_analyseSem;
    bool m_isInitialized;
    Mlt::Event *m_threadStartEvent;
    Mlt::Event *m_threadStopEvent;
    Mlt::Event *m_threadCreateEvent;
    Mlt::Event *m_threadJoinEvent;
    Mlt::Event *m_displayEvent;
    Mlt::Profile *m_monitorProfile;
    FrameRenderer *m_frameRenderer;
    int m_projectionLocation;
    int m_modelViewLocation;
    int m_vertexLocation;
    int m_texCoordLocation;
    int m_colorspaceLocation;
    int m_textureLocation[3];
    float m_zoom;
    bool m_openGLSync;
    bool m_sendFrame;
    SharedFrame m_sharedFrame;
    QMutex m_mutex;
    QPoint m_offset;
    QOffscreenSurface m_offscreenSurface;
    QOpenGLContext *m_shareContext;
    bool m_audioWaveDisplayed;
    static void on_frame_show(mlt_consumer, void *self, mlt_frame frame);
    static void on_gl_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr);
    static void on_gl_nosync_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr);
    void createAudioOverlay(bool isAudio);
    void removeAudioOverlay();
    void adjustAudioOverlay(bool isAudio);
    QOpenGLFramebufferObject *m_fbo;
    void refreshSceneLayout();

private slots:
    void resizeGL(int width, int height);
    void updateTexture(GLuint yName, GLuint uName, GLuint vName);
    void paintGL();
    void onFrameDisplayed(const SharedFrame &frame);

protected:
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void createShader();
};

class RenderThread : public QThread
{
    Q_OBJECT
public:
    RenderThread(thread_function_t function, void *data, QOpenGLContext *context, QSurface *surface);
    ~RenderThread();

protected:
    void run() Q_DECL_OVERRIDE;

private:
    thread_function_t m_function;
    void *m_data;
    QOpenGLContext *m_context;
    QSurface *m_surface;
};

class FrameRenderer : public QThread
{
    Q_OBJECT
public:
    explicit FrameRenderer(QOpenGLContext *shareContext, QSurface *surface);
    ~FrameRenderer();
    QSemaphore *semaphore()
    {
        return &m_semaphore;
    }
    QOpenGLContext *context() const
    {
        return m_context;
    }
    void clearFrame();
    Q_INVOKABLE void showFrame(Mlt::Frame frame);
    Q_INVOKABLE void showGLFrame(Mlt::Frame frame);
    Q_INVOKABLE void showGLNoSyncFrame(Mlt::Frame frame);

public slots:
    void cleanup();

signals:
    void textureReady(GLuint yName, GLuint uName = 0, GLuint vName = 0);
    void frameDisplayed(const SharedFrame &frame);
    void audioSamplesSignal(const audioShortVector &, int, int, int);

private:
    QSemaphore m_semaphore;
    SharedFrame m_frame;
    SharedFrame m_displayFrame;
    QOpenGLContext *m_context;
    QSurface *m_surface;

public:
    GLuint m_renderTexture[3];
    GLuint m_displayTexture[3];
    QOpenGLFunctions_3_2_Core *m_gl32;
    bool sendAudioForAnalysis;
};

#endif
