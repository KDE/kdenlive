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

#include <QFont>
#include <QMutex>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QQuickView>
#include <QRect>
#include <QSemaphore>
#include <QThread>
#include <QTimer>

#include "bin/model/markerlistmodel.hpp"
#include "definitions.h"
#include "kdenlivesettings.h"
#include "scopes/sharedframe.h"

#include <mlt++/MltProfile.h>

class QOpenGLFunctions_3_2_Core;

namespace Mlt {
class Filter;
class Producer;
class Consumer;
} // namespace Mlt

class RenderThread;
class FrameRenderer;
class MonitorProxy;

using thread_function_t = void *(*)(void *);

/* QQuickView that renders an .
 *
 * Creates an MLT consumer and renders a GL view from the consumer. This pipeline is one of:
 *
 *    A. YUV gl texture w/o GPU filter acceleration
 *    B. YUV gl texture multithreaded w/o GPU filter acceleration
 *    C. RGB gl texture multithreaded w/ GPU filter acceleration and no sync
 *    D. RGB gl texture multithreaded w/ GPU filter acceleration and sync
 */
class GLWidget : public QQuickView, protected QOpenGLFunctions
{
    Q_OBJECT
    Q_PROPERTY(QRect rect READ rect NOTIFY rectChanged)
    Q_PROPERTY(float zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(QPoint offset READ offset NOTIFY offsetChanged)

public:
    friend class MonitorController;
    friend class Monitor;
    friend class MonitorProxy;
    using ClientWaitSync_fp = GLenum (*)(GLsync, GLbitfield, GLuint64);

    GLWidget(int id, QObject *parent = nullptr);
    ~GLWidget() override;

    int requestedSeekPosition;
    void createThread(RenderThread **thread, thread_function_t function, void *data);
    void startGlsl();
    void stopGlsl();
    void clear();
    // TODO: currently unused
    int reconfigureMulti(const QString &params, const QString &path, Mlt::Profile *profile);
    void stopCapture();
    int reconfigure();
    /** @brief Get the current MLT producer playlist.
     * @return A string describing the playlist */
    const QString sceneList(const QString &root, const QString &fullPath = QString());

    int displayWidth() const { return m_rect.width(); }
    void updateAudioForAnalysis();
    int displayHeight() const { return m_rect.height(); }

    QObject *videoWidget() { return this; }
    Mlt::Filter *glslManager() const { return m_glslManager; }
    QRect rect() const { return m_rect; }
    QRect effectRect() const { return m_effectRect; }
    float zoom() const;
    QPoint offset() const;
    std::shared_ptr<Mlt::Consumer> consumer();
    Mlt::Producer *producer();
    QSize profileSize() const;
    QRect displayRect() const;
    /** @brief set to true if we want to emit a QImage of the frame for analysis */
    bool sendFrameForAnalysis;
    void updateGamma();
    /** @brief delete and rebuild consumer, for example when external display is switched */
    void resetConsumer(bool fullReset);
    void lockMonitor();
    void releaseMonitor();
    int realTime() const;
    int droppedFrames() const;
    void resetDrops();
    bool checkFrameNumber(int pos, int offset, bool isPlaying);
    /** @brief Return current timeline position */
    int getCurrentPos() const;
    /** @brief Requests a monitor refresh */
    void requestRefresh();
    void setRulerInfo(int duration, const std::shared_ptr<MarkerListModel> &model = nullptr);
    MonitorProxy *getControllerProxy();
    bool playZone(bool loop = false);
    bool loopClip();
    void startConsumer();
    void stop();
    int rulerHeight() const;
    /** @brief return current play producer's playing speed */
    double playSpeed() const;
    /** @brief Turn drop frame feature on/off */
    void setDropFrames(bool drop);
    /** @brief Returns current audio volume */
    int volume() const;
    /** @brief Set audio volume on consumer */
    void setVolume(double volume);
    /** @brief Returns current producer's duration in frames */
    int duration() const;
    /** @brief Set a property on the MLT consumer */
    void setConsumerProperty(const QString &name, const QString &value);
    /** @brief Clear consumer cache */
    void purgeCache();
    /** @brief Show / hide monitor ruler */
    void switchRuler(bool show);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    /** @brief Update producer, should ONLY be called from monitor */
    int setProducer(const std::shared_ptr<Mlt::Producer> &producer, bool isActive, int position);
    int setProducer(const QString &file);
    QString frameToTime(int frames) const;

public slots:
    void requestSeek(int position);
    void setZoom(float zoom);
    void setOffsetX(int x, int max);
    void setOffsetY(int y, int max);
    void slotZoom(bool zoomIn);
    void initializeGL();
    void releaseAnalyse();
    void switchPlay(bool play, double speed = 1.0);
    void reloadProfile();
    /** @brief Update MLT's consumer scaling */
    void updateScaling();

signals:
    void frameDisplayed(const SharedFrame &frame);
    void frameRendered(int pos);
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
    void mouseSeek(int eventDelta, uint modifiers);
    void startDrag();
    void analyseFrame(const QImage &);
    void showContextMenu(const QPoint &);
    void lockMonitor(bool);
    void passKeyEvent(QKeyEvent *);
    void panView(const QPoint &diff);
    void activateMonitor();

protected:
    Mlt::Filter *m_glslManager;
    // TODO: MTL has lock/unlock of individual nodes. Use those.
    // keeping this for refactoring ease.
    QMutex m_mltMutex;
    std::shared_ptr<Mlt::Consumer> m_consumer;
    std::shared_ptr<Mlt::Producer> m_producer;
    int m_id;
    int m_rulerHeight;
    QColor m_bgColor;

private:
    QRect m_rect;
    QRect m_effectRect;
    GLuint m_texture[3];
    QOpenGLShaderProgram *m_shader;
    QPoint m_panStart;
    QPoint m_dragStart;
    QSemaphore m_initSem;
    QSemaphore m_analyseSem;
    bool m_isInitialized;
    Mlt::Event *m_threadStartEvent;
    Mlt::Event *m_threadStopEvent;
    Mlt::Event *m_threadCreateEvent;
    Mlt::Event *m_threadJoinEvent;
    Mlt::Event *m_displayEvent;
    Mlt::Event *m_renderEvent;
    FrameRenderer *m_frameRenderer;
    int m_projectionLocation;
    int m_modelViewLocation;
    int m_vertexLocation;
    int m_texCoordLocation;
    int m_colorspaceLocation;
    int m_textureLocation[3];
    QTimer m_refreshTimer;
    float m_zoom;
    QSize m_profileSize;
    int m_colorSpace;
    double m_dar;
    bool m_sendFrame;
    bool m_isZoneMode;
    bool m_isLoopMode;
    QPoint m_offset;
    MonitorProxy *m_proxy;
    std::shared_ptr<Mlt::Producer> m_blackClip;
    static void on_frame_show(mlt_consumer, void *self, mlt_frame frame);
    static void on_frame_render(mlt_consumer, GLWidget *widget, mlt_frame frame);
    static void on_gl_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr);
    static void on_gl_nosync_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr);
    QOpenGLFramebufferObject *m_fbo;
    void refreshSceneLayout();
    void resetZoneMode();

    /* OpenGL context management. Interfaces to MLT according to the configured render pipeline.
     */
private slots:
    void resizeGL(int width, int height);
    void updateTexture(GLuint yName, GLuint uName, GLuint vName);
    void paintGL();
    void onFrameDisplayed(const SharedFrame &frame);
    void refresh();

protected:
    QMutex m_contextSharedAccess;
    QOffscreenSurface m_offscreenSurface;
    SharedFrame m_sharedFrame;
    QOpenGLContext *m_shareContext;

    bool acquireSharedFrameTextures();
    void bindShaderProgram();
    void createGPUAccelFragmentProg();
    void createShader();
    void createYUVTextureProjectFragmentProg();
    void disableGPUAccel();
    void releaseSharedFrameTextures();

    // pipeline A - YUV gl texture w/o GPU filter acceleration
    // pipeline B - YUV gl texture multithreaded w/o GPU filter acceleration
    // pipeline C - RGB gl texture multithreaded w/ GPU filter acceleration and no sync
    // pipeline D - RGB gl texture multithreaded w/ GPU filter acceleration and sync
    bool m_openGLSync;
    bool initGPUAccelSync();

    // pipeline C & D
    bool initGPUAccel();
    bool onlyGLESGPUAccel() const;

    // pipeline A & B & C & D
    // not null iff D
    ClientWaitSync_fp m_ClientWaitSync;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
};

class RenderThread : public QThread
{
    Q_OBJECT
public:
    RenderThread(thread_function_t function, void *data, QOpenGLContext *context, QSurface *surface);
    ~RenderThread() override;

protected:
    void run() override;

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
    explicit FrameRenderer(QOpenGLContext *shareContext, QSurface *surface, GLWidget::ClientWaitSync_fp clientWaitSync);
    ~FrameRenderer() override;
    QSemaphore *semaphore() { return &m_semaphore; }
    QOpenGLContext *context() const { return m_context; }
    Q_INVOKABLE void showFrame(Mlt::Frame frame);
    Q_INVOKABLE void showGLFrame(Mlt::Frame frame);
    Q_INVOKABLE void showGLNoSyncFrame(Mlt::Frame frame);

public slots:
    void cleanup();

signals:
    void textureReady(GLuint yName, GLuint uName = 0, GLuint vName = 0);
    void frameDisplayed(const SharedFrame &frame);

private:
    QSemaphore m_semaphore;
    SharedFrame m_displayFrame;
    QOpenGLContext *m_context;
    QSurface *m_surface;
    GLWidget::ClientWaitSync_fp m_ClientWaitSync;

    void pipelineSyncToFrame(Mlt::Frame &);

public:
    GLuint m_renderTexture[3];
    GLuint m_displayTexture[3];
    QOpenGLFunctions_3_2_Core *m_gl32;
    bool sendAudioForAnalysis;
};
#endif
