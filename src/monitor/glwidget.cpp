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
#include "glwidget.h"
#include "monitorproxy.h"
#include "profiles/profilemodel.hpp"
#include "timeline2/view/qml/timelineitems.h"
#include "timeline2/view/qmltypes/thumbnailprovider.h"
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

VideoWidget::VideoWidget(int id, QWidget *parent)
    : QQuickWidget(parent)
    , sendFrameForAnalysis(false)
    , m_glslManager(nullptr)
    , m_consumer(nullptr)
    , m_producer(nullptr)
    , m_id(id)
    , m_rulerHeight(int(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5))
    , m_bgColor(KdenliveSettings::window_background())
    , m_shader(nullptr)
    , m_initSem(0)
    , m_analyseSem(1)
    , m_isInitialized(false)
    , m_maxProducerPosition(0)
    , m_threadStartEvent(nullptr)
    , m_threadStopEvent(nullptr)
    , m_threadCreateEvent(nullptr)
    , m_threadJoinEvent(nullptr)
    , m_displayEvent(nullptr)
    , m_frameRenderer(nullptr)
    , m_projectionLocation(0)
    , m_modelViewLocation(0)
    , m_vertexLocation(0)
    , m_texCoordLocation(0)
    , m_colorspaceLocation(0)
    , m_zoom(1.0f)
    , m_profileSize(1920, 1080)
    , m_colorSpace(601)
    , m_dar(1.78)
    , m_sendFrame(false)
    , m_isZoneMode(false)
    , m_isLoopMode(false)
    , m_loopIn(0)
    , m_offset(QPoint(0, 0))
    , m_fbo(nullptr)
    , m_shareContext(nullptr)
    , m_openGLSync(false)
    , m_ClientWaitSync(nullptr)
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0) || KDECLARATIVE_VERSION > QT_VERSION_CHECK(5, 98, 0)
    engine()->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
#else
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
#endif
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));

    m_texture[0] = m_texture[1] = m_texture[2] = 0;
    qRegisterMetaType<Mlt::Frame>("Mlt::Frame");
    qRegisterMetaType<SharedFrame>("SharedFrame");
    setAcceptDrops(true);

    if (m_id == Kdenlive::ClipMonitor && !(KdenliveSettings::displayClipMonitorInfo() & 0x01)) {
        m_rulerHeight = 0;
    } else if (!(KdenliveSettings::displayProjectMonitorInfo() & 0x01)) {
        m_rulerHeight = 0;
    }
    m_displayRulerHeight = m_rulerHeight;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    quickWindow()->setPersistentOpenGLContext(true);
    quickWindow()->setPersistentSceneGraph(true);
    quickWindow()->setClearBeforeRendering(false);
#else
    // TODO: qt6
    quickWindow()->setPersistentGraphics(true);
    quickWindow()->setPersistentSceneGraph(true);
    //quickWindow()->setClearBeforeRendering(false);
#endif
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    auto fmt = QOpenGLContext::globalShareContext()->format();
    fmt.setDepthBufferSize(format().depthBufferSize());
    fmt.setStencilBufferSize(format().stencilBufferSize());
    m_offscreenSurface.setFormat(fmt);
    m_offscreenSurface.create();

    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(10);
    m_blackClip.reset(new Mlt::Producer(pCore->getProjectProfile(), "color:0"));
    m_blackClip->set("mlt_image_format", "rgba");
    m_blackClip->set("kdenlive:id", "black");
    m_blackClip->set("out", 3);
    connect(&m_refreshTimer, &QTimer::timeout, this, &VideoWidget::refresh);
    m_producer = m_blackClip;
    rootContext()->setContextProperty("markersModel", nullptr);
    if (!initGPUAccel()) {
        disableGPUAccel();
    }

    connect(quickWindow(), &QQuickWindow::sceneGraphInitialized, this, &VideoWidget::initializeGL, Qt::DirectConnection);
    connect(quickWindow(), &QQuickWindow::beforeRendering, this, &VideoWidget::paintGL, Qt::DirectConnection);
    // connect(pCore.get(), &Core::updateMonitorProfile, this, &VideoWidget::reloadProfile);
    connect(pCore.get(), &Core::switchTimelineRecord, this, &VideoWidget::switchRecordState);

    registerTimelineItems();
    m_proxy = new MonitorProxy(this);
    rootContext()->setContextProperty("controller", m_proxy);
    engine()->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
}

VideoWidget::~VideoWidget()
{
    // C & D
    delete m_glslManager;
    delete m_threadStartEvent;
    delete m_threadStopEvent;
    delete m_threadCreateEvent;
    delete m_threadJoinEvent;
    delete m_displayEvent;
    if (m_frameRenderer) {
        if (m_frameRenderer->isRunning()) {
            QMetaObject::invokeMethod(m_frameRenderer, "cleanup");
            m_frameRenderer->quit();
            m_frameRenderer->wait();
            m_frameRenderer->deleteLater();
        } else {
            delete m_frameRenderer;
        }
    }
    m_blackClip.reset();
    delete m_shareContext;
    delete m_shader;
    // delete pCore->getCurrentProfile();
}

void VideoWidget::updateAudioForAnalysis()
{
    if (m_frameRenderer) {
        m_frameRenderer->sendAudioForAnalysis = KdenliveSettings::monitor_audio();
    }
}

void VideoWidget::initializeGL()
{
    if (m_isInitialized) return;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    quickWindow()->openglContext()->makeCurrent(&m_offscreenSurface);
#else
    QOpenGLContext &context = *static_cast< QOpenGLContext  *>(quickWindow()->rendererInterface()->getResource(quickWindow(), QSGRendererInterface::OpenGLContextResource));
    context.makeCurrent(&m_offscreenSurface);
#endif
    initializeOpenGLFunctions();

    // C & D
    if (onlyGLESGPUAccel()) {
        disableGPUAccel();
    }

    createShader();

    m_openGLSync = initGPUAccelSync();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    quickWindow()->openglContext()->doneCurrent();
#else
    context.doneCurrent();
#endif

    // C & D
    if (m_glslManager) {
        // Create a context sharing with this context for the RenderThread context.
        // This is needed because openglContext() is active in another thread
        // at the time that RenderThread is created.
        // See this Qt bug for more info: https://bugreports.qt.io/browse/QTBUG-44677
        // TODO: QTBUG-44677 is closed. still applicable?
        m_shareContext = new QOpenGLContext;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_shareContext->setFormat(quickWindow()->openglContext()->format());
        m_shareContext->setShareContext(quickWindow()->openglContext());
#else
        m_shareContext->setFormat(context.format());
        m_shareContext->setShareContext(&context);
#endif
        m_shareContext->create();
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_frameRenderer = new FrameRenderer(quickWindow()->openglContext(), &m_offscreenSurface, m_ClientWaitSync);
#else
    m_frameRenderer = new FrameRenderer(&context, &m_offscreenSurface, m_ClientWaitSync);
#endif

    m_frameRenderer->sendAudioForAnalysis = KdenliveSettings::monitor_audio();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    quickWindow()->openglContext()->makeCurrent(quickWindow());
#else
    context.makeCurrent(quickWindow());
#endif
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &VideoWidget::onFrameDisplayed, Qt::QueuedConnection);
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &VideoWidget::frameDisplayed, Qt::QueuedConnection);
    connect(m_frameRenderer, &FrameRenderer::textureReady, this, &VideoWidget::updateTexture, Qt::DirectConnection);
    m_initSem.release();
    m_isInitialized = true;
    QMetaObject::invokeMethod(this, "reconfigure", Qt::QueuedConnection);
}

void VideoWidget::resizeGL(int width, int height)
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
    resizeGL(event->size().width(), event->size().height());
}

void VideoWidget::forceRefreshZoom()
{
    // Only used for Qt6
}

void VideoWidget::createGPUAccelFragmentProg()
{
    m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, "uniform sampler2D tex;"
                                                               "varying highp vec2 coordinates;"
                                                               "void main(void) {"
                                                               "  gl_FragColor = texture2D(tex, coordinates);"
                                                               "}");
    m_shader->link();
    m_textureLocation[0] = m_shader->uniformLocation("tex");
}

void VideoWidget::createShader()
{
    m_shader = new QOpenGLShaderProgram;
    m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex, "uniform highp mat4 projection;"
                                                             "uniform highp mat4 modelView;"
                                                             "attribute highp vec4 vertex;"
                                                             "attribute highp vec2 texCoord;"
                                                             "varying highp vec2 coordinates;"
                                                             "void main(void) {"
                                                             "  gl_Position = projection * modelView * vertex;"
                                                             "  coordinates = texCoord;"
                                                             "}");
    // C & D
    if (m_glslManager) {
        createGPUAccelFragmentProg();
    } else {
        // A & B
        createYUVTextureProjectFragmentProg();
    }

    m_projectionLocation = m_shader->uniformLocation("projection");
    m_modelViewLocation = m_shader->uniformLocation("modelView");
    m_vertexLocation = m_shader->attributeLocation("vertex");
    m_texCoordLocation = m_shader->attributeLocation("texCoord");
}

void VideoWidget::createYUVTextureProjectFragmentProg()
{
    m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                      "uniform sampler2D Ytex, Utex, Vtex;"
                                      "uniform lowp int colorspace;"
                                      "varying highp vec2 coordinates;"
                                      "void main(void) {"
                                      "  mediump vec3 texel;"
                                      "  texel.r = texture2D(Ytex, coordinates).r - 16.0/255.0;"  // Y
                                      "  texel.g = texture2D(Utex, coordinates).r - 128.0/255.0;" // U
                                      "  texel.b = texture2D(Vtex, coordinates).r - 128.0/255.0;" // V
                                      "  mediump mat3 coefficients;"
                                      "  if (colorspace == 601) {"
                                      "    coefficients = mat3("
                                      "      1.1643,  1.1643,  1.1643," // column 1
                                      "      0.0,    -0.39173, 2.017,"  // column 2
                                      "      1.5958, -0.8129,  0.0);"   // column 3
                                      "  } else {"                      // ITU-R 709
                                      "    coefficients = mat3("
                                      "      1.1643, 1.1643, 1.1643," // column 1
                                      "      0.0,   -0.213,  2.112,"  // column 2
                                      "      1.793, -0.533,  0.0);"   // column 3
                                      "  }"
                                      "  gl_FragColor = vec4(coefficients * texel, 1.0);"
                                      "}");
    m_shader->link();
    m_textureLocation[0] = m_shader->uniformLocation("Ytex");
    m_textureLocation[1] = m_shader->uniformLocation("Utex");
    m_textureLocation[2] = m_shader->uniformLocation("Vtex");
    m_colorspaceLocation = m_shader->uniformLocation("colorspace");
}

static void uploadTextures(QOpenGLContext *context, const SharedFrame &frame, GLuint texture[])
{
    int width = frame.get_image_width();
    int height = frame.get_image_height();
    const uint8_t *image = frame.get_image(mlt_image_yuv420p);
    QOpenGLFunctions *f = context->functions();

    // The planes of pixel data may not be a multiple of the default 4 bytes.
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Upload each plane of YUV to a texture.
    if (texture[0] != 0u) {
        f->glDeleteTextures(3, texture);
    }
    check_error(f);
    f->glGenTextures(3, texture);
    check_error(f);

    f->glBindTexture(GL_TEXTURE_2D, texture[0]);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    check_error(f);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, image);
    check_error(f);

    f->glBindTexture(GL_TEXTURE_2D, texture[1]);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    check_error(f);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, image + width * height);
    check_error(f);

    f->glBindTexture(GL_TEXTURE_2D, texture[2]);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    check_error(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    check_error(f);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, image + width * height + width / 2 * height / 2);
    check_error(f);
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

bool VideoWidget::acquireSharedFrameTextures()
{
    // A
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if ((m_glslManager == nullptr) && !quickWindow()->openglContext()->supportsThreadedOpenGL()) {
        QMutexLocker locker(&m_contextSharedAccess);
        if (!m_sharedFrame.is_valid()) {
            return false;
        }
        uploadTextures(quickWindow()->openglContext(), m_sharedFrame, m_texture);
    } else
#else
        QOpenGLContext &context = *static_cast< QOpenGLContext  *>(quickWindow()->rendererInterface()->getResource(quickWindow(), QSGRendererInterface::OpenGLContextResource));
        if ((m_glslManager == nullptr) && !context.supportsThreadedOpenGL()) {
            QMutexLocker locker(&m_contextSharedAccess);
            if (!m_sharedFrame.is_valid()) {
                return false;
            }
            uploadTextures(&context, m_sharedFrame, m_texture);
        } else
#endif
    if (m_glslManager) {
        // C & D
        m_contextSharedAccess.lock();
        if (m_sharedFrame.is_valid()) {
            m_texture[0] = *(reinterpret_cast<const GLuint *>(m_sharedFrame.get_image(mlt_image_opengl_texture)));
        }
    }

    if (!m_texture[0]) {
        // C & D
        if (m_glslManager) m_contextSharedAccess.unlock();
        return false;
    }

    return true;
}

void VideoWidget::bindShaderProgram()
{
    m_shader->bind();

    // C & D
    if (m_glslManager) {
        m_shader->setUniformValue(m_textureLocation[0], 0);
    } else {
        // A & B
        m_shader->setUniformValue(m_textureLocation[0], 0);
        m_shader->setUniformValue(m_textureLocation[1], 1);
        m_shader->setUniformValue(m_textureLocation[2], 2);
        m_shader->setUniformValue(m_colorspaceLocation, m_colorSpace);
    }
}

void VideoWidget::releaseSharedFrameTextures()
{
    // C & D
    if (m_glslManager) {
        glFinish();
        m_contextSharedAccess.unlock();
    }
}

bool VideoWidget::initGPUAccel()
{
    if (!KdenliveSettings::gpu_accel()) return false;

    m_glslManager = new Mlt::Filter(pCore->getProjectProfile(), "glsl.manager");
    return m_glslManager->is_valid();
}

// C & D
// TODO: insure safe, idempotent on all pipelines.
void VideoWidget::disableGPUAccel()
{
    delete m_glslManager;
    m_glslManager = nullptr;
    KdenliveSettings::setGpu_accel(false);
    // Need to destroy MLT global reference to prevent filters from trying to use GPU.
    mlt_properties_set_data(mlt_global_properties(), "glslManager", nullptr, 0, nullptr, nullptr);
    Q_EMIT gpuNotSupported();
}

bool VideoWidget::onlyGLESGPUAccel() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return (m_glslManager != nullptr) && quickWindow()->openglContext()->isOpenGLES();
#else
    QOpenGLContext &context = *static_cast< QOpenGLContext  *>(quickWindow()->rendererInterface()->getResource(quickWindow(), QSGRendererInterface::OpenGLContextResource));
    return (m_glslManager != nullptr) && context.isOpenGLES();
#endif
}

#if defined(Q_OS_WIN)
bool VideoWidget::initGPUAccelSync()
{
    // no-op
    // TODO: getProcAddress is not working on Windows?
    return false;
}
#else
bool VideoWidget::initGPUAccelSync()
{
    if (!KdenliveSettings::gpu_accel()) return false;
    if (m_glslManager == nullptr) return false;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!quickWindow()->openglContext()->hasExtension("GL_ARB_sync")) return false;

    m_ClientWaitSync = ClientWaitSync_fp(quickWindow()->openglContext()->getProcAddress("glClientWaitSync"));
#else
    QOpenGLContext &context = *static_cast< QOpenGLContext  *>(quickWindow()->rendererInterface()->getResource(quickWindow(), QSGRendererInterface::OpenGLContextResource));
    if (!context.hasExtension("GL_ARB_sync")) return false;

    m_ClientWaitSync = ClientWaitSync_fp(context.getProcAddress("glClientWaitSync"));
#endif
    if (m_ClientWaitSync) {
        return true;
    } else {
        qWarning() << "no GL sync";
        // fallback on A || B
        // TODO: fallback on A || B || C?
        disableGPUAccel();
        return false;
    }
}
#endif

void VideoWidget::paintGL()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QOpenGLFunctions *f = quickWindow()->openglContext()->functions();
#else
    QOpenGLContext &context = *static_cast< QOpenGLContext  *>(quickWindow()->rendererInterface()->getResource(quickWindow(), QSGRendererInterface::OpenGLContextResource));
    QOpenGLFunctions *f = context.functions();
#endif

    float width = this->width() * devicePixelRatioF();
    float height = this->height() * devicePixelRatioF();
    f->glClearColor(float(m_bgColor.redF()), float(m_bgColor.greenF()), float(m_bgColor.blueF()), 1);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    f->glDisable(GL_BLEND);
    f->glDisable(GL_DEPTH_TEST);
    f->glDepthMask(GL_FALSE);
    f->glViewport(0, qRound(m_displayRulerHeight * devicePixelRatioF() * 0.5), int(width), int(height));
    check_error(f);

    if (!acquireSharedFrameTextures()) return;

    // Bind textures.
    for (uint i = 0; i < 3; ++i) {
        if (m_texture[i] != 0u) {
            f->glActiveTexture(GL_TEXTURE0 + i);
            f->glBindTexture(GL_TEXTURE_2D, m_texture[i]);
            check_error(f);
        }
    }

    bindShaderProgram();
    check_error(f);

    // Setup an orthographic projection.
    QMatrix4x4 projection;
    projection.scale(2.0f / width, 2.0f / height);
    m_shader->setUniformValue(m_projectionLocation, projection);
    check_error(f);

    // Set model view.
    QMatrix4x4 modelView;
    if (!qFuzzyCompare(m_zoom, 1.0f)) {
        if ((offset().x() != 0) || (offset().y() != 0)) modelView.translate(-offset().x() * devicePixelRatioF(), offset().y() * devicePixelRatioF());
        modelView.scale(zoom(), zoom());
    }
    m_shader->setUniformValue(m_modelViewLocation, modelView);
    check_error(f);

    // Provide vertices of triangle strip.
    QVector<QVector2D> vertices;
    width = m_rect.width() * devicePixelRatioF();
    height = m_rect.height() * devicePixelRatioF();
    vertices << QVector2D(-width / 2.0f, -height / 2.0f);
    vertices << QVector2D(-width / 2.0f, height / 2.0f);
    vertices << QVector2D(width / 2.0f, -height / 2.0f);
    vertices << QVector2D(width / 2.0f, height / 2.0f);
    m_shader->enableAttributeArray(m_vertexLocation);
    check_error(f);
    m_shader->setAttributeArray(m_vertexLocation, vertices.constData());
    check_error(f);

    // Provide texture coordinates.
    QVector<QVector2D> texCoord;
    texCoord << QVector2D(0.0f, 1.0f);
    texCoord << QVector2D(0.0f, 0.0f);
    texCoord << QVector2D(1.0f, 1.0f);
    texCoord << QVector2D(1.0f, 0.0f);
    m_shader->enableAttributeArray(m_texCoordLocation);
    check_error(f);
    m_shader->setAttributeArray(m_texCoordLocation, texCoord.constData());
    check_error(f);

    // Render
    glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
    check_error(f);

    if (m_sendFrame && m_analyseSem.tryAcquire(1)) {
        // Render RGB frame for analysis
        if (!qFuzzyCompare(m_zoom, 1.0f)) {
            // Disable monitor zoom to render frame
            modelView = QMatrix4x4();
            m_shader->setUniformValue(m_modelViewLocation, modelView);
        }
        if ((m_fbo == nullptr) || m_fbo->size() != m_profileSize) {
            delete m_fbo;
            QOpenGLFramebufferObjectFormat fmt;
            fmt.setSamples(1);
            m_fbo = new QOpenGLFramebufferObject(m_profileSize.width(), m_profileSize.height(), fmt); // GL_TEXTURE_2D);
        }
        m_fbo->bind();
        glViewport(0, 0, m_profileSize.width(), m_profileSize.height());

        QMatrix4x4 projection2;
        projection2.scale(2.0f / width, 2.0f / height);
        m_shader->setUniformValue(m_projectionLocation, projection2);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
        check_error(f);
        m_fbo->release();
        Q_EMIT analyseFrame(m_fbo->toImage());
        m_sendFrame = false;
    }
    // Cleanup
    m_shader->disableAttributeArray(m_vertexLocation);
    m_shader->disableAttributeArray(m_texCoordLocation);
    m_shader->release();
    for (uint i = 0; i < 3; ++i) {
        if (m_texture[i] != 0u) {
            f->glActiveTexture(GL_TEXTURE0 + i);
            f->glBindTexture(GL_TEXTURE_2D, 0);
            check_error(f);
        }
    }
    glActiveTexture(GL_TEXTURE0);
    check_error(f);

    releaseSharedFrameTextures();
    check_error(f);
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
    resizeGL(width(), height());
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
    event->accept();
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
    (*thread) = new RenderThread(function, data, m_shareContext, &m_offscreenSurface);
    (*thread)->start();
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

    // C & D
    // TODO This is commented out for now because it is causing crashes.
    // Technically, this should be the correct thing to do, but it appears
    // some changes have created regression (see shotcut)
    // with respect to restarting the consumer in GPU mode.
    // m_glslManager->fire_event("close glsl");
    m_texture[0] = 0;
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
        resizeGL(width(), height());
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
        resizeGL(width(), height());
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
        delete m_threadStartEvent;
        m_threadStartEvent = nullptr;
        delete m_threadStopEvent;
        m_threadStopEvent = nullptr;

        delete m_threadCreateEvent;
        delete m_threadJoinEvent;
        if (m_glslManager) {
            m_threadCreateEvent = m_consumer->listen("consumer-thread-create", this, mlt_listener(onThreadCreate));
            m_threadJoinEvent = m_consumer->listen("consumer-thread-join", this, mlt_listener(onThreadJoin));
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
                m_threadStartEvent = m_consumer->listen("consumer-thread-started", this, mlt_listener(onThreadStarted));
            }
            if (!m_threadStopEvent) {
                m_threadStopEvent = m_consumer->listen("consumer-thread-stopped", this, mlt_listener(onThreadStopped));
            }
            if (!serviceName.startsWith(QLatin1String("decklink"))) {
                m_consumer->set("mlt_image_format", "glsl");
            }
        } else {
            // A & B
            m_consumer->set("mlt_image_format", "yuv422");
        }

        delete m_displayEvent;
        // C & D
        if (m_glslManager) {
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, mlt_listener(on_gl_frame_show));
        } else {
            // A & B
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, mlt_listener(on_frame_show));
        }

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
    resizeGL(width(), height());
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
    resizeGL(width(), height());
}

void VideoWidget::onFrameDisplayed(const SharedFrame &frame)
{
    m_contextSharedAccess.lock();
    m_sharedFrame = frame;
    m_sendFrame = sendFrameForAnalysis;
    m_contextSharedAccess.unlock();
    quickWindow()->update();
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
    if ((event->modifiers() & Qt::ControlModifier)) {
        event->accept();
        return;
    }
    if (!m_dragStart.isNull() && m_panStart.isNull() && ((event->button() & Qt::LeftButton) != 0u) && !event->isAccepted()) {
        event->accept();
        Q_EMIT monitorPlay();
    }
    m_dragStart = QPoint();
    m_panStart = QPoint();
    setCursor(Qt::ArrowCursor);
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

void VideoWidget::updateTexture(GLuint yName, GLuint uName, GLuint vName)
{
    m_texture[0] = yName;
    m_texture[1] = uName;
    m_texture[2] = vName;
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

void VideoWidget::on_gl_nosync_frame_show(mlt_consumer, VideoWidget *widget, mlt_event_data data)
{
    auto frame = Mlt::EventData(data).to_frame();
    if (frame.get_int("rendered") != 0) {
        int timeout = (widget->consumer()->get_int("real_time") > 0) ? 0 : 1000;
        if ((widget->m_frameRenderer != nullptr) && widget->m_frameRenderer->semaphore()->tryAcquire(1, timeout)) {
            QMetaObject::invokeMethod(widget->m_frameRenderer, "showGLNoSyncFrame", Qt::QueuedConnection, Q_ARG(Mlt::Frame, frame));
        }
    }
}

void VideoWidget::on_gl_frame_show(mlt_consumer, VideoWidget *widget, mlt_event_data data)
{
    auto frame = Mlt::EventData(data).to_frame();
    if (frame.get_int("rendered") != 0) {
        int timeout = (widget->consumer()->get_int("real_time") > 0) ? 0 : 1000;
        if ((widget->m_frameRenderer != nullptr) && widget->m_frameRenderer->semaphore()->tryAcquire(1, timeout)) {
            QMetaObject::invokeMethod(widget->m_frameRenderer, "showGLFrame", Qt::QueuedConnection, Q_ARG(Mlt::Frame, frame));
        }
    }
}

RenderThread::RenderThread(thread_function_t function, void *data, QOpenGLContext *context, QSurface *surface)
    : QThread(nullptr)
    , m_function(function)
    , m_data(data)
    , m_context(nullptr)
    , m_surface(surface)
{
    if (context) {
        m_context = new QOpenGLContext;
        m_context->setFormat(context->format());
        m_context->setShareContext(context);
        m_context->create();
        m_context->moveToThread(this);
    }
}

RenderThread::~RenderThread()
{
    // would otherwise leak if RenderThread is allocated with a context but not run.
    // safe post-run
    delete m_context;
}

// TODO: missing some exception handling?
void RenderThread::run()
{
    if (m_context) {
        m_context->makeCurrent(m_surface);
    }
    m_function(m_data);
    if (m_context) {
        m_context->doneCurrent();
        delete m_context;
        m_context = nullptr;
    }
}

FrameRenderer::FrameRenderer(QOpenGLContext *shareContext, QSurface *surface, VideoWidget::ClientWaitSync_fp clientWaitSync)
    : QThread(nullptr)
    , m_semaphore(3)
    , m_context(nullptr)
    , m_surface(surface)
    , m_ClientWaitSync(clientWaitSync)
    , m_gl32(nullptr)
    , sendAudioForAnalysis(false)
{
    Q_ASSERT(shareContext);
    m_renderTexture[0] = m_renderTexture[1] = m_renderTexture[2] = 0;
    m_displayTexture[0] = m_displayTexture[1] = m_displayTexture[2] = 0;
    // B & C & D
    if (KdenliveSettings::gpu_accel() || shareContext->supportsThreadedOpenGL()) {
        m_context = new QOpenGLContext;
        m_context->setFormat(shareContext->format());
        m_context->setShareContext(shareContext);
        m_context->create();
        m_context->moveToThread(this);
    }
    setObjectName(QStringLiteral("FrameRenderer"));
    moveToThread(this);
    start();
}

FrameRenderer::~FrameRenderer()
{
    delete m_context;
    delete m_gl32;
}

void FrameRenderer::showFrame(Mlt::Frame frame)
{
    // Save this frame for future use and to keep a reference to the GL Texture.
    m_displayFrame = SharedFrame(frame);

    if ((m_context != nullptr) && m_context->isValid()) {
        m_context->makeCurrent(m_surface);
        // Upload each plane of YUV to a texture.
        QOpenGLFunctions *f = m_context->functions();
        uploadTextures(m_context, m_displayFrame, m_renderTexture);
        f->glBindTexture(GL_TEXTURE_2D, 0);
        check_error(f);
        f->glFinish();

        for (int i = 0; i < 3; ++i) {
            std::swap(m_renderTexture[i], m_displayTexture[i]);
        }
        Q_EMIT textureReady(m_displayTexture[0], m_displayTexture[1], m_displayTexture[2]);
        m_context->doneCurrent();
    }
    // The frame is now done being modified and can be shared with the rest
    // of the application.
    Q_EMIT frameDisplayed(m_displayFrame);
    m_semaphore.release();
}

void FrameRenderer::showGLFrame(Mlt::Frame frame)
{
    if ((m_context != nullptr) && m_context->isValid()) {
        m_context->makeCurrent(m_surface);
        pipelineSyncToFrame(frame);

        m_context->functions()->glFinish();
        m_context->doneCurrent();

        // Save this frame for future use and to keep a reference to the GL Texture.
        m_displayFrame = SharedFrame(frame);
    }
    // The frame is now done being modified and can be shared with the rest
    // of the application.
    Q_EMIT frameDisplayed(m_displayFrame);
    m_semaphore.release();
}

void FrameRenderer::showGLNoSyncFrame(Mlt::Frame frame)
{
    if ((m_context != nullptr) && m_context->isValid()) {

        frame.set("movit.convert.use_texture", 1);
        m_context->makeCurrent(m_surface);
        m_context->functions()->glFinish();

        m_context->doneCurrent();

        // Save this frame for future use and to keep a reference to the GL Texture.
        m_displayFrame = SharedFrame(frame);
    }
    // The frame is now done being modified and can be shared with the rest
    // of the application.
    Q_EMIT frameDisplayed(m_displayFrame);
    m_semaphore.release();
}

void FrameRenderer::cleanup()
{
    if ((m_renderTexture[0] != 0u) && (m_renderTexture[1] != 0u) && (m_renderTexture[2] != 0u)) {
        m_context->makeCurrent(m_surface);
        m_context->functions()->glDeleteTextures(3, m_renderTexture);
        if ((m_displayTexture[0] != 0u) && (m_displayTexture[1] != 0u) && (m_displayTexture[2] != 0u)) {
            m_context->functions()->glDeleteTextures(3, m_displayTexture);
        }
        m_context->doneCurrent();
        m_renderTexture[0] = m_renderTexture[1] = m_renderTexture[2] = 0;
        m_displayTexture[0] = m_displayTexture[1] = m_displayTexture[2] = 0;
    }
}

// D
void FrameRenderer::pipelineSyncToFrame(Mlt::Frame &frame)
{
    auto sync = GLsync(frame.get_data("movit.convert.fence"));
    if (!sync) return;

#ifdef Q_OS_WIN
    // On Windows, use QOpenGLFunctions_3_2_Core instead of getProcAddress.
    // TODO: move to initialization of m_ClientWaitSync
    if (!m_gl32) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        // TODO: Qt6
        m_gl32 = m_context->versionFunctions<QOpenGLFunctions_3_2_Core>();
#endif
        if (m_gl32) {
            m_gl32->initializeOpenGLFunctions();
        }
    }
    if (m_gl32) {
        m_gl32->glClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
        check_error(m_context->functions());
    }
#else
    if (m_ClientWaitSync) {
        m_ClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
        check_error(m_context->functions());
    }
#endif // Q_OS_WIN
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

bool VideoWidget::playZone(bool loop)
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
        m_producer->seek(m_proxy->zoneIn());
        m_consumer->start();
        m_consumer->set("scrub_audio", 0);
        m_consumer->set("refresh", 1);
        m_consumer->set("volume", KdenliveSettings::volume() / 100.);
        m_producer->set_speed(1.0);
    } else {
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
        m_producer->set_speed(1.0);
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
        if (m_displayEvent) {
            delete m_displayEvent;
        }
        m_displayEvent = nullptr;
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
        resizeGL(width(), height());
    }
    return true;
}

void VideoWidget::switchRuler(bool show)
{
    m_rulerHeight = show ? int(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5) : 0;
    m_displayRulerHeight = m_rulerHeight;
    resizeGL(width(), height());
    Q_EMIT m_proxy->rulerHeightChanged();
}

const QStringList VideoWidget::getGPUInfo()
{
    if (!m_isInitialized) {
        return {};
    }
    return {QString::fromUtf8((const char *)glGetString(GL_VENDOR)), QString::fromUtf8((const char *)glGetString(GL_RENDERER))};
}
