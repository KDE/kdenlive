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

#include "monitor/monitor.h"
#include "monitor/view/qmliconprovider.hpp"
#include <QApplication>
#include <QFontDatabase>
#include <QOpenGLContext>
#if QT_CONFIG(opengles2)
#include <QOpenGLFunctions_ES2>
#else
#include <QOpenGLFunctions_3_2_Core>
#endif
#include <QPainter>
#include <QQmlContext>
#include <QQuickItem>
#include <QStyle>
#include <QtGlobal>
#include <memory>

#include <ki18n_version.h>

#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <KLocalizedQmlContext>
#else
#include <KLocalizedContext>
#endif
#include <KLocalizedString>
#include <KMessageBox>

#include "bin/model/markersortmodel.h"
#include "core.h"
#include "monitorproxy.h"
#include "profiles/profilemodel.hpp"
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
    , m_initSem(0)
    , m_glslManager(nullptr)
    , m_frameRenderer(nullptr)
    , m_colorSpace(601)
    , m_dar(1.78)
    , m_isZoneMode(false)
    , m_isLoopMode(false)
    , m_loopIn(0)
    , m_offset(QPoint(0, 0))
{
#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    KLocalization::setupLocalizedContext(engine());
#else
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));
#endif
    qRegisterMetaType<Mlt::Frame>("Mlt::Frame");
    qRegisterMetaType<SharedFrame>("SharedFrame");
    setAcceptDrops(true);
    setClearColor(KdenliveSettings::window_background());

    if (m_id == Kdenlive::ClipMonitor && !(KdenliveSettings::displayClipMonitorInfo() & Monitor::InfoOverlay)) {
        m_rulerHeight = 0;
    } else if (!(KdenliveSettings::displayProjectMonitorInfo() & Monitor::InfoOverlay)) {
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

    m_proxy = new MonitorProxy(this);
    rootContext()->setContextProperty("controller", m_proxy);
    engine()->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize);
    engine()->addImageProvider(QStringLiteral("icon"), new QmlIconProvider(QSize(iconSize, iconSize), this));
    m_mouseTimer.setSingleShot(true);
    m_mouseTimer.setInterval(2000);
}

VideoWidget::~VideoWidget()
{
    stop();
    m_mouseTimer.stop();
    if (m_frameRenderer && m_frameRenderer->isRunning()) {
        m_frameRenderer->quit();
        m_frameRenderer->wait();
        m_frameRenderer->deleteLater();
    }
    m_blackClip.reset();
}

void VideoWidget::enableMouseTimer(bool enable)
{
    m_fullScreen = enable;
    if (enable) {
        connect(&m_mouseTimer, &QTimer::timeout, this, &VideoWidget::blankCursor, Qt::UniqueConnection);
    } else {
        m_mouseTimer.stop();
        setCursor(Qt::ArrowCursor);
        disconnect(&m_mouseTimer, &QTimer::timeout, this, &VideoWidget::blankCursor);
    }
}

void VideoWidget::blankCursor()
{
    setCursor(Qt::BlankCursor);
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

void VideoWidget::setFixedImageSize(const QSize fixedSize)
{
    m_fixedSize = fixedSize;
}

void VideoWidget::resizeVideo(int width, int height)
{
    double x, y, w, h;
    height -= m_displayRulerHeight;
    double this_aspect = double(width) / height;

    // Special case optimization to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if (m_fixedSize.isValid()) {
        w = m_fixedSize.width();
        h = m_fixedSize.height();
    } else {
        if (int(this_aspect * 1000) == int(m_dar * 1000)) {
            w = width;
            h = height;
        }
        // Use OpenGL to normalise sample aspect ratio
        else if (height * m_dar > width) {
            w = width;
            h = width / m_dar;
        } else {
            w = height * m_dar;
            h = height;
        }
    }
    x = (width - w) / 2.0;
    y = (height - h) / 2.0;
    m_rect = QRectF(x, y, w, h);
    const QSize parentSize = parentWidget()->size();
    m_monitorOffset = QPointF((parentSize.width() - width) / 2., (parentSize.height() - m_displayRulerHeight - height) / 2.);

    QQuickItem *rootQml = rootObject();
    if (rootQml) {
        QSize s = pCore->getCurrentFrameSize();
        double scalex = m_rect.width() * m_zoom / s.width();
        double scaley = m_rect.height() * m_zoom / s.height();
        rootQml->setProperty("center", m_rect.center());
        rootQml->setProperty("scalex", scalex);
        rootQml->setProperty("scaley", scaley);
        if (rootQml->objectName() == QLatin1String("rootsplit")) {
            // Adjust splitter pos
            rootQml->setProperty("splitterPos", x + (rootQml->property("percentage").toDouble() * w));
        }
    }
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

void VideoWidget::updateImagePosition()
{
    if (m_fixedSize.isValid()) {
        resizeVideo(width(), height());
    }
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
        // Allow max zoom at 60 pixels in the monitor view
        double maxZoom = qMin(m_profileSize.width() / 60., m_profileSize.height() / 60.);
        if (m_zoom < maxZoom) {
            setZoom(m_zoom * 1.2);
        }
    } else if (m_zoom > 0.2f) {
        setZoom(m_zoom / 1.2f);
    }
}

void VideoWidget::slotZoomReset()
{
    setZoom(1.);
}

void VideoWidget::refreshRect()
{
    resizeVideo(width(), height());
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
    if (!m_consumer) {
        return;
    }
    if (!qFuzzyIsNull(m_producer->get_speed())) {
        m_consumer->purge();
    }
    restartConsumer();
    m_consumer->set("refresh", 1);
    if (KdenliveSettings::audio_scrub() && !noAudioScrub) {
        m_consumer->set("volume", KdenliveSettings::volume() / 100.0);
        m_consumer->set("scrub_audio", 1);
    } else {
        m_consumer->set("scrub_audio", 0);
    }
}

void VideoWidget::requestRefresh(bool slowRefresh)
{
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.start(slowRefresh ? 200 : 10);
    } else if (m_producer && qFuzzyIsNull(m_producer->get_speed())) {
        m_consumer->set("scrub_audio", 0);
        m_refreshTimer.start(slowRefresh ? 200 : 10);
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
    if (!m_producer) {
        return false;
    }
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
    if (m_fullScreen) {
        if (!m_mouseTimer.isActive()) {
            setCursor(Qt::ArrowCursor);
        } else {
            m_mouseTimer.stop();
        }
    }
    QQuickWidget::mousePressEvent(event);
    // For some reason, on Qt6 in mouseReleaseEvent, the event is always accepted, so use this m_qmlEvent bool to track if the event is accepted in qml
    m_qmlEvent = event->isAccepted();
    m_dragStart = QPoint();
    if (rootObject() != nullptr && m_qmlEvent && rootObject()->property("captureRightClick").toBool()) {
        // The event has been handled in qml
        m_swallowDrop = true;
    } else {
        m_swallowDrop = false;
    }
    if ((event->button() & Qt::LeftButton) != 0u) {
        if ((event->modifiers() & Qt::ControlModifier) != 0u) {
            // Pan view
            m_panStart = event->pos();
            setCursor(Qt::ClosedHandCursor);
        } else if (getControllerProxy()->dragType() != QLatin1String("-")) {
            m_dragStart = event->pos();
        }
    } else if ((event->button() & Qt::RightButton) != 0u && !m_swallowDrop) {
        Q_EMIT showContextMenu(event->globalPosition().toPoint());
    } else if ((event->button() & Qt::MiddleButton) != 0u) {
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void VideoWidget::focusInEvent(QFocusEvent *event)
{
    if (m_fullScreen) {
        if (!m_mouseTimer.isActive()) {
            setCursor(Qt::ArrowCursor);
        }
        if (parentWidget()->isFullScreen()) {
            m_mouseTimer.start();
        }
    }
    QQuickWidget::focusInEvent(event);
}

void VideoWidget::focusOutEvent(QFocusEvent *event)
{
    if (m_fullScreen) {
        if (!m_mouseTimer.isActive()) {
            setCursor(Qt::ArrowCursor);
        }
        m_mouseTimer.stop();
    }
    QQuickWidget::focusOutEvent(event);
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_fullScreen) {
        m_mouseTimer.start();
    }
    bool qmlClick = rootObject()->property("captureRightClick").toBool();
    QQuickWidget::mouseReleaseEvent(event);
    rootObject()->setProperty("captureRightClick", false);
    bool playMonitor = KdenliveSettings::play_monitor_on_click() &&
                       (m_dragStart.isNull() || (event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance()) && m_panStart.isNull();

    m_dragStart = QPoint();
    m_panStart = QPoint();
    setCursor(Qt::ArrowCursor);
    if (event->modifiers() & Qt::ControlModifier || m_qmlEvent) {
        event->accept();
        m_swallowDrop = false;
        return;
    }
    if (playMonitor && ((event->button() & Qt::LeftButton) != 0u) && !m_swallowDrop && !qmlClick) {
        event->accept();
        Q_EMIT monitorPlay();
    }
    m_swallowDrop = false;
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_fullScreen) {
        if (!m_mouseTimer.isActive()) {
            setCursor(Qt::ArrowCursor);
        }
        m_mouseTimer.start();
    }
    if ((rootObject() != nullptr) && rootObject()->objectName() != QLatin1String("root") && !(event->modifiers() & Qt::ControlModifier) &&
        !(event->buttons() & Qt::MiddleButton)) {
        event->ignore();
        QQuickWidget::mouseMoveEvent(event);
        return;
    }
    QQuickWidget::mouseMoveEvent(event);
    if (event->buttons() & Qt::RightButton || event->buttons() & Qt::NoButton) {
        event->accept();
        return;
    }
    if (!m_panStart.isNull()) {
        Q_EMIT panView(m_panStart - event->pos());
        m_panStart = event->pos();
        event->accept();
        return;
    }

    if (!m_dragStart.isNull() && (event->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
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
        if (producer == nullptr && currentId == QLatin1String("black")) {
            return 0;
        }
    }
    if (m_consumer) {
        consumerPosition = m_consumer->position();
    }
    pause();
    m_producer.reset();
    if (producer) {
        m_producer = std::move(producer);
    } else {
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
        m_proxy->setPositionAdvanced(position >= 0 ? position : m_producer->position(), true);
    }
    return error;
}

bool VideoWidget::isPaused() const
{
    return m_producer && qAbs(m_producer->get_speed()) < 0.1;
}

void VideoWidget::pause()
{
    int position = m_consumer ? m_consumer->position() + 1 : -1;
    if (m_producer && (!isPaused() || (m_maxProducerPosition - position < 25))) {
        Q_EMIT paused();
        m_producer->set_speed(0);
        if (m_consumer && m_consumer->is_valid()) {
            m_consumer->set("volume", 0);
            m_producer->seek(position);
            m_consumer->purge();
            m_consumer->start();
            m_consumer->set("scrub_audio", 0);
        }
    }
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
        QString audioBackend = (KdenliveSettings::external_display()) ? QStringLiteral("decklink:%1").arg(KdenliveSettings::blackmagic_output_device())
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
        m_consumer->set("scrub_audio", KdenliveSettings::audio_scrub());
        switch (pCore->getProjectProfile().colorspace()) {
        case 601:
        case 170:
            m_consumer->set("color_trc", "smpte170m");
            break;
        case 240:
            m_consumer->set("color_trc", "smpte240m");
            break;
        case 470:
            m_consumer->set("color_trc", "bt470bg");
            break;
        case 2020:
            // if (isDeckLinkHLG) {
            //     m_consumer->set("color_trc", "arib-std-b67");
            m_consumer->clear("color_trc");
            break;
        default:
            m_consumer->set("color_trc", "bt709");
            break;
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
    return m_rect.toRect();
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
    // When zooming a lot in the image, switch to nearest neighbor interpolation for the display so we can see individual pixels
    m_nearestNeighborInterpolation = zoom > 10;
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

void VideoWidget::setOffsetX(int horizontalScrollValue, int horizontalScrollMaximum, int verticalScrollBarWidth)
{
    m_offset.setX(horizontalScrollValue);

    if (rootObject()) {
        double adjustedOffset = 0.0;
        if (m_zoom > 1.0) {
            // Center the view and account for the zoom and scrollbar width
            double scrollValue = static_cast<double>(horizontalScrollValue);
            double scrollMax = static_cast<double>(horizontalScrollMaximum);
            double scrollBarWidth = static_cast<double>(verticalScrollBarWidth);

            // Center the offset, then adjust for the zoomed-in view and scrollbar width
            double centerOffset = scrollValue - (scrollMax / 2.0);
            double zoomAdjustment = (scrollBarWidth * m_zoom) / 2.0;
            adjustedOffset = centerOffset + zoomAdjustment;
        }
        rootObject()->setProperty("offsetx", adjustedOffset);
    }

    quickWindow()->update();
}

void VideoWidget::setOffsetY(int verticalScrollValue, int verticalScrollMaximum, int horizontalScrollBarHeight)
{
    m_offset.setY(verticalScrollValue);

    if (rootObject()) {
        double adjustedOffset = 0.0;
        if (m_zoom > 1.0) {
            // Center the view and account for the zoom and scrollbar height
            double scrollValue = static_cast<double>(verticalScrollValue);
            double scrollMax = static_cast<double>(verticalScrollMaximum);
            double scrollBarHeight = static_cast<double>(horizontalScrollBarHeight);

            // Center the offset, then adjust for the zoomed-in view and scrollbar height
            double centerOffset = scrollValue - (scrollMax / 2.0);
            double zoomAdjustment = (scrollBarHeight * m_zoom) / 2.0;
            adjustedOffset = centerOffset + zoomAdjustment;
        }
        rootObject()->setProperty("offsety", adjustedOffset);
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
        double current_speed = m_producer->get_speed();
        m_producer->set_speed(speed);
        m_proxy->setSpeed(speed);
        if (qFuzzyCompare(speed, 1.0) || speed < -6. || speed > 6.) {
            m_consumer->set("scrub_audio", 0);
        } else if (KdenliveSettings::audio_scrub()) {
            m_consumer->set("scrub_audio", 1);
        }
        if (qFuzzyIsNull(current_speed)) {
            if (m_maxProducerPosition - m_consumer->position() < 6) {
                m_consumer->set("prefill", 1);
            } else {
                m_consumer->set("prefill", 6);
            }
            m_consumer->start();
            m_consumer->set("refresh", 1);
            m_consumer->set("volume", KdenliveSettings::volume() / 100.);
        } else {
            // Speed change, purge to reduce latency
            m_consumer->purge();
            m_producer->seek(m_consumer->position() + (speed > 1. ? 1 : 0));
        }
    } else {
        pause();
    }
    return true;
}

bool VideoWidget::playZone(bool startFromIn, bool loop)
{
    if (!m_producer || m_proxy->zoneOut() <= m_proxy->zoneIn()) {
        pCore->displayMessage(i18n("Select a zone to play"), ErrorMessage, 500);
        return false;
    }
    return playZone(m_proxy->zoneIn(), m_proxy->zoneOut(), startFromIn, loop, true);
}

bool VideoWidget::loopClip(std::pair<int, int> inOut)
{
    if (!m_producer || inOut.second <= inOut.first) {
        pCore->displayMessage(i18n("Select a clip to play"), ErrorMessage, 500);
        return false;
    }
    return playZone(inOut.first, inOut.second, false, true, false);
}

bool VideoWidget::playZone(int in, int out, bool startFromIn, bool loop, bool zoneMode)
{
    double current_speed = m_producer->get_speed();
    m_producer->set_speed(0);
    m_proxy->setSpeed(0);
    m_loopOut = out;
    m_loopIn = in;
    if (qFuzzyIsNull(current_speed)) {
        if (startFromIn || getCurrentPos() > m_loopOut) {
            m_producer->seek(m_loopIn);
        }
        m_consumer->start();
        m_producer->set_speed(1.0);
        m_consumer->set("scrub_audio", 0);
        m_consumer->set("refresh", 1);
        m_consumer->set("volume", KdenliveSettings::volume() / 100.);
    } else if (getCurrentPos() > m_loopOut) {
        // Speed change, purge to reduce latency
        m_consumer->set("refresh", 0);
        m_producer->seek(m_loopIn);
        m_consumer->purge();
        m_producer->set_speed(1.0);
        m_consumer->set("refresh", 1);
    }
    m_isZoneMode = zoneMode;
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
    case 1:
        previewHeight = qMin(previewHeight, 1080);
        break;
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
