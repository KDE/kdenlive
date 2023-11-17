/*
    SPDX-FileCopyrightText: 2011-2014 Meltytech LLC
    SPDX-FileCopyrightText: 2011-2014 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QFont>
#include <QMutex>
#include <QOffscreenSurface>
#include <QOpenGLContext>

#include <QQuickWidget>
#include <QRect>
#include <QSemaphore>
#include <QThread>
#include <QTimer>

#include "bin/model/markerlistmodel.hpp"
#include "definitions.h"
#include "kdenlivesettings.h"
#include "scopes/sharedframe.h"

#include <mlt++/MltEvent.h>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProfile.h>

namespace Mlt {
class Filter;
class Producer;
class Consumer;
} // namespace Mlt

class RenderThread;
class FrameRenderer;
class MonitorProxy;
class MarkerSortModel;

typedef void *(*thread_function_t)(void *);

/** @class VideoWidget
 *  @brief QQuickView that renders an .
 *
 * Creates an MLT consumer and renders a GL view from the consumer. This pipeline is one of:
 *
 *    A. YUV gl texture w/o GPU filter acceleration
 *    B. YUV gl texture multithreaded w/o GPU filter acceleration
 *    C. RGB gl texture multithreaded w/ GPU filter acceleration and no sync
 *    D. RGB gl texture multithreaded w/ GPU filter acceleration and sync
 */
class VideoWidget : public QQuickWidget
{
    Q_OBJECT
    Q_PROPERTY(QRect rect READ rect NOTIFY rectChanged)
    Q_PROPERTY(float zoom READ zoom NOTIFY zoomChanged)

public:
    friend class MonitorController;
    friend class Monitor;
    friend class MonitorProxy;

    VideoWidget(int id, QObject *parent = nullptr);
    virtual ~VideoWidget();

    int requestedSeekPosition;
    void createThread(RenderThread **thread, thread_function_t function, void *data);
    void startGlsl();
    void stopGlsl();
    void clear();
    void stopCapture();

    int displayWidth() const { return m_rect.width(); }
    void updateAudioForAnalysis();
    int displayHeight() const { return m_rect.height(); }

    QObject *videoWidget() { return this; }
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
    /** @brief delete and rebuild consumer, for example when external display is switched */
    void resetConsumer(bool fullReset);
    void lockMonitor();
    void releaseMonitor();
    int droppedFrames() const;
    void resetDrops();
    bool checkFrameNumber(int pos, bool isPlaying);
    /** @brief Return current timeline position */
    int getCurrentPos() const;
    /** @brief Requests a monitor refresh */
    void requestRefresh();
    void setRulerInfo(int duration, const std::shared_ptr<MarkerSortModel> &model = nullptr);
    MonitorProxy *getControllerProxy();
    bool playZone(bool loop = false);
    bool loopClip(QPoint inOut);
    void startConsumer();
    void stop();
    int rulerHeight() const;
    /** @brief return current play producer's playing speed */
    double playSpeed() const;
    /** @brief Purge and restart consumer */
    void restart();
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
    /** @brief Returns true if consumer is initialized */
    bool isReady() const;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /** @brief Update producer, should ONLY be called from monitor
     * @param producer
     * @param isActive
     * @param position If == 0 producer position will be used.
     * If == -1 consumer position will be used if possible.
     * If == -2 position will not be set.
     */
    int setProducer(const std::shared_ptr<Mlt::Producer> &producer, bool isActive, int position);
    int setProducer(const QString &file);
    QString frameToTime(int frames) const;

public Q_SLOTS:
    virtual void initialize();
    virtual void beforeRendering(){};
    virtual void renderVideo();
    virtual void onFrameDisplayed(const SharedFrame &frame);
    void requestSeek(int position, bool noAudioScrub = false);
    void setZoom(float zoom, bool force = false);
    void setOffsetX(int x, int max);
    void setOffsetY(int y, int max);
    void slotZoom(bool zoomIn);
    void releaseAnalyse();
    bool switchPlay(bool play, double speed = 1.0);
    void reloadProfile();
    /** @brief Update MLT's consumer scaling
     *  @returns true is scaling was changed
     */
    bool updateScaling();

Q_SIGNALS:
    void frameDisplayed(const SharedFrame &frame);
    void frameRendered(int pos);
    void imageReady();
    void dragStarted();
    void seekTo(int x);
    void gpuNotSupported();
    void started();
    void paused();
    void playing();
    void rectChanged();
    void zoomChanged(float zoomRatio);
    void monitorPlay();
    void switchFullScreen(bool minimizeOnly = false);
    void mouseSeek(int eventDelta, uint modifiers);
    void startDrag();
    void analyseFrame(const QImage &);
    void showContextMenu(const QPoint &);
    void lockMonitor(bool);
    void passKeyEvent(QKeyEvent *);
    void panView(const QPoint &diff);

protected:
    // TODO: MTL has lock/unlock of individual nodes. Use those.
    // keeping this for refactoring ease.
    QMutex m_mltMutex;
    std::shared_ptr<Mlt::Consumer> m_consumer;
    std::shared_ptr<Mlt::Producer> m_producer;
    int m_id;
    /** @brief The height of the qml ruler */
    int m_rulerHeight;
    /** @brief The height of the qml ruler and audio thumbs */
    int m_displayRulerHeight;
    int m_maxTextureSize;
    SharedFrame m_sharedFrame;
    QMutex m_mutex;

    /** @brief adjust monitor ruler size (for example if we want to display audio thumbs permanently) */
    void updateRulerHeight(int addedHeight);

private:
    QRect m_rect;
    QRect m_effectRect;
    QPoint m_panStart;
    QPoint m_dragStart;
    QSemaphore m_initSem;
    QSemaphore m_analyseSem;
    bool m_isInitialized;
    bool m_qmlEvent;
    int m_maxProducerPosition;
    int m_bckpMax;
    std::unique_ptr<Mlt::Filter> m_glslManager;
    std::unique_ptr<Mlt::Event> m_threadStartEvent;
    std::unique_ptr<Mlt::Event> m_threadStopEvent;
    std::unique_ptr<Mlt::Event> m_threadCreateEvent;
    std::unique_ptr<Mlt::Event> m_threadJoinEvent;
    std::unique_ptr<Mlt::Event> m_displayEvent;
    FrameRenderer *m_frameRenderer;
    QTimer m_refreshTimer;
    float m_zoom;
    QSize m_profileSize;
    int m_colorSpace;
    double m_dar;
    bool m_sendFrame;
    bool m_isZoneMode;
    bool m_isLoopMode;
    int m_loopIn;
    int m_loopOut;
    QPoint m_offset;
    MonitorProxy *m_proxy;
    std::unique_ptr<RenderThread> m_renderThread;
    std::shared_ptr<Mlt::Producer> m_blackClip;
    static void on_frame_show(mlt_consumer, VideoWidget *widget, mlt_event_data);
    static void on_frame_render(mlt_consumer, VideoWidget *widget, mlt_frame frame);
    /*static void on_gl_frame_show(mlt_consumer, VideoWidget *widget, mlt_event_data data);
    static void on_gl_nosync_frame_show(mlt_consumer, VideoWidget *widget, mlt_event_data data);*/

    void refreshSceneLayout();
    void resetZoneMode();
    bool initGPUAccel();
    void disableGPUAccel();
    /** @brief Restart consumer, keeping preview scaling settings */
    bool restartConsumer();

    /* OpenGL context management. Interfaces to MLT according to the configured render pipeline.
     */
private Q_SLOTS:
    void resizeVideo(int width, int height);
    int reconfigure();
    void refresh();
    void switchRecordState(bool on);

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
    RenderThread(thread_function_t function, void *data);
    ~RenderThread();

protected:
    void run() override;

private:
    thread_function_t m_function;
    void *m_data;
    std::unique_ptr<QOpenGLContext> m_context;
    std::unique_ptr<QOffscreenSurface> m_surface;
};

class FrameRenderer : public QThread
{
    Q_OBJECT
public:
    explicit FrameRenderer();
    ~FrameRenderer();
    QSemaphore *semaphore() { return &m_semaphore; }
    SharedFrame getDisplayFrame();
    Q_INVOKABLE void showFrame(Mlt::Frame frame);
    void requestImage();
    QImage image() const { return m_image; }

Q_SIGNALS:
    void frameDisplayed(const SharedFrame &frame);
    void imageReady();

private:
    QSemaphore m_semaphore;
    SharedFrame m_displayFrame;
    bool m_imageRequested;
    QImage m_image;
};
