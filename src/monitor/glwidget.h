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

#include <QMutex>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QQuickView>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QRect>
#include <QSemaphore>
#include <QThread>
#include <QTimer>

#include "bin/model/markerlistmodel.hpp"
#include "definitions.h"
#include "scopes/sharedframe.h"

class QOpenGLFunctions_3_2_Core;

namespace Mlt {
class Filter;
class Producer;
class Consumer;
class Profile;
}

class RenderThread;
class FrameRenderer;
class MonitorProxy;

typedef void *(*thread_function_t)(void *);

class GLWidget : public QQuickView, protected QOpenGLFunctions
{
    Q_OBJECT
    Q_PROPERTY(QRect rect READ rect NOTIFY rectChanged)
    Q_PROPERTY(float zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(QPoint offset READ offset NOTIFY offsetChanged)

public:
    friend class MonitorController;
    friend class Monitor;

    GLWidget(int id, QObject *parent = nullptr);
    ~GLWidget();

    int requestedSeekPosition;
    void createThread(RenderThread **thread, thread_function_t function, void *data);
    void startGlsl();
    void stopGlsl();
    void clear();
    int reconfigureMulti(const QString &params, const QString &path, Mlt::Profile *profile);
    void stopCapture();
    int reconfigure(Mlt::Profile *profile = nullptr);
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
    void reloadProfile();
    void lockMonitor();
    void releaseMonitor();
    int realTime() const;
    void setAudioThumb(int channels = 0, const QVariantList &audioCache = QList<QVariant>());
    int droppedFrames() const;
    void resetDrops();
    bool checkFrameNumber(int pos);
    /** @brief Return current timeline position */
    int getCurrentPos() const;
    /** @brief Requests a monitor refresh */
    void requestRefresh();
    void setRulerInfo(int duration, std::shared_ptr<MarkerListModel> model = nullptr);
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

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    /** @brief Update producer, should ONLY be called from monitor */
    int setProducer(Mlt::Producer *producer, bool isActive, int position = -1);

public slots:
    void seek(int pos);
    void requestSeek();
    void setZoom(float zoom);
    void setOffsetX(int x, int max);
    void setOffsetY(int y, int max);
    void slotSwitchAudioOverlay(bool enable);
    void slotZoomScene(double value);
    void initializeGL();
    void releaseAnalyse();
    void switchPlay(bool play, double speed = 1.0);

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
    void mouseSeek(int eventDelta, uint modifiers);
    void startDrag();
    void analyseFrame(const QImage &);
    void audioSamplesSignal(const audioShortVector &, int, int, int);
    void showContextMenu(const QPoint &);
    void lockMonitor(bool);
    void passKeyEvent(QKeyEvent *);
    void panView(const QPoint &diff);
    void seekPosition(int);
    void activateMonitor();

protected:
    Mlt::Filter *m_glslManager;
    Mlt::Consumer *m_consumer;
    Mlt::Producer *m_producer;
    Mlt::Profile *m_monitorProfile;
    QMutex m_mutex;

private:
    int m_id;
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
    FrameRenderer *m_frameRenderer;
    int m_projectionLocation;
    int m_modelViewLocation;
    int m_vertexLocation;
    int m_texCoordLocation;
    int m_colorspaceLocation;
    int m_textureLocation[3];
    QTimer m_refreshTimer;
    float m_zoom;
    bool m_openGLSync;
    bool m_sendFrame;
    bool m_isZoneMode;
    bool m_isLoopMode;
    SharedFrame m_sharedFrame;
    QPoint m_offset;
    QOffscreenSurface m_offscreenSurface;
    QOpenGLContext *m_shareContext;
    bool m_audioWaveDisplayed;
    MonitorProxy *m_proxy;
    QScopedPointer<Mlt::Producer> m_blackClip;
    static void on_frame_show(mlt_consumer, void *self, mlt_frame frame);
    static void on_gl_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr);
    static void on_gl_nosync_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr);
    void createAudioOverlay(bool isAudio);
    void removeAudioOverlay();
    void adjustAudioOverlay(bool isAudio);
    QOpenGLFramebufferObject *m_fbo;
    void refreshSceneLayout();
    void resetZoneMode();

private slots:
    void resizeGL(int width, int height);
    void updateTexture(GLuint yName, GLuint uName, GLuint vName);
    void paintGL();
    void onFrameDisplayed(const SharedFrame &frame);
    void refresh();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    void createShader();
};

class RenderThread : public QThread
{
    Q_OBJECT
public:
    RenderThread(thread_function_t function, void *data, QOpenGLContext *context, QSurface *surface);
    ~RenderThread();

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
    explicit FrameRenderer(QOpenGLContext *shareContext, QSurface *surface);
    ~FrameRenderer();
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
    void audioSamplesSignal(const audioShortVector &, int, int, int);

private:
    QSemaphore m_semaphore;
    SharedFrame m_displayFrame;
    QOpenGLContext *m_context;
    QSurface *m_surface;

public:
    GLuint m_renderTexture[3];
    GLuint m_displayTexture[3];
    QOpenGLFunctions_3_2_Core *m_gl32;
    bool sendAudioForAnalysis;
};

class MonitorProxy : public QObject
{
    Q_OBJECT
    // Q_PROPERTY(int consumerPosition READ consumerPosition NOTIFY consumerPositionChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int seekPosition READ seekPosition WRITE setSeekPosition NOTIFY seekPositionChanged)
    Q_PROPERTY(int zoneIn READ zoneIn WRITE setZoneIn NOTIFY zoneChanged)
    Q_PROPERTY(int zoneOut READ zoneOut WRITE setZoneOut NOTIFY zoneChanged)
    Q_PROPERTY(int rulerHeight READ rulerHeight NOTIFY rulerHeightChanged)
    Q_PROPERTY(QString markerComment READ markerComment NOTIFY markerCommentChanged)
public:
    MonitorProxy(GLWidget *parent)
        : QObject(parent)
        , q(parent)
        , m_position(0)
        , m_seekPosition(-1)
        , m_zoneIn(0)
        , m_zoneOut(-1)
        , m_rulerHeight(QFontMetrics(QApplication::font()).lineSpacing() * 0.7)
    {
    }
    int seekPosition() const { return m_seekPosition; }
    int position() const { return m_position; }
    int rulerHeight() const { return m_rulerHeight; }
    QString markerComment() const { return m_markerComment; }
    Q_INVOKABLE void requestSeekPosition(int pos)
    {
        q->activateMonitor();
        m_seekPosition = pos;
        emit seekPositionChanged();
        emit seekRequestChanged();
    }
    void setPosition(int pos)
    {
        m_position = pos;
        emit positionChanged();
    }
    void setMarkerComment(const QString &comment)
    {
        if (m_markerComment == comment) {
            return;
        }
        m_markerComment = comment;
        emit markerCommentChanged();
    }
    void setSeekPosition(int pos)
    {
        m_seekPosition = pos;
        emit seekPositionChanged();
    }
    void pauseAndSeek(int pos)
    {
        q->switchPlay(false);
        m_seekPosition = pos;
        emit seekPositionChanged();
    }
    int zoneIn() const { return m_zoneIn; }
    int zoneOut() const { return m_zoneOut; }
    void setZoneIn(int pos)
    {
        if (m_zoneIn > 0) {
            emit removeSnap(m_zoneIn);
        }
        m_zoneIn = pos;
        if (pos > 0) {
            emit addSnap(pos);
        }
        emit zoneChanged();
    }
    void setZoneOut(int pos)
    {
        if (m_zoneOut > 0) {
            emit removeSnap(m_zoneOut);
        }
        m_zoneOut = pos;
        if (pos > 0) {
            emit addSnap(pos);
        }
        emit zoneChanged();
    }
    Q_INVOKABLE void setZone(int in, int out)
    {
        if (m_zoneIn > 0) {
            emit removeSnap(m_zoneIn);
        }
        if (m_zoneOut > 0) {
            emit removeSnap(m_zoneOut);
        }
        m_zoneIn = in;
        m_zoneOut = out;
        if (m_zoneIn > 0) {
            emit addSnap(m_zoneIn);
        }
        if (m_zoneOut > 0) {
            emit addSnap(m_zoneOut);
        }
        emit zoneChanged();
    }
    void setZone(QPoint zone)
    {
        setZone(zone.x(), zone.y());
    }
    void resetZone()
    {
        m_zoneIn = 0;
        m_zoneOut = -1;
    }
    QPoint zone() const { return QPoint(m_zoneIn, m_zoneOut); }
signals:
    void positionChanged();
    void seekPositionChanged();
    void seekRequestChanged();
    void zoneChanged();
    void markerCommentChanged();
    void rulerHeightChanged();
    void addSnap(int);
    void removeSnap(int);

private:
    GLWidget *q;
    int m_position;
    int m_seekPosition;
    int m_zoneIn;
    int m_zoneOut;
    int m_rulerHeight;
    QString m_markerComment;
};

#endif
