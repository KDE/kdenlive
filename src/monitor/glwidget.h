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
#include <QMutex>
#include <QThread>
#include <QRect>
#include "sharedframe.h"

class QOpenGLFunctions_3_2_Core;
class QOffscreenSurface;
class QOpenGLTexture;
//class QmlFilter;
//class QmlMetadata;

namespace Mlt {
class Filter;
}

class RenderThread;
class FrameRenderer;


typedef void* ( *thread_function_t )( void* );

class GLWidget : public QQuickView, protected QOpenGLFunctions
{
    Q_OBJECT
    Q_PROPERTY(QRect rect READ rect NOTIFY rectChanged)
    Q_PROPERTY(float zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(QPoint offset READ offset NOTIFY offsetChanged)

public:
    GLWidget(bool accel = false, QObject *parent = 0);
    ~GLWidget();

    void createThread(RenderThread** thread, thread_function_t function, void* data);
    void startGlsl();
    void stopGlsl();
    int setProducer(Mlt::Producer*, bool isMulti = false);
    int reconfigure(bool isMulti);

    int displayWidth() const { return m_rect.width(); }
    int displayHeight() const { return m_rect.height(); }

    QObject* videoWidget() { return this; }
    QQuickView* videoQuickView() { return this; }
    Mlt::Filter* glslManager() const { return m_glslManager; }
    QRect rect() const { return m_rect; }
    QRect effectRect() const { return m_effectRect; }
    float zoom() const;
    QPoint offset() const;
    Mlt::Consumer *consumer();
    Mlt::Producer *producer();
    QSize profileSize() const;
    /** @brief set to true if we want to emit a QImage of the frame for analysis */
    bool sendFrameForAnalysis;

protected:
    void mouseReleaseEvent(QMouseEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);

public slots:
    void setZoom(float zoom);
    void setOffsetX(int x);
    void setOffsetY(int y);
    void setBlankScene();
    void slotShowEffectScene();
    void slotShowRootScene();
    void slotSwitchAudioOverlay(bool enable);
    //void setCurrentFilter(QmlFilter* filter, QmlMetadata* meta);

signals:
    void frameDisplayed(const SharedFrame& frame);
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
    void mouseSeek(int eventDelta, bool fast);
    void startDrag();
    void effectChanged(QRect r);
    void analyseFrame(QImage);

private:
    QRect m_rect;
    QRect m_effectRect;
    GLuint m_texture[3];
    QOpenGLShaderProgram* m_shader;
    QPoint m_dragStart;
    Mlt::Filter* m_glslManager;
    Mlt::Consumer* m_consumer;
    Mlt::Producer* m_producer;
    QSemaphore m_initSem;
    bool m_isInitialized;
    Mlt::Event* m_threadStartEvent;
    Mlt::Event* m_threadStopEvent;
    Mlt::Event* m_threadCreateEvent;
    Mlt::Event* m_threadJoinEvent;
    FrameRenderer* m_frameRenderer;
    int m_projectionLocation;
    int m_modelViewLocation;
    int m_vertexLocation;
    int m_texCoordLocation;
    int m_colorspaceLocation;
    int m_textureLocation[3];
    float m_zoom;
    QPoint m_offset;
    bool m_audioWaveDisplayed;
    static void on_frame_show(mlt_consumer, void* self, mlt_frame frame);
    void createAudioOverlay(bool isAudio);
    void removeAudioOverlay();
    void adjustAudioOverlay(bool isAudio);
    QOpenGLFramebufferObject *m_fbo;

private slots:
    void initializeGL();
    void resizeGL(int width, int height);
    void updateTexture(GLuint yName, GLuint uName, GLuint vName);
    void paintGL();
    void effectRectChanged();

protected:
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void keyPressEvent(QKeyEvent* event);
    void createShader();
};

class RenderThread : public QThread
{
    Q_OBJECT
public:
    RenderThread(thread_function_t function, void* data, QOpenGLContext *context);

protected:
    void run();

private:
    thread_function_t m_function;
    void* m_data;
    QOpenGLContext* m_context;
    QOffscreenSurface* m_surface;
};

class FrameRenderer : public QThread
{
    Q_OBJECT
public:
    FrameRenderer(QOpenGLContext* shareContext);
    ~FrameRenderer();
    QSemaphore* semaphore() { return &m_semaphore; }
    QOpenGLContext* context() const { return m_context; }
    SharedFrame getDisplayFrame();
    Q_INVOKABLE void showFrame(Mlt::Frame frame);

public slots:
    void cleanup();

signals:
    void textureReady(GLuint yName, GLuint uName = 0, GLuint vName = 0);
    void frameDisplayed(const SharedFrame& frame);
    void analyseFrame(QImage);

private:
    QSemaphore m_semaphore;
    SharedFrame m_frame;
    QOpenGLContext* m_context;
    QOffscreenSurface* m_surface;

public:
    GLuint m_renderTexture[3];
    GLuint m_displayTexture[3];
    QOpenGLFunctions_3_2_Core* m_gl32;
};



#endif
