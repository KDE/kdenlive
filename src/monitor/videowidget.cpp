/*
    SPDX-FileCopyrightText: 2011-2016 Meltytech LLC
    SPDX-FileCopyrightText: Dan Dennedy <dan@dennedy.org>
    SPDX-FileCopyrightText: Jean-Baptiste Mardelle

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

    GL shader based on BSD licensed code from Peter Bengtsson:
    https://www.fourcc.org/source/YUV420P-OpenGL-GLSLang.c
    SPDX-FileCopyrightText: 2004 Peter Bengtsson

    SPDX-License-Identifier: BSD-3-Clause
*/

#include <QApplication>
#include <QFontDatabase>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_2_Core>
#include <QPainter>
#include <QQmlContext>
#include <QQuickItem>
#include <QtGlobal>
#include <memory>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "kdeclarative_version.h"
#endif
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0) || KDECLARATIVE_VERSION > QT_VERSION_CHECK(5, 98, 0)
#include <KQuickIconProvider>
#else
#include <KDeclarative/KDeclarative>
#endif
#include <KLocalizedContext>
#include <KLocalizedString>
#include <KMessageBox>

#include "bin/model/markersortmodel.h"
#include "core.h"
#include "monitorproxy.h"
#include "profiles/profilemodel.hpp"
#include "timeline2/view/qml/timelineitems.h"
#include "timeline2/view/qmltypes/thumbnailprovider.h"
#include "videowidget.h"
#include <lib/localeHandling.h>
#include <mlt++/Mlt.h>

#ifndef GL_UNPACK_ROW_LENGTH
#ifdef GL_UNPACK_ROW_LENGTH_EXT
#define GL_UNPACK_ROW_LENGTH GL_UNPACK_ROW_LENGTH_EXT
#else
#error GL_UNPACK_ROW_LENGTH undefined
#endif
#endif

#if 1
#define check_error(fn)                                                                                                                                        \
    {                                                                                                                                                          \
    }
#else
#define check_error(fn)                                                                                                                                        \
    {                                                                                                                                                          \
        int err = fn->glGetError();                                                                                                                            \
        if (err != GL_NO_ERROR) {                                                                                                                              \
            qCritical(KDENLIVE_LOG) << "GL error" << hex << err << dec << "at" << __FILE__ << ":" << __LINE__;                                                 \
        }                                                                                                                                                      \
    }
#endif

#ifndef GL_TIMEOUT_IGNORED
#define GL_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFFull
#endif

using namespace Mlt;

VideoWidget::VideoWidget(int id, QObject *parent)
    : QQuickWidget((QWidget *)parent)
    , sendFrameForAnalysis(false)
    , m_consumer(nullptr)
    , m_producer(nullptr)
    , m_id(id)
    , m_rulerHeight(int(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5))
    , m_sendFrame(false)
    , m_analyseSem(1)
    , m_zoom(1.0f)
    , m_profileSize(1920, 1080)
    , m_isInitialized(false)
    , m_initSem(0)
    , m_maxProducerPosition(0)
    , m_glslManager(nullptr)
    , m_frameRenderer(nullptr)
    , m_colorSpace(601)
    , m_dar(1.78)
    , m_isZoneMode(false)
    , m_isLoopMode(false)
    , m_loopIn(0)
    , m_offset(QPoint(0, 0))
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0) || KDECLARATIVE_VERSION > QT_VERSION_CHECK(5, 98, 0)
    engine()->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
#else
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
#endif
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));
    qRegisterMetaType<Mlt::Frame>("Mlt::Frame");
    qRegisterMetaType<SharedFrame>("SharedFrame");
    setAcceptDrops(true);
    setClearColor(KdenliveSettings::window_background());

    if (m_id == Kdenlive::ClipMonitor && !(KdenliveSettings::displayClipMonitorInfo() & 0x01)) {
        m_rulerHeight = 0;
    } else if (!(KdenliveSettings::displayProjectMonitorInfo() & 0x01)) {
        m_rulerHeight = 0;
    }
    m_displayRulerHeight = m_rulerHeight;
    if (!initGPUAccel()) {
        m_glslManager.reset();
    }
    quickWindow()->setPersistentGraphics(true);
    quickWindow()->setPersistentSceneGraph(true);
    setResizeMode(QQuickWidget::SizeRootObjectToView);

    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(10);
    m_blackClip.reset(new Mlt::Producer(pCore->getProjectProfile(), "color:0"));
    m_blackClip->set("mlt_image_format", "rgba");
    m_blackClip->set("kdenlive:id", "black");
    m_blackClip->set("out", 3);
    connect(&m_refreshTimer, &QTimer::timeout, this, &VideoWidget::refresh);
    m_producer = m_blackClip;
    rootContext()->setContextProperty("markersModel", nullptr);
    connect(pCore.get(), &Core::switchTimelineRecord, this, &VideoWidget::switchRecordState);

    registerTimelineItems();
    m_proxy = new MonitorProxy(this);
    rootContext()->setContextProperty("controller", m_proxy);
    engine()->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
}

VideoWidget::~VideoWidget()
{
    stop();
    if (m_frameRenderer && m_frameRenderer->isRunning()) {
        m_frameRenderer->quit();
        m_frameRenderer->wait();
        m_frameRenderer->deleteLater();
    }
    m_blackClip.reset();
}

void VideoWidget::updateAudioForAnalysis()
{
    if (m_frameRenderer) {
        m_frameRenderer->requestImage();
    }
}

void VideoWidget::initialize()
{
    m_frameRenderer = new FrameRenderer();
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &VideoWidget::onFrameDisplayed, Qt::QueuedConnection);
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &VideoWidget::frameDisplayed, Qt::QueuedConnection);
    connect(m_frameRenderer, SIGNAL(imageReady()), SIGNAL(imageReady()));
    m_initSem.release();
    m_isInitialized = true;
}

void VideoWidget::renderVideo()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (m_sendFrame) {
        QImage img = image();
        Q_EMIT analyseFrame(img);
        m_sendFrame = false;
    }
#endif
}

QImage VideoWidget::image() const
{
    SharedFrame frame = m_frameRenderer->getDisplayFrame();
    if (frame.is_valid()) {
        const uint8_t *image = frame.get_image(mlt_image_rgba);
        if (image) {
            int width = frame.get_image_width();
            int height = frame.get_image_height();
            QImage temp(image, width, height, QImage::Format_RGBA8888);
            return temp.copy();
        }
    }
    return QImage();
}

const QStringList VideoWidget::getGPUInfo()
{
    return {};
}

void VideoWidget::resizeVideo(int width, int height)
{
    int x, y, w, h;
    height -= m_displayRulerHeight;
    double this_aspect = double(width) / height;

    // Special case optimization to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if (int(this_aspect * 1000) == int(m_dar * 1000)) {
        w = width;
        h = height;
    }
    // Use OpenGL to normalise sample aspect ratio
    else if (height * m_dar > width) {
        w = width;
        h = int(width / m_dar);
    } else {
        w = int(height * m_dar);
        h = height;
    }
    x = (width - w) / 2;
    y = (height - h) / 2;
    m_rect.setRect(x, y, w, h);
    QQuickItem *rootQml = rootObject();
    if (rootQml) {
        QSize s = pCore->getCurrentFrameSize();
        double scalex = double(m_rect.width() * m_zoom) / s.width();
        double scaley = double(m_rect.height() * m_zoom) / s.height();
        rootQml->setProperty("center", m_rect.center());
        rootQml->setProperty("scalex", scalex);
        rootQml->setProperty("scaley", scaley);
        if (rootQml->objectName() == QLatin1String("rootsplit")) {
            // Adjust splitter pos
            rootQml->setProperty("splitterPos", x + (rootQml->property("percentage").toDouble() * w));
        }
    }
    Q_EMIT rectChanged();
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    QQuickWidget::resizeEvent(event);
    if (refreshZoom) {
        QMetaObject::invokeMethod(this, "forceRefreshZoom", Qt::QueuedConnection);
        refreshZoom = false;
    }
    resizeVideo(event->size().width(), event->size().height());
}

void VideoWidget::forceRefreshZoom()
{
    setZoom(m_zoom, true);
}

void VideoWidget::clear()
{
    stopGlsl();
    quickWindow()->update();
}

void VideoWidget::releaseAnalyse()
{
    m_analyseSem.release();
}

bool VideoWidget::initGPUAccel()
{
    if (!KdenliveSettings::gpu_accel()) return false;

    m_glslManager.reset(new Mlt::Filter(pCore->getProjectProfile(), "glsl.manager"));
    return m_glslManager->is_valid();
}

void VideoWidget::disableGPUAccel()
{
    m_glslManager.reset();
    KdenliveSettings::setGpu_accel(false);
    // Need to destroy MLT global reference to prevent filters from trying to use GPU.
    mlt_properties_set_data(mlt_global_properties(), "glslManager", nullptr, 0, nullptr, nullptr);
    Q_EMIT gpuNotSupported();
}

void VideoWidget::slotZoom(bool zoomIn)
{
    if (zoomIn) {
        if (m_zoom < 12.0f) {
            setZoom(m_zoom * 1.2);
        }
    } else if (m_zoom > 0.2f) {
        setZoom(m_zoom / 1.2f);
    }
}

void VideoWidget::updateRulerHeight(int addedHeight)
{
    m_displayRulerHeight =
        m_rulerHeight > 0 ? int(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5) + addedHeight : 0;
    resizeVideo(width(), height());
}

bool VideoWidget::isReady() const
{
    return m_consumer != nullptr;
}

void VideoWidget::requestSeek(int position, bool noAudioScrub)
{
    m_producer->seek(position);
    if (!qFuzzyIsNull(m_producer->get_speed())) {
        m_consumer->purge();
    }
    if (!m_consumer) {
        return;
    }
    restartConsumer();
    m_consumer->set("refresh", 1);
    if (KdenliveSettings::audio_scrub() && !noAudioScrub) {
        m_consumer->set("scrub_audio", 1);
    } else {
        m_consumer->set("scrub_audio", 0);
    }
}

void VideoWidget::requestRefresh()
{
    if (m_producer && qFuzzyIsNull(m_producer->get_speed())) {
        m_consumer->set("scrub_audio", 0);
        m_refreshTimer.start();
    }
}

QString VideoWidget::frameToTime(int frames) const
{
    return m_consumer ? m_consumer->frames_to_time(frames, mlt_time_smpte_df) : QStringLiteral("-");
}

void VideoWidget::refresh()
{
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mltMutex);
    if (m_consumer) {
        restartConsumer();
        m_consumer->set("refresh", 1);
    }
}

bool VideoWidget::checkFrameNumber(int pos, bool isPlaying)
{
    const double speed = m_producer->get_speed();
    m_proxy->positionFromConsumer(pos, isPlaying);
    if (m_isLoopMode || m_isZoneMode) {
        // not sure why we need to check against pos + 1 but otherwise the
        // playback shows one frame after the intended out frame
        if (isPlaying && pos + 1 >= m_loopOut) {
            m_consumer->purge();
            if (!m_isLoopMode) {
                // end play zone mode
                m_isZoneMode = false;
                m_producer->set_speed(0);
                m_proxy->setSpeed(0);
                m_consumer->set("refresh", 0);
                m_proxy->setPosition(m_loopOut);
                m_producer->seek(m_loopOut);
                m_loopOut = 0;
                return false;
            }
            m_producer->seek(m_isZoneMode ? m_proxy->zoneIn() : m_loopIn);
            m_producer->set_speed(1.0);
            m_proxy->setSpeed(1.);
            m_consumer->set("refresh", 1);
            return true;
        }
        return true;
    } else if (isPlaying) {
        if (pos > m_maxProducerPosition - 2 && !(speed < 0.)) {
            // Playing past last clip, pause
            m_producer->set_speed(0);
            m_proxy->setSpeed(0);
            m_consumer->set("refresh", 0);
            m_consumer->purge();
            m_proxy->setPosition(qMax(0, m_maxProducerPosition));
            m_producer->seek(qMax(0, m_maxProducerPosition));
            return false;
        } else if (pos <= 0 && speed < 0.) {
            // rewinding reached 0, pause
            m_producer->set_speed(0);
            m_proxy->setSpeed(0);
            m_consumer->set("refresh", 0);
            m_consumer->purge();
            m_proxy->setPosition(0);
            m_producer->seek(0);
            return false;
        }
    }
    return isPlaying;
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if ((rootObject() != nullptr) && rootObject()->property("captureRightClick").toBool() && !(event->modifiers() & Qt::ControlModifier) &&
        !(event->buttons() & Qt::MiddleButton)) {
        event->ignore();
        QQuickWidget::mousePressEvent(event);
        return;
    }
    QQuickWidget::mousePressEvent(event);
    // For some reason, on Qt6 in mouseReleaseEvent, the event is always accepted, so use this m_qmlEvent bool to track if the event is accepted in qml
    m_qmlEvent = event->isAccepted();
    if (rootObject() != nullptr && rootObject()->property("captureRightClick").toBool()) {
        // The event has been handled in qml
        m_swallowDrop = true;
    }
    // event->accept();
    if ((event->button() & Qt::LeftButton) != 0u) {
        if ((event->modifiers() & Qt::ControlModifier) != 0u) {
            // Pan view
            m_panStart = event->pos();
            setCursor(Qt::ClosedHandCursor);
        } else {
            m_dragStart = event->pos();
        }
    } else if ((event->button() & Qt::RightButton) != 0u) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        Q_EMIT showContextMenu(event->globalPos());
#else
        Q_EMIT showContextMenu(event->globalPosition().toPoint());
#endif
    } else if ((event->button() & Qt::MiddleButton) != 0u) {
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QQuickWidget::mouseReleaseEvent(event);
    /*if (m_dragStart.isNull() && m_panStart.isNull() && (rootObject() != nullptr) && rootObject()->objectName() != QLatin1String("root") &&
        !(event->modifiers() & Qt::ControlModifier)) {
        event->accept();
        qDebug()<<"::::::: MOUSE RELEASED B IGNORED";
        return;
    }*/
    if (event->modifiers() & Qt::ControlModifier || m_qmlEvent) {
        event->accept();
        return;
    }
    if (!m_dragStart.isNull() && m_panStart.isNull() && ((event->button() & Qt::LeftButton) != 0u) && !m_swallowDrop) {
        event->accept();
        Q_EMIT monitorPlay();
    }
    m_swallowDrop = false;
    m_dragStart = QPoint();
    m_panStart = QPoint();
    setCursor(Qt::ArrowCursor);
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if ((rootObject() != nullptr) && rootObject()->objectName() != QLatin1String("root") && !(event->modifiers() & Qt::ControlModifier) &&
        !(event->buttons() & Qt::MiddleButton)) {
        event->ignore();
        QQuickWidget::mouseMoveEvent(event);
        return;
    }
    QQuickWidget::mouseMoveEvent(event);
    if (!(event->buttons() & Qt::LeftButton)) {
        event->accept();
        return;
    }
    if (!m_panStart.isNull()) {
        Q_EMIT panView(m_panStart - event->pos());
        m_panStart = event->pos();
        event->accept();
        return;
    }

    if (!event->isAccepted() && !m_dragStart.isNull() && (event->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragStart = QPoint();
        Q_EMIT startDrag();
    }
    event->accept();
}

void VideoWidget::keyPressEvent(QKeyEvent *event)
{
    QQuickWidget::keyPressEvent(event);
    if (!event->isAccepted()) {
        Q_EMIT passKeyEvent(event);
    }
}

void VideoWidget::createThread(RenderThread **thread, thread_function_t function, void *data)
{
#ifdef Q_OS_WIN
    // On Windows, MLT event consumer-thread-create is fired from the Qt main thread.
    while (!m_isInitialized) {
        qApp->processEvents();
    }
#else
    if (!m_isInitialized) {
        m_initSem.acquire();
    }
#endif
    if (!m_renderThread) {
        m_renderThread.reset(new RenderThread(function, data));
        (*thread) = m_renderThread.get();
        (*thread)->start();
    } else {
        m_renderThread->start();
    }
}

static void onThreadCreate(mlt_properties owner, VideoWidget *self, mlt_event_data data)
{
    Q_UNUSED(owner)
    auto threadData = (mlt_event_data_thread *)Mlt::EventData(data).to_object();
    if (threadData) {
        auto renderThread = (RenderThread *)threadData->thread;
        self->createThread(&renderThread, threadData->function, threadData->data);
        // TODO: useless ?
        // self->lockMonitor();
    }
}

static void onThreadJoin(mlt_properties owner, VideoWidget *self, mlt_event_data data)
{
    Q_UNUSED(owner)
    Q_UNUSED(self)
    auto threadData = (mlt_event_data_thread *)Mlt::EventData(data).to_object();
    if (threadData) {
        auto renderThread = (RenderThread *)threadData->thread;
        if (renderThread) {
            renderThread->quit();
            renderThread->wait();
            delete renderThread;
            // TODO: useless ?
            // self->releaseMonitor();
        }
    }
}

void VideoWidget::startGlsl()
{
    // C & D
    if (m_glslManager) {
        // clearFrameRenderer();
        m_glslManager->fire_event("init glsl");
        if (m_glslManager->get_int("glsl_supported") == 0) {
            disableGPUAccel();
        } else {
            Q_EMIT started();
        }
    }
}

static void onThreadStarted(mlt_properties owner, VideoWidget *self, mlt_event_data)
{
    Q_UNUSED(owner)
    self->startGlsl();
}

void VideoWidget::releaseMonitor()
{
    Q_EMIT lockMonitor(false);
}

void VideoWidget::lockMonitor()
{
    Q_EMIT lockMonitor(true);
}

void VideoWidget::stopGlsl()
{
    if (m_consumer) {
        m_consumer->purge();
    }
}

static void onThreadStopped(mlt_properties owner, VideoWidget *self, mlt_event_data)
{
    Q_UNUSED(owner)
    self->stopGlsl();
}

int VideoWidget::setProducer(const QString &file)
{
    if (m_producer) {
        m_producer.reset();
    }
    m_producer = std::make_shared<Mlt::Producer>(new Mlt::Producer(pCore->getProjectProfile(), nullptr, file.toUtf8().constData()));
    if (!m_producer || !m_producer->is_valid()) {
        m_producer.reset();
        m_producer = m_blackClip;
    }
    if (m_consumer) {
        // m_consumer->stop();
        if (!m_consumer->is_stopped()) {
            m_consumer->stop();
        }
    }
    int error = reconfigure();
    if (error == 0) {
        // The profile display aspect ratio may have changed.
        resizeVideo(width(), height());
        startConsumer();
    }
    return error;
}

int VideoWidget::setProducer(const std::shared_ptr<Mlt::Producer> &producer, bool isActive, int position)
{
    int error = 0;
    QString currentId;
    int consumerPosition = 0;
    if (m_producer) {
        currentId = m_producer->parent().get("kdenlive:id");
    }
    if (m_consumer) {
        consumerPosition = m_consumer->position();
    }
    stop();
    if (producer) {
        m_producer = producer;
    } else {
        if (currentId == QLatin1String("black")) {
            return 0;
        }
        m_producer = m_blackClip;
        // Reset markersModel
        rootContext()->setContextProperty("markersModel", nullptr);
    }
    m_producer->set_speed(0);
    m_proxy->setSpeed(0);
    error = reconfigure();
    if (error == 0) {
        // The profile display aspect ratio may have changed.
        resizeVideo(width(), height());
    } else {
        return error;
    }
    if (!m_consumer) {
        return error;
    }
    if (position == -1 && m_producer->parent().get("kdenlive:id") == currentId) {
        position = consumerPosition;
    }
    if (isActive) {
        startConsumer();
        if (position != -2) {
            m_proxy->resetPosition();
        }
    }
    m_consumer->set("scrub_audio", 0);
    if (position != -2) {
        m_proxy->setPositionAdvanced(position > 0 ? position : m_producer->position(), true);
    }
    return error;
}

int VideoWidget::droppedFrames() const
{
    return (m_consumer ? m_consumer->get_int("drop_count") : 0);
}

void VideoWidget::resetDrops()
{
    if (m_consumer) {
        m_consumer->set("drop_count", 0);
    }
}

void VideoWidget::stopCapture()
{
    if (strcmp(m_consumer->get("mlt_service"), "multi") == 0) {
        m_consumer->set("refresh", 0);
        m_consumer->purge();
        m_consumer->stop();
    }
}

int VideoWidget::reconfigure()
{
    int error = 0;
    // use SDL for audio, OpenGL for video
    QString serviceName = property("mlt_service").toString();
    if ((m_consumer == nullptr) || !m_consumer->is_valid() || strcmp(m_consumer->get("mlt_service"), "multi") == 0) {
        if (m_consumer) {
            m_consumer->purge();
            m_consumer->stop();
            m_consumer.reset();
        }
        QString audioBackend = (KdenliveSettings::external_display()) ? QString("decklink:%1").arg(KdenliveSettings::blackmagic_output_device())
                                                                      : KdenliveSettings::audiobackend();
        if (m_consumer == nullptr || serviceName.isEmpty() || serviceName != audioBackend) {
            m_consumer.reset(new Mlt::FilteredConsumer(pCore->getMonitorProfile(), audioBackend.toLatin1().constData()));
            if (m_consumer->is_valid()) {
                serviceName = audioBackend;
            } else {
                // Warning, audio backend unavailable on system
                m_consumer.reset();
                QStringList backends = {"sdl2_audio", "sdl_audio", "rtaudio"};
                for (const QString &bk : backends) {
                    if (bk == audioBackend) {
                        // Already tested
                        continue;
                    }
                    m_consumer.reset(new Mlt::FilteredConsumer(pCore->getMonitorProfile(), bk.toLatin1().constData()));
                    if (m_consumer->is_valid()) {
                        if (audioBackend == KdenliveSettings::sdlAudioBackend()) {
                            // switch sdl audio backend
                            KdenliveSettings::setSdlAudioBackend(bk);
                        }
                        KdenliveSettings::setAudiobackend(bk);
                        serviceName = bk;
                        break;
                    } else {
                        m_consumer.reset();
                    }
                }
            }
            if (!m_consumer || !m_consumer->is_valid()) {
                qWarning() << "no audio backend found";
                return -1;
            }
            setProperty("mlt_service", serviceName);
            if (KdenliveSettings::external_display()) {
                m_consumer->set("terminate_on_pause", 0);
            }
            m_consumer->set("width", m_profileSize.width());
            m_consumer->set("height", m_profileSize.height());
            m_colorSpace = pCore->getCurrentProfile()->colorspace();
            m_dar = pCore->getCurrentDar();
        }
        m_threadStartEvent.reset();
        m_threadStopEvent.reset();
        m_threadCreateEvent.reset();
        m_threadJoinEvent.reset();

        if (m_glslManager) {
            m_threadCreateEvent.reset(m_consumer->listen("consumer-thread-create", this, mlt_listener(onThreadCreate)));
            m_threadJoinEvent.reset(m_consumer->listen("consumer-thread-join", this, mlt_listener(onThreadJoin)));
        }
    }
    if (m_consumer->is_valid()) {
        // Connect the producer to the consumer - tell it to "run" later
        if (m_producer) {
            m_consumer->connect(*m_producer.get());
            // m_producer->set_speed(0.0);
        }

        int dropFrames = 1;
        if (!KdenliveSettings::monitor_dropframes()) {
            dropFrames = -dropFrames;
        }
        m_consumer->set("real_time", dropFrames);
        m_consumer->set("channels", pCore->audioChannels());
        if (KdenliveSettings::previewScaling() > 1) {
            m_consumer->set("scale", 1.0 / KdenliveSettings::previewScaling());
        }
        // C & D
        if (m_glslManager) {
            if (!m_threadStartEvent) {
                m_threadStartEvent.reset(m_consumer->listen("consumer-thread-started", this, mlt_listener(onThreadStarted)));
            }
            if (!m_threadStopEvent) {
                m_threadStopEvent.reset(m_consumer->listen("consumer-thread-stopped", this, mlt_listener(onThreadStopped)));
            }
            if (!serviceName.startsWith(QLatin1String("decklink"))) {
                m_consumer->set("mlt_image_format", "glsl");
            }
        } else {
            // A & B
            m_consumer->set("mlt_image_format", "yuv422");
        }
        m_displayEvent.reset(m_consumer->listen("consumer-frame-show", this, mlt_listener(on_frame_show)));

        int volume = KdenliveSettings::volume();
        if (serviceName.startsWith(QLatin1String("sdl"))) {
            QString audioDevice = KdenliveSettings::audiodevicename();
            if (!audioDevice.isEmpty()) {
                m_consumer->set("audio_device", audioDevice.toUtf8().constData());
            }

            QString audioDriver = KdenliveSettings::audiodrivername();
            if (!audioDriver.isEmpty()) {
                m_consumer->set("audio_driver", audioDriver.toUtf8().constData());
            }
        }
        if (!pCore->getProjectProfile().progressive()) {
            m_consumer->set("progressive", KdenliveSettings::monitor_progressive());
        }
        m_consumer->set("volume", volume / 100.0);
        // m_consumer->set("progressive", 1);
        m_consumer->set("rescale", KdenliveSettings::mltinterpolation().toUtf8().constData());
        m_consumer->set("deinterlacer", KdenliveSettings::mltdeinterlacer().toUtf8().constData());
        /*
#ifdef Q_OS_WIN
        m_consumer->set("audio_buffer", 2048);
#else
        m_consumer->set("audio_buffer", 512);
#endif
        */
        int fps = qRound(pCore->getCurrentFps());
        m_consumer->set("buffer", qMax(25, fps));
        m_consumer->set("prefill", 6);
        m_consumer->set("drop_max", fps / 4);
        if (KdenliveSettings::audio_scrub()) {
            m_consumer->set("scrub_audio", 1);
        } else {
            m_consumer->set("scrub_audio", 0);
        }
        if (KdenliveSettings::monitor_gamma() == 0) {
            m_consumer->set("color_trc", "iec61966_2_1");
        } else {
            m_consumer->set("color_trc", "bt709");
        }
    } else {
        // Cleanup on error
        error = 2;
    }
    return error;
}

float VideoWidget::zoom() const
{
    return m_zoom;
}

void VideoWidget::reloadProfile()
{
    // The profile display aspect ratio may have changed.
    bool existingConsumer = false;
    if (m_consumer) {
        // Make sure to delete and rebuild consumer to match profile
        m_consumer->purge();
        m_consumer->stop();
        m_consumer.reset();
        existingConsumer = true;
    }
    m_blackClip.reset(new Mlt::Producer(pCore->getProjectProfile(), "color:0"));
    m_blackClip->set("kdenlive:id", "black");
    m_blackClip->set("mlt_image_format", "rgba");
    if (existingConsumer) {
        reconfigure();
    }
    resizeVideo(width(), height());
    refreshSceneLayout();
}

QSize VideoWidget::profileSize() const
{
    return m_profileSize;
}

QRect VideoWidget::displayRect() const
{
    return m_rect;
}

QPoint VideoWidget::offset() const
{
    return {m_offset.x() - static_cast<int>(width() * m_zoom / 2), m_offset.y() - static_cast<int>(height() * m_zoom / 2)};
}

void VideoWidget::setZoom(float zoom, bool force)
{
    if (!force && m_zoom == zoom) {
        return;
    }
    double zoomRatio = double(zoom / m_zoom);
    m_zoom = zoom;
    Q_EMIT zoomChanged(zoomRatio);
    if (rootObject()) {
        rootObject()->setProperty("zoom", m_zoom);
        double scalex = rootObject()->property("scalex").toDouble() * zoomRatio;
        rootObject()->setProperty("scalex", scalex);
        double scaley = rootObject()->property("scaley").toDouble() * zoomRatio;
        rootObject()->setProperty("scaley", scaley);
    }
    resizeVideo(width(), height());
}

void VideoWidget::onFrameDisplayed(const SharedFrame &frame)
{
    m_mutex.lock();
    m_sharedFrame = frame;
    m_sendFrame = sendFrameForAnalysis;
    m_mutex.unlock();
    quickWindow()->update();
}

void VideoWidget::purgeCache()
{
    if (m_consumer) {
        // m_consumer->set("buffer", 1);
        m_consumer->purge();
        m_producer->seek(m_proxy->getPosition() + 1);
    }
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QQuickWidget::mouseDoubleClickEvent(event);
    if (event->isAccepted()) {
        return;
    }
    if ((rootObject() == nullptr) || rootObject()->objectName() != QLatin1String("rooteffectscene")) {
        Q_EMIT switchFullScreen();
    }
    event->accept();
}

void VideoWidget::setOffsetX(int x, int max)
{
    m_offset.setX(x);
    if (rootObject()) {
        rootObject()->setProperty("offsetx", m_zoom > 1.0f ? x - max / 2.0f + 10 * m_zoom : 0);
    }
    quickWindow()->update();
}

void VideoWidget::setOffsetY(int y, int max)
{
    m_offset.setY(y);
    if (rootObject()) {
        rootObject()->setProperty("offsety", m_zoom > 1.0f ? y - max / 2.0f + 10 * m_zoom : 0);
    }
    quickWindow()->update();
}

std::shared_ptr<Mlt::Consumer> VideoWidget::consumer()
{
    return m_consumer;
}

Mlt::Producer *VideoWidget::producer()
{
    return m_producer.get();
}

void VideoWidget::resetConsumer(bool fullReset)
{
    if (fullReset && m_consumer) {
        m_consumer->purge();
        m_consumer->stop();
        m_consumer.reset();
    }
    reconfigure();
}

void VideoWidget::on_frame_show(mlt_consumer, VideoWidget *widget, mlt_event_data data)
{
    auto frame = Mlt::EventData(data).to_frame();
    if (frame.is_valid() && frame.get_int("rendered")) {
        int timeout = (widget->consumer()->get_int("real_time") > 0) ? 0 : 1000;
        if ((widget->m_frameRenderer != nullptr) && widget->m_frameRenderer->semaphore()->tryAcquire(1, timeout)) {
            QMetaObject::invokeMethod(widget->m_frameRenderer, "showFrame", Qt::QueuedConnection, Q_ARG(Mlt::Frame, frame));
        }
    }
}

RenderThread::RenderThread(thread_function_t function, void *data)
    : QThread(nullptr)
    , m_function(function)
    , m_data(data)
    , m_context{new QOpenGLContext}
    , m_surface{new QOffscreenSurface}
{
    QSurfaceFormat format;
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setMajorVersion(3);
    format.setMinorVersion(2);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    m_context->setFormat(format);
    m_context->create();
    m_context->moveToThread(this);
    m_surface->setFormat(format);
    m_surface->create();
}

RenderThread::~RenderThread()
{
    m_surface->destroy();
}

// TODO: missing some exception handling?
void RenderThread::run()
{
    Q_ASSERT(m_context->isValid());
    m_context->makeCurrent(m_surface.get());
    m_function(m_data);
    m_context->doneCurrent();
}

FrameRenderer::FrameRenderer()
    : QThread(nullptr)
    , m_semaphore(3)
    , m_imageRequested(false)
{
    setObjectName(QStringLiteral("FrameRenderer"));
    moveToThread(this);
    start();
}

FrameRenderer::~FrameRenderer() {}

void FrameRenderer::requestImage()
{
    m_imageRequested = true;
}

void FrameRenderer::showFrame(Mlt::Frame frame)
{
    // Save this frame for future use and to keep a reference to the GL Texture.
    m_displayFrame = SharedFrame(frame);
    Q_EMIT frameDisplayed(m_displayFrame);
    if (m_imageRequested) {
        m_imageRequested = false;
        Q_EMIT imageReady();
    }

    m_semaphore.release();
}

SharedFrame FrameRenderer::getDisplayFrame()
{
    return m_displayFrame;
}

void VideoWidget::refreshSceneLayout()
{
    if (!rootObject()) {
        return;
    }
    QSize s = pCore->getCurrentFrameSize();
    Q_EMIT m_proxy->profileChanged();
    rootObject()->setProperty("scalex", double(m_rect.width() * m_zoom) / s.width());
    rootObject()->setProperty("scaley", double(m_rect.height() * m_zoom) / s.height());
}

bool VideoWidget::switchPlay(bool play, double speed)
{
    if (!m_producer || !m_consumer) {
        return false;
    }
    if (m_isZoneMode || m_isLoopMode) {
        resetZoneMode();
    }
    if (play) {
        if (m_consumer->position() >= m_maxProducerPosition && speed > 0) {
            // We are at the end of the clip / timeline
            if (m_id == Kdenlive::ClipMonitor || (m_id == Kdenlive::ProjectMonitor && KdenliveSettings::jumptostart())) {
                m_producer->seek(0);
            } else {
                return false;
            }
        }
        qDebug() << "pos: " << m_consumer->position() << "out: " << m_producer->get_playtime() - 1;
        double current_speed = m_producer->get_speed();
        m_producer->set_speed(speed);
        m_proxy->setSpeed(speed);
        if (qFuzzyCompare(speed, 1.0) || speed < -6. || speed > 6.) {
            m_consumer->set("scrub_audio", 0);
        } else if (KdenliveSettings::audio_scrub()) {
            m_consumer->set("scrub_audio", 1);
        }
        if (qFuzzyIsNull(current_speed)) {
            m_consumer->start();
            m_consumer->set("refresh", 1);
            m_consumer->set("volume", KdenliveSettings::volume() / 100.);
        } else {
            // Speed change, purge to reduce latency
            m_consumer->purge();
            m_producer->seek(m_consumer->position() + (speed > 1. ? 1 : 0));
        }
    } else {
        Q_EMIT paused();
        m_producer->set_speed(0);
        m_consumer->set("volume", 0);
        m_proxy->setSpeed(0);
        m_producer->seek(m_consumer->position() + 1);
        m_consumer->purge();
        m_consumer->start();
        m_consumer->set("scrub_audio", 0);
    }
    return true;
}

bool VideoWidget::playZone(bool startFromIn, bool loop)
{
    if (!m_producer || m_proxy->zoneOut() <= m_proxy->zoneIn()) {
        pCore->displayMessage(i18n("Select a zone to play"), ErrorMessage, 500);
        return false;
    }
    double current_speed = m_producer->get_speed();
    m_producer->set_speed(0);
    m_proxy->setSpeed(0);
    m_loopOut = m_proxy->zoneOut();
    m_loopIn = m_proxy->zoneIn();
    if (qFuzzyIsNull(current_speed)) {
        if (startFromIn || getCurrentPos() > m_loopOut) {
            m_producer->seek(m_loopIn);
        }
        m_consumer->start();
        m_producer->set_speed(1.0);
        m_consumer->set("scrub_audio", 0);
        m_consumer->set("refresh", 1);
        m_consumer->set("volume", KdenliveSettings::volume() / 100.);
    } else if (startFromIn || getCurrentPos() > m_loopOut) {
        // Speed change, purge to reduce latency
        m_consumer->set("refresh", 0);
        m_producer->seek(m_loopIn);
        m_consumer->purge();
        m_producer->set_speed(1.0);
        m_consumer->set("refresh", 1);
    }
    m_isZoneMode = true;
    m_isLoopMode = loop;
    return true;
}

bool VideoWidget::restartConsumer()
{
    int result = 0;
    if (m_consumer->is_stopped()) {
        // When restarting the consumer, we need to restore the preview scaling
        int cWidth = m_consumer->get_int("width");
        int cHeigth = m_consumer->get_int("height");
        result = m_consumer->start();
        if (cWidth > 0) {
            m_consumer->set("width", cWidth);
            m_consumer->set("height", cHeigth);
        }
    }
    return result != -1;
}

bool VideoWidget::loopClip(QPoint inOut)
{
    if (!m_producer || inOut.y() <= inOut.x()) {
        pCore->displayMessage(i18n("Select a clip to play"), ErrorMessage, 500);
        return false;
    }
    m_loopIn = inOut.x();
    double current_speed = m_producer->get_speed();
    m_producer->set_speed(0);
    m_proxy->setSpeed(0);
    m_loopOut = inOut.y();
    if (qFuzzyIsNull(current_speed)) {
        m_producer->seek(m_loopIn);
        m_consumer->start();
        m_consumer->set("scrub_audio", 0);
        m_consumer->set("refresh", 1);
        m_consumer->set("volume", KdenliveSettings::volume() / 100.);
    } else {
        // Speed change, purge to reduce latency
        m_consumer->set("refresh", 0);
        m_consumer->purge();
        m_producer->seek(m_loopIn);
        m_producer->set_speed(1.0);
        m_consumer->set("refresh", 1);
    }
    m_isZoneMode = false;
    m_isLoopMode = true;
    return true;
}

void VideoWidget::resetZoneMode()
{
    if (!m_isZoneMode && !m_isLoopMode) {
        return;
    }
    m_loopIn = 0;
    m_loopOut = 0;
    m_isZoneMode = false;
    m_isLoopMode = false;
}

MonitorProxy *VideoWidget::getControllerProxy()
{
    return m_proxy;
}

int VideoWidget::getCurrentPos() const
{
    return m_proxy->getPosition();
}

void VideoWidget::setRulerInfo(int duration, const std::shared_ptr<MarkerSortModel> &model)
{
    m_maxProducerPosition = duration;
    rootObject()->setProperty("duration", duration);
    if (model != nullptr) {
        // we are resetting marker/snap model, reset zone
        rootContext()->setContextProperty("markersModel", model.get());
    }
}

void VideoWidget::switchRecordState(bool on)
{
    if (on) {
        if (m_maxProducerPosition == 0x7fffffff) {
            // We are already in rec mode
            return;
        }
        m_bckpMax = m_maxProducerPosition;
        m_maxProducerPosition = 0x7fffffff;
    } else {
        m_maxProducerPosition = m_bckpMax;
    }
}

void VideoWidget::startConsumer()
{
    if (m_consumer == nullptr) {
        return;
    }
    if (!restartConsumer()) {
        // ARGH CONSUMER BROKEN!!!!
        KMessageBox::error(
            qApp->activeWindow(),
            i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
        m_displayEvent.reset();
        m_consumer.reset();
        return;
    }
    m_consumer->set("refresh", 1);
}

void VideoWidget::stop()
{
    m_refreshTimer.stop();
    // why this lock?
    QMutexLocker locker(&m_mltMutex);
    if (m_producer) {
        if (m_isZoneMode || m_isLoopMode) {
            resetZoneMode();
        }
        m_producer->set_speed(0.0);
        m_proxy->setSpeed(0);
    }
    if (m_consumer) {
        m_consumer->purge();
        if (!m_consumer->is_stopped()) {
            m_consumer->stop();
        }
    }
}

double VideoWidget::playSpeed() const
{
    if (m_producer) {
        return m_producer->get_speed();
    }
    return 0.0;
}

void VideoWidget::restart()
{
    // why this lock?
    if (m_consumer) {
        // Make sure to delete and rebuild consumer to match profile
        m_consumer->purge();
        m_consumer->stop();
        reconfigure();
    }
}

int VideoWidget::volume() const
{
    if ((!m_consumer) || (!m_producer)) {
        return -1;
    }
    if (m_consumer->get("mlt_service") == QStringLiteral("multi")) {
        return (int(100 * m_consumer->get_double("0.volume")));
    }
    return (int(100 * m_consumer->get_double("volume")));
}

void VideoWidget::setVolume(double volume)
{
    if (m_consumer) {
        if (m_consumer->get("mlt_service") == QStringLiteral("multi")) {
            m_consumer->set("0.volume", volume);
        } else {
            m_consumer->set("volume", volume);
        }
    }
}

int VideoWidget::duration() const
{
    if (!m_producer) {
        return 0;
    }
    return m_producer->get_playtime();
}

void VideoWidget::setConsumerProperty(const QString &name, const QString &value)
{
    QMutexLocker locker(&m_mltMutex);
    if (m_consumer) {
        m_consumer->set(name.toUtf8().constData(), value.toUtf8().constData());
        if (m_consumer->start() == -1) {
            qCWarning(KDENLIVE_LOG) << "ERROR, Cannot start monitor";
        }
    }
}

bool VideoWidget::updateScaling()
{
    int previewHeight = pCore->getCurrentFrameSize().height();
    switch (KdenliveSettings::previewScaling()) {
    case 2:
        previewHeight = qMin(previewHeight, 720);
        break;
    case 4:
        previewHeight = qMin(previewHeight, 540);
        break;
    case 8:
        previewHeight = qMin(previewHeight, 360);
        break;
    case 16:
        previewHeight = qMin(previewHeight, 270);
        break;
    default:
        break;
    }
    int pWidth = int(previewHeight * pCore->getCurrentDar() / pCore->getCurrentSar());
    if (pWidth % 2 > 0) {
        pWidth++;
    }
    QSize profileSize(pWidth, previewHeight);
    if (profileSize == m_profileSize) {
        return false;
    }
    m_profileSize = profileSize;
    pCore->getMonitorProfile().set_width(m_profileSize.width());
    pCore->getMonitorProfile().set_height(m_profileSize.height());
    if (m_consumer) {
        m_consumer->set("width", m_profileSize.width());
        m_consumer->set("height", m_profileSize.height());
        resizeVideo(width(), height());
    }
    return true;
}

void VideoWidget::switchRuler(bool show)
{
    m_rulerHeight = show ? int(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5) : 0;
    m_displayRulerHeight = m_rulerHeight;
    resizeVideo(width(), height());
    Q_EMIT m_proxy->rulerHeightChanged();
}
