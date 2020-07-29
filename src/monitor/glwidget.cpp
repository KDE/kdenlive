/*
 * Copyright (c) 2011-2016 Meltytech, LLC
 * Original author: Dan Dennedy <dan@dennedy.org>
 * Modified for Kdenlive: Jean-Baptiste Mardelle
 *
 * GL shader based on BSD licensed code from Peter Bengtsson:
 * https://www.fourcc.org/source/YUV420P-OpenGL-GLSLang.c
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

#include <KDeclarative/KDeclarative>
#include <KMessageBox>
#include <QApplication>
#include <QOpenGLFunctions_3_2_Core>
#include <QPainter>
#include <QQmlContext>
#include <QQuickItem>
#include <QFontDatabase>
#include <kdeclarative_version.h>
#include <klocalizedstring.h>

#include "core.h"
#include "glwidget.h"
#include "kdenlivesettings.h"
#include "monitorproxy.h"
#include "profiles/profilemodel.hpp"
#include "timeline2/view/qml/timelineitems.h"
#include <mlt++/Mlt.h>
#include <lib/localeHandling.h>

#ifndef GL_UNPACK_ROW_LENGTH
#ifdef GL_UNPACK_ROW_LENGTH_EXT
#define GL_UNPACK_ROW_LENGTH GL_UNPACK_ROW_LENGTH_EXT
#else
#error GL_UNPACK_ROW_LENGTH undefined
#endif
#endif

#if 1
#define check_error(fn) {}
#else
#define check_error(fn) { int err = fn->glGetError(); if (err != GL_NO_ERROR) { qCritical(KDENLIVE_LOG) << "GL error"  << hex << err << dec << "at" << __FILE__ << ":" << __LINE__; } }
#endif


#ifndef GL_TIMEOUT_IGNORED
#define GL_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFFull
#endif

using namespace Mlt;

GLWidget::GLWidget(int id, QObject *parent)
    : QQuickView((QWindow *)parent)
    , sendFrameForAnalysis(false)
    , m_glslManager(nullptr)
    , m_consumer(nullptr)
    , m_producer(nullptr)
    , m_id(id)
    , m_rulerHeight(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5)
    , m_bgColor(KdenliveSettings::window_background())
    , m_shader(nullptr)
    , m_initSem(0)
    , m_analyseSem(1)
    , m_isInitialized(false)
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
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
    kdeclarative.setupContext();

    m_texture[0] = m_texture[1] = m_texture[2] = 0;
    qRegisterMetaType<Mlt::Frame>("Mlt::Frame");
    qRegisterMetaType<SharedFrame>("SharedFrame");

    if (m_id == Kdenlive::ClipMonitor && !(KdenliveSettings::displayClipMonitorInfo() & 0x01)) {
        m_rulerHeight = 0;
    } else if (!(KdenliveSettings::displayProjectMonitorInfo() & 0x01)) {
        m_rulerHeight = 0;
    }
    m_displayRulerHeight = m_rulerHeight;

    setPersistentOpenGLContext(true);
    setPersistentSceneGraph(true);
    setClearBeforeRendering(false);
    setResizeMode(QQuickView::SizeRootObjectToView);

    m_offscreenSurface.setFormat(QWindow::format());
    m_offscreenSurface.create();

    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(50);
    m_blackClip.reset(new Mlt::Producer(pCore->getCurrentProfile()->profile(), "color:0"));
    m_blackClip->set("kdenlive:id", "black");
    m_blackClip->set("out", 3);
    connect(&m_refreshTimer, &QTimer::timeout, this, &GLWidget::refresh);
    m_producer = m_blackClip;
    rootContext()->setContextProperty("markersModel", 0);
    if (!initGPUAccel()) {
        disableGPUAccel();
    }

    connect(this, &QQuickWindow::sceneGraphInitialized, this, &GLWidget::initializeGL, Qt::DirectConnection);
    connect(this, &QQuickWindow::beforeRendering, this, &GLWidget::paintGL, Qt::DirectConnection);
    connect(pCore.get(), &Core::updateMonitorProfile, this, &GLWidget::reloadProfile);

    registerTimelineItems();
    m_proxy = new MonitorProxy(this);
    rootContext()->setContextProperty("controller", m_proxy);
}

GLWidget::~GLWidget()
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

void GLWidget::updateAudioForAnalysis()
{
    if (m_frameRenderer) {
        m_frameRenderer->sendAudioForAnalysis = KdenliveSettings::monitor_audio();
    }
}

void GLWidget::initializeGL()
{
    if (m_isInitialized) return;

    openglContext()->makeCurrent(&m_offscreenSurface);
    initializeOpenGLFunctions();

    qCDebug(KDENLIVE_LOG) << "OpenGL vendor: " << QString::fromUtf8((const char *)glGetString(GL_VENDOR));
    qCDebug(KDENLIVE_LOG) << "OpenGL renderer: " << QString::fromUtf8((const char *)glGetString(GL_RENDERER));
    qCDebug(KDENLIVE_LOG) << "OpenGL Threaded: " << openglContext()->supportsThreadedOpenGL();
    qCDebug(KDENLIVE_LOG) << "OpenGL ARG_SYNC: " << openglContext()->hasExtension("GL_ARB_sync");
    qCDebug(KDENLIVE_LOG) << "OpenGL OpenGLES: " << openglContext()->isOpenGLES();

    // C & D
    if (onlyGLESGPUAccel()) {
        disableGPUAccel();
    }

    createShader();

    m_openGLSync = initGPUAccelSync();

    // C & D
    if (m_glslManager) {
        // Create a context sharing with this context for the RenderThread context.
        // This is needed because openglContext() is active in another thread
        // at the time that RenderThread is created.
        // See this Qt bug for more info: https://bugreports.qt.io/browse/QTBUG-44677
        // TODO: QTBUG-44677 is closed. still applicable?
        m_shareContext = new QOpenGLContext;
        m_shareContext->setFormat(openglContext()->format());
        m_shareContext->setShareContext(openglContext());
        m_shareContext->create();
    }

    m_frameRenderer = new FrameRenderer(openglContext(), &m_offscreenSurface, m_ClientWaitSync);

    m_frameRenderer->sendAudioForAnalysis = KdenliveSettings::monitor_audio();

    openglContext()->makeCurrent(this);
    connect(m_frameRenderer, &FrameRenderer::textureReady, this, &GLWidget::updateTexture, Qt::DirectConnection);
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &GLWidget::onFrameDisplayed, Qt::QueuedConnection);
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &GLWidget::frameDisplayed, Qt::QueuedConnection);
    m_initSem.release();
    m_isInitialized = true;
    reconfigure();
}

void GLWidget::resizeGL(int width, int height)
{
    int x, y, w, h;
    height -= m_displayRulerHeight;
    double this_aspect = (double)width / height;

    // Special case optimization to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if ((int)(this_aspect * 1000) == (int)(m_dar * 1000)) {
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
    x = (width - w) / 2;
    y = (height - h) / 2;
    m_rect.setRect(x, y, w, h);
    QQuickItem *rootQml = rootObject();
    if (rootQml) {
        QSize s = pCore->getCurrentFrameSize();
        double scalex = (double)m_rect.width() / s.width() * m_zoom;
        double scaley = (double)m_rect.height() / s.height() * m_zoom;
        rootQml->setProperty("center", m_rect.center());
        rootQml->setProperty("scalex", scalex);
        rootQml->setProperty("scaley", scaley);
        if (rootQml->objectName() == QLatin1String("rootsplit")) {
            // Adjust splitter pos
            rootQml->setProperty("splitterPos", x + (rootQml->property("percentage").toDouble() * w));
        }
    }
    emit rectChanged();
}

void GLWidget::resizeEvent(QResizeEvent *event)
{
    resizeGL(event->size().width(), event->size().height());
    QQuickView::resizeEvent(event);
}

void GLWidget::createGPUAccelFragmentProg()
{
    m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, "uniform sampler2D tex;"
                                                               "varying highp vec2 coordinates;"
                                                               "void main(void) {"
                                                               "  gl_FragColor = texture2D(tex, coordinates);"
                                                               "}");
    m_shader->link();
    m_textureLocation[0] = m_shader->uniformLocation("tex");
}

void GLWidget::createShader()
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

void GLWidget::createYUVTextureProjectFragmentProg()
{
    m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                      "uniform sampler2D Ytex, Utex, Vtex;"
                                      "uniform lowp int colorspace;"
                                      "varying highp vec2 coordinates;"
                                      "void main(void) {"
                                      "  mediump vec3 texel;"
                                      "  texel.r = texture2D(Ytex, coordinates).r - 0.0625;" // Y
                                      "  texel.g = texture2D(Utex, coordinates).r - 0.5;"    // U
                                      "  texel.b = texture2D(Vtex, coordinates).r - 0.5;"    // V
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

void GLWidget::clear()
{
    stopGlsl();
    update();
}

void GLWidget::releaseAnalyse()
{
    m_analyseSem.release();
}

bool GLWidget::acquireSharedFrameTextures()
{
    // A
    if ((m_glslManager == nullptr) && !openglContext()->supportsThreadedOpenGL()) {
        QMutexLocker locker(&m_contextSharedAccess);
        if (!m_sharedFrame.is_valid()) {
            return false;
        }
        uploadTextures(openglContext(), m_sharedFrame, m_texture);
    } else if (m_glslManager) {
        // C & D
        m_contextSharedAccess.lock();
        if (m_sharedFrame.is_valid()) {
            m_texture[0] = *((const GLuint *)m_sharedFrame.get_image(mlt_image_glsl_texture));
        }
    }

    if (!m_texture[0]) {
        // C & D
        if (m_glslManager) m_contextSharedAccess.unlock();
        return false;
    }

    return true;
}

void GLWidget::bindShaderProgram()
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

void GLWidget::releaseSharedFrameTextures()
{
    // C & D
    if (m_glslManager) {
        glFinish();
        m_contextSharedAccess.unlock();
    }
}

bool GLWidget::initGPUAccel()
{
    if (!KdenliveSettings::gpu_accel()) return false;

    m_glslManager = new Mlt::Filter(pCore->getCurrentProfile()->profile(), "glsl.manager");
    return m_glslManager->is_valid();
}

// C & D
// TODO: insure safe, idempotent on all pipelines.
void GLWidget::disableGPUAccel()
{
    delete m_glslManager;
    m_glslManager = nullptr;
    KdenliveSettings::setGpu_accel(false);
    // Need to destroy MLT global reference to prevent filters from trying to use GPU.
    mlt_properties_set_data(mlt_global_properties(), "glslManager", nullptr, 0, nullptr, nullptr);
    emit gpuNotSupported();
}

bool GLWidget::onlyGLESGPUAccel() const
{
    return (m_glslManager != nullptr) && openglContext()->isOpenGLES();
}

#if defined(Q_OS_WIN)
bool GLWidget::initGPUAccelSync()
{
    // no-op
    // TODO: getProcAddress is not working on Windows?
    return false;
}
#else
bool GLWidget::initGPUAccelSync()
{
    if (!KdenliveSettings::gpu_accel()) return false;
    if (m_glslManager == nullptr) return false;
    if (!openglContext()->hasExtension("GL_ARB_sync")) return false;

    m_ClientWaitSync = (ClientWaitSync_fp)openglContext()->getProcAddress("glClientWaitSync");
    if (m_ClientWaitSync) {
        return true;
    } else {
        qCDebug(KDENLIVE_LOG) << "  / / // NO GL SYNC, ERROR";
        // fallback on A || B
        // TODO: fallback on A || B || C?
        disableGPUAccel();
        return false;
    }
}
#endif

void GLWidget::paintGL()
{
    QOpenGLFunctions *f = openglContext()->functions();
    float width = this->width() * devicePixelRatio();
    float height = this->height() * devicePixelRatio();

    f->glDisable(GL_BLEND);
    f->glDisable(GL_DEPTH_TEST);
    f->glDepthMask(GL_FALSE);
    f->glViewport(0, (m_displayRulerHeight * devicePixelRatio() * 0.5 + 0.5), width, height);
    check_error(f);
    f->glClearColor(m_bgColor.redF(), m_bgColor.greenF(), m_bgColor.blueF(), 0);
    f->glClear(GL_COLOR_BUFFER_BIT);
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
        if ((offset().x() != 0) || (offset().y() != 0)) modelView.translate(-offset().x() * devicePixelRatio(), offset().y() * devicePixelRatio());
        modelView.scale(zoom(), zoom());
    }
    m_shader->setUniformValue(m_modelViewLocation, modelView);
    check_error(f);

    // Provide vertices of triangle strip.
    QVector<QVector2D> vertices;
    width = m_rect.width() * devicePixelRatio();
    height = m_rect.height() * devicePixelRatio();
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
        if ((m_fbo == nullptr) || m_fbo->size() != m_profileSize) {
            delete m_fbo;
            QOpenGLFramebufferObjectFormat fmt;
            fmt.setSamples(1);
            fmt.setInternalTextureFormat(GL_RGB);                             // GL_RGBA32F);  // which one is the fastest ?
            m_fbo = new QOpenGLFramebufferObject(m_profileSize.width(), m_profileSize.height(), fmt); // GL_TEXTURE_2D);
        }
        m_fbo->bind();
        glViewport(0, 0, m_profileSize.width(), m_profileSize.height());

        QMatrix4x4 projection2;
        projection2.scale(2.0f / (float)width, 2.0f / (float)height);
        m_shader->setUniformValue(m_projectionLocation, projection2);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
        check_error(f);
        m_fbo->release();
        emit analyseFrame(m_fbo->toImage());
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

void GLWidget::slotZoom(bool zoomIn)
{
    if (zoomIn) {
        if (qFuzzyCompare(m_zoom, 1.0f)) {
            setZoom(2.0f);
        } else if (qFuzzyCompare(m_zoom, 2.0f)) {
            setZoom(3.0f);
        } else if (m_zoom < 1.0f) {
            setZoom(m_zoom * 2);
        }
    } else {
        if (qFuzzyCompare(m_zoom, 3.0f)) {
            setZoom(2.0);
        } else if (qFuzzyCompare(m_zoom, 2.0f)) {
            setZoom(1.0);
        } else if (m_zoom > 0.2) {
            setZoom(m_zoom / 2);
        }
    }
}

void GLWidget::updateRulerHeight(int addedHeight)
{
    m_displayRulerHeight =  m_rulerHeight > 0 ? QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5 + addedHeight : 0;
    resizeGL(width(), height());
}

void GLWidget::requestSeek(int position)
{
    m_consumer->set("scrub_audio", 1);
    m_producer->seek(position);
    if (!qFuzzyIsNull(m_producer->get_speed())) {
        m_consumer->purge();
    }
    restartConsumer();
    m_consumer->set("refresh", 1);
}

void GLWidget::requestRefresh()
{
    if (m_producer && qFuzzyIsNull(m_producer->get_speed())) {
        m_consumer->set("scrub_audio", 0);
        m_refreshTimer.start();
    }
}

QString GLWidget::frameToTime(int frames) const
{
    return m_consumer ? m_consumer->frames_to_time(frames, mlt_time_smpte_df) : QStringLiteral("-");
}

void GLWidget::refresh()
{
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mltMutex);
    restartConsumer();
    m_consumer->set("refresh", 1);
}

bool GLWidget::checkFrameNumber(int pos, int offset, bool isPlaying)
{
    const double speed = m_producer->get_speed();
    m_proxy->positionFromConsumer(pos, isPlaying);
    int maxPos = m_producer->get_int("out");
    if (m_isLoopMode || m_isZoneMode) {
        if (isPlaying && pos >= maxPos) {
            m_consumer->purge();
            if (!m_isLoopMode) {
                return false;
            }
            m_producer->seek(m_isZoneMode ? m_proxy->zoneIn() : m_loopIn);
            m_producer->set_speed(1.0);
            m_consumer->set("refresh", 1);
            return true;
        }
        return true;
    } else if (isPlaying) {
        maxPos -= offset;
        if (pos >= (maxPos - 1) && !(speed < 0.)) {
            // Playing past last clip, pause
            m_producer->set_speed(0);
            m_consumer->set("refresh", 0);
            m_consumer->purge();
            m_proxy->setPosition(qMax(0, maxPos));
            m_producer->seek(qMax(0, maxPos));
            return false;
        } else if (pos <= 0 && speed < 0.) {
            // rewinding reached 0, pause
            m_producer->set_speed(0);
            m_consumer->set("refresh", 0);
            m_consumer->purge();
            m_proxy->setPosition(0);
            m_producer->seek(0);
            return false;
        }
    }
    return isPlaying;
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    if ((rootObject() != nullptr) && rootObject()->objectName() != QLatin1String("root") && !(event->modifiers() & Qt::ControlModifier) &&
        !(event->buttons() & Qt::MiddleButton)) {
        event->ignore();
        QQuickView::mousePressEvent(event);
        return;
    }
    if ((event->button() & Qt::LeftButton) != 0u) {
        if ((event->modifiers() & Qt::ControlModifier) != 0u) {
            // Pan view
            m_panStart = event->pos();
            setCursor(Qt::ClosedHandCursor);
        } else {
            m_dragStart = event->pos();
        }
    } else if ((event->button() & Qt::RightButton) != 0u) {
        emit showContextMenu(event->globalPos());
    } else if ((event->button() & Qt::MiddleButton) != 0u) {
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    event->accept();
    QQuickView::mousePressEvent(event);
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if ((rootObject() != nullptr) && rootObject()->objectName() != QLatin1String("root") && !(event->modifiers() & Qt::ControlModifier) &&
        !(event->buttons() & Qt::MiddleButton)) {
        event->ignore();
        QQuickView::mouseMoveEvent(event);
        return;
    }
    /*    if (event->modifiers() == Qt::ShiftModifier && m_producer) {
        emit seekTo(m_producer->get_length() *  event->x() / width());
        return;
    }*/
    QQuickView::mouseMoveEvent(event);
    if (!m_panStart.isNull()) {
        emit panView(m_panStart - event->pos());
        m_panStart = event->pos();
        event->accept();
        QQuickView::mouseMoveEvent(event);
        return;
    }
    if (!(event->buttons() & Qt::LeftButton)) {
        QQuickView::mouseMoveEvent(event);
        return;
    }
    if (!event->isAccepted() && !m_dragStart.isNull() && (event->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragStart = QPoint();
        emit startDrag();
    }
}

void GLWidget::keyPressEvent(QKeyEvent *event)
{
    QQuickView::keyPressEvent(event);
    if (!event->isAccepted()) {
        emit passKeyEvent(event);
    }
}

void GLWidget::createThread(RenderThread **thread, thread_function_t function, void *data)
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

static void onThreadCreate(mlt_properties owner, GLWidget *self, RenderThread **thread, int *priority, thread_function_t function, void *data)
{
    Q_UNUSED(owner)
    Q_UNUSED(priority)
    // self->clearFrameRenderer();
    self->createThread(thread, function, data);
    self->lockMonitor();
}

static void onThreadJoin(mlt_properties owner, GLWidget *self, RenderThread *thread)
{
    Q_UNUSED(owner)
    if (thread) {
        thread->quit();
        thread->wait();
        delete thread;
        // self->clearFrameRenderer();
        self->releaseMonitor();
    }
}

void GLWidget::startGlsl()
{
    // C & D
    if (m_glslManager) {
        // clearFrameRenderer();
        m_glslManager->fire_event("init glsl");
        if (m_glslManager->get_int("glsl_supported") == 0) {
            disableGPUAccel();
        } else {
            emit started();
        }
    }
}

static void onThreadStarted(mlt_properties owner, GLWidget *self)
{
    Q_UNUSED(owner)
    self->startGlsl();
}

void GLWidget::releaseMonitor()
{
    emit lockMonitor(false);
}

void GLWidget::lockMonitor()
{
    emit lockMonitor(true);
}

void GLWidget::stopGlsl()
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

static void onThreadStopped(mlt_properties owner, GLWidget *self)
{
    Q_UNUSED(owner)
    self->stopGlsl();
}

int GLWidget::setProducer(const QString &file)
{
    if (m_producer) {
        m_producer.reset();
    }
    qDebug()<<"==== OPENING PROIDUCER FILE: "<<file;
    m_producer = std::make_shared<Mlt::Producer>(new Mlt::Producer(pCore->getCurrentProfile()->profile(), nullptr, file.toUtf8().constData()));
    if (m_consumer) {
        //m_consumer->stop();
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

int GLWidget::setProducer(const std::shared_ptr<Mlt::Producer> &producer, bool isActive, int position)
{
    int error = 0;
    QString currentId;
    int consumerPosition = 0;
    currentId = m_producer->parent().get("kdenlive:id");
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
        rootContext()->setContextProperty("markersModel", 0);
    }
    // redundant check. postcondition of above is m_producer != null
    m_producer->set_speed(0);
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
    }
    m_consumer->set("scrub_audio", 0);
    m_proxy->setPosition(position > 0 ? position : m_producer->position());
    return error;
}

int GLWidget::droppedFrames() const
{
    return (m_consumer ? m_consumer->get_int("drop_count") : 0);
}

void GLWidget::resetDrops()
{
    if (m_consumer) {
        m_consumer->set("drop_count", 0);
    }
}

void GLWidget::stopCapture()
{
    if (strcmp(m_consumer->get("mlt_service"), "multi") == 0) {
        m_consumer->set("refresh", 0);
        m_consumer->purge();
        m_consumer->stop();
    }
}

int GLWidget::reconfigureMulti(const QString &params, const QString &path, Mlt::Profile *profile)
{
    Q_UNUSED(params);
    Q_UNUSED(path);
    Q_UNUSED(profile);
    // TODO Fix or delete
    /*
    QString serviceName = property("mlt_service").toString();
    if ((m_consumer == nullptr) || !m_consumer->is_valid() || strcmp(m_consumer->get("mlt_service"), "multi") != 0) {
        if (m_consumer) {
            m_consumer->purge();
            m_consumer->stop();
            m_consumer.reset();
        }
        m_consumer.reset(new Mlt::FilteredConsumer(*profile, "multi"));
        delete m_threadStartEvent;
        m_threadStartEvent = nullptr;
        delete m_threadStopEvent;
        m_threadStopEvent = nullptr;

        delete m_threadCreateEvent;
        delete m_threadJoinEvent;
        if (m_consumer) {
            m_threadCreateEvent = m_consumer->listen("consumer-thread-create", this, (mlt_listener)onThreadCreate);
            m_threadJoinEvent = m_consumer->listen("consumer-thread-join", this, (mlt_listener)onThreadJoin);
        }
    }
    if (m_consumer->is_valid()) {
        // build sub consumers
        // m_consumer->set("mlt_image_format", "yuv422");
        reloadProfile();
        int volume = KdenliveSettings::volume();
        m_consumer->set("0", serviceName.toUtf8().constData());
        m_consumer->set("0.mlt_image_format", "yuv422");
        m_consumer->set("0.terminate_on_pause", 0);
        // m_consumer->set("0.preview_off", 1);
        m_consumer->set("0.real_time", 0);
        m_consumer->set("0.volume", (double)volume / 100);

        if (serviceName.startsWith(QLatin1String("sdl_audio"))) {
#ifdef Q_OS_WIN
            m_consumer->set("0.audio_buffer", 2048);
#else
            m_consumer->set("0.audio_buffer", 512);
#endif
            QString audioDevice = KdenliveSettings::audiodevicename();
            if (!audioDevice.isEmpty()) {
                m_consumer->set("audio_device", audioDevice.toUtf8().constData());
            }

            QString audioDriver = KdenliveSettings::audiodrivername();
            if (!audioDriver.isEmpty()) {
                m_consumer->set("audio_driver", audioDriver.toUtf8().constData());
            }
        }

        m_consumer->set("1", "avformat");
        m_consumer->set("1.target", path.toUtf8().constData());
        // m_consumer->set("1.real_time", -KdenliveSettings::mltthreads());
        m_consumer->set("terminate_on_pause", 0);
        m_consumer->set("1.terminate_on_pause", 0);

        // m_consumer->set("1.terminate_on_pause", 0);// was commented out. restoring it  fixes mantis#3415 - FFmpeg recording freezes
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList paramList = params.split(' ', QString::SkipEmptyParts);
#else
        QStringList paramList = params.split(' ', Qt::SkipEmptyParts);
#endif
        for (int i = 0; i < paramList.count(); ++i) {
            QString key = "1." + paramList.at(i).section(QLatin1Char('='), 0, 0);
            QString value = paramList.at(i).section(QLatin1Char('='), 1, 1);
            if (value == QLatin1String("%threads")) {
                value = QString::number(QThread::idealThreadCount());
            }
            m_consumer->set(key.toUtf8().constData(), value.toUtf8().constData());
        }
        // Connect the producer to the consumer - tell it to "run" later
        delete m_displayEvent;
        // C & D
        if (m_glslManager) {
            // D
            if (m_openGLSync) {
                m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_gl_frame_show);
            } else {
                // C
                m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_gl_nosync_frame_show);
            }
        } else {
            // A & B
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_frame_show);
        }
        m_consumer->connect(*m_producer.get());
        m_consumer->start();
        return 0;
    }
    */
    return -1;
}

int GLWidget::reconfigure()
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
            m_consumer.reset(new Mlt::FilteredConsumer(*pCore->getProjectProfile(), audioBackend.toLatin1().constData()));
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
                    m_consumer.reset(new Mlt::FilteredConsumer(*pCore->getProjectProfile(), bk.toLatin1().constData()));
                    if (m_consumer->is_valid()) {
                        if (audioBackend == KdenliveSettings::sdlAudioBackend()) {
                            // switch sdl audio backend
                            KdenliveSettings::setSdlAudioBackend(bk);
                        }
                        qDebug() << "++++++++\nSwitching audio backend to: " << bk << "\n++++++++++";
                        KdenliveSettings::setAudiobackend(bk);
                        serviceName = bk;
                        break;
                    } else {
                        m_consumer.reset();
                    }
                }
            }
            if (!m_consumer || !m_consumer->is_valid()) {
                qWarning() << "WARNING, NO AUDIO BACKEND FOUND";
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
        if (m_consumer) {
            m_threadCreateEvent = m_consumer->listen("consumer-thread-create", this, (mlt_listener)onThreadCreate);
            m_threadJoinEvent = m_consumer->listen("consumer-thread-join", this, (mlt_listener)onThreadJoin);
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
                m_threadStartEvent = m_consumer->listen("consumer-thread-started", this, (mlt_listener)onThreadStarted);
            }
            if (!m_threadStopEvent) {
                m_threadStopEvent = m_consumer->listen("consumer-thread-stopped", this, (mlt_listener)onThreadStopped);
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
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_gl_frame_show);
        } else {
            // A & B
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_frame_show);
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
        /*if (!pCore->getCurrentProfile()->progressive())
            m_consumer->set("progressive", property("progressive").toBool());*/
        m_consumer->set("volume", volume / 100.0);
        // m_consumer->set("progressive", 1);
        m_consumer->set("rescale", KdenliveSettings::mltinterpolation().toUtf8().constData());
        m_consumer->set("deinterlace_method", KdenliveSettings::mltdeinterlacer().toUtf8().constData());
        /*
#ifdef Q_OS_WIN
        m_consumer->set("audio_buffer", 2048);
#else
        m_consumer->set("audio_buffer", 512);
#endif
        */
        int fps = qRound(pCore->getCurrentFps());
        m_consumer->set("buffer", qMax(25, fps));
        m_consumer->set("prefill", qMax(1, fps / 25));
        m_consumer->set("drop_max", fps / 4);
        m_consumer->set("scrub_audio", 1);
        m_consumer->set("channels", 2);
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

float GLWidget::zoom() const
{
    return m_zoom;
}

void GLWidget::reloadProfile()
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
    m_blackClip.reset(new Mlt::Producer(pCore->getCurrentProfile()->profile(), "color:0"));
    m_blackClip->set("kdenlive:id", "black");
    if (existingConsumer) {
        reconfigure();
    }
    resizeGL(width(), height());
    refreshSceneLayout();
}

QSize GLWidget::profileSize() const
{
    return m_profileSize;
}

QRect GLWidget::displayRect() const
{
    return m_rect;
}

QPoint GLWidget::offset() const
{
    return {m_offset.x() - ((int)((float)m_profileSize.width() * m_zoom) - width()) / 2,
            m_offset.y() - ((int)((float)m_profileSize.height() * m_zoom) - height()) / 2};
}

void GLWidget::setZoom(float zoom)
{
    double zoomRatio = zoom / m_zoom;
    m_zoom = zoom;
    emit zoomChanged();
    if (rootObject()) {
        rootObject()->setProperty("zoom", m_zoom);
        double scalex = rootObject()->property("scalex").toDouble() * zoomRatio;
        rootObject()->setProperty("scalex", scalex);
        double scaley = rootObject()->property("scaley").toDouble() * zoomRatio;
        rootObject()->setProperty("scaley", scaley);
    }
    update();
}

void GLWidget::onFrameDisplayed(const SharedFrame &frame)
{
    m_contextSharedAccess.lock();
    m_sharedFrame = frame;
    m_sendFrame = sendFrameForAnalysis;
    m_contextSharedAccess.unlock();
    update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QQuickView::mouseReleaseEvent(event);
    if (m_dragStart.isNull() && m_panStart.isNull() && (rootObject() != nullptr) && rootObject()->objectName() != QLatin1String("root") &&
        !(event->modifiers() & Qt::ControlModifier)) {
        event->ignore();
        return;
    }
    if (!m_dragStart.isNull() && m_panStart.isNull() && ((event->button() & Qt::LeftButton) != 0u) && !event->isAccepted()) {
        emit monitorPlay();
    }
    m_dragStart = QPoint();
    m_panStart = QPoint();
    setCursor(Qt::ArrowCursor);
}

void GLWidget::purgeCache()
{
    if (m_consumer) {
        m_consumer->purge();
        m_producer->seek(m_proxy->getPosition() + 1);
    }
}

void GLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QQuickView::mouseDoubleClickEvent(event);
    if (event->isAccepted()) {
        return;
    }
    if ((rootObject() == nullptr) || rootObject()->objectName() != QLatin1String("rooteffectscene")) {
        emit switchFullScreen();
    }
    event->accept();
}

void GLWidget::setOffsetX(int x, int max)
{
    m_offset.setX(x);
    emit offsetChanged();
    if (rootObject()) {
        rootObject()->setProperty("offsetx", m_zoom > 1.0f ? x - max / 2.0 - 10 : 0);
    }
    update();
}

void GLWidget::setOffsetY(int y, int max)
{
    m_offset.setY(y);
    if (rootObject()) {
        rootObject()->setProperty("offsety", m_zoom > 1.0f ? y - max / 2.0 - 10 : 0);
    }
    update();
}

std::shared_ptr<Mlt::Consumer> GLWidget::consumer()
{
    return m_consumer;
}

void GLWidget::updateGamma()
{
    reconfigure();
}

void GLWidget::resetConsumer(bool fullReset)
{
    if (fullReset && m_consumer) {
        m_consumer->purge();
        m_consumer->stop();
        m_consumer.reset();
    }
    reconfigure();
}

const QString GLWidget::sceneList(const QString &root, const QString &fullPath)
{
    LocaleHandling::resetLocale();
    QString playlist;
    qCDebug(KDENLIVE_LOG) << " * * *Setting document xml root: " << root;
    Mlt::Consumer xmlConsumer(pCore->getCurrentProfile()->profile(), "xml", fullPath.isEmpty() ? "kdenlive_playlist" : fullPath.toUtf8().constData());
    if (!root.isEmpty()) {
        xmlConsumer.set("root", root.toUtf8().constData());
    }
    if (!xmlConsumer.is_valid()) {
        return QString();
    }
    xmlConsumer.set("store", "kdenlive");
    xmlConsumer.set("time_format", "clock");
    // Disabling meta creates cleaner files, but then we don't have access to metadata on the fly (meta channels, etc)
    // And we must use "avformat" instead of "avformat-novalidate" on project loading which causes a big delay on project opening
    // xmlConsumer.set("no_meta", 1);
    xmlConsumer.connect(*m_producer.get());
    xmlConsumer.run();
    playlist = fullPath.isEmpty() ? QString::fromUtf8(xmlConsumer.get("kdenlive_playlist")) : fullPath;
    return playlist;
}

void GLWidget::updateTexture(GLuint yName, GLuint uName, GLuint vName)
{
    m_texture[0] = yName;
    m_texture[1] = uName;
    m_texture[2] = vName;
}

void GLWidget::on_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    if (frame.get_int("rendered") != 0) {
        auto *widget = static_cast<GLWidget *>(self);
        int timeout = (widget->consumer()->get_int("real_time") > 0) ? 0 : 1000;
        if ((widget->m_frameRenderer != nullptr) && widget->m_frameRenderer->semaphore()->tryAcquire(1, timeout)) {
            QMetaObject::invokeMethod(widget->m_frameRenderer, "showFrame", Qt::QueuedConnection, Q_ARG(Mlt::Frame, frame));
        }
    }
}

void GLWidget::on_gl_nosync_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    if (frame.get_int("rendered") != 0) {
        auto *widget = static_cast<GLWidget *>(self);
        int timeout = (widget->consumer()->get_int("real_time") > 0) ? 0 : 1000;
        if ((widget->m_frameRenderer != nullptr) && widget->m_frameRenderer->semaphore()->tryAcquire(1, timeout)) {
            QMetaObject::invokeMethod(widget->m_frameRenderer, "showGLNoSyncFrame", Qt::QueuedConnection, Q_ARG(Mlt::Frame, frame));
        }
    }
}

void GLWidget::on_gl_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    if (frame.get_int("rendered") != 0) {
        auto *widget = static_cast<GLWidget *>(self);
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

FrameRenderer::FrameRenderer(QOpenGLContext *shareContext, QSurface *surface, GLWidget::ClientWaitSync_fp clientWaitSync)
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
        emit textureReady(m_displayTexture[0], m_displayTexture[1], m_displayTexture[2]);
        m_context->doneCurrent();
    }
    // The frame is now done being modified and can be shared with the rest
    // of the application.
    emit frameDisplayed(m_displayFrame);
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
    emit frameDisplayed(m_displayFrame);
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
    emit frameDisplayed(m_displayFrame);
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
    auto sync = (GLsync)frame.get_data("movit.convert.fence");
    if (!sync) return;

#ifdef Q_OS_WIN
    // On Windows, use QOpenGLFunctions_3_2_Core instead of getProcAddress.
    // TODO: move to initialization of m_ClientWaitSync
    if (!m_gl32) {
        m_gl32 = m_context->versionFunctions<QOpenGLFunctions_3_2_Core>();
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

void GLWidget::refreshSceneLayout()
{
    if (!rootObject()) {
        return;
    }
    QSize s = pCore->getCurrentFrameSize();
    emit m_proxy->profileChanged();
    rootObject()->setProperty("scalex", (double)m_rect.width() / s.width() * m_zoom);
    rootObject()->setProperty("scaley", (double)m_rect.height() / s.height() * m_zoom);
}

void GLWidget::switchPlay(bool play, double speed)
{
    if (!m_producer || !m_consumer) {
        return;
    }
    if (m_isZoneMode || m_isLoopMode) {
        resetZoneMode();
    }
    if (play) {
        if (m_id == Kdenlive::ClipMonitor && m_consumer->position() == m_producer->get_out() && speed > 0) {
            m_producer->seek(0);
        }
        m_producer->set_speed(speed);
        if (speed <= 1. || speed > 6.) {
            m_consumer->set("scrub_audio", 0);
        } else {
            m_consumer->set("scrub_audio", 1);
        }
        m_consumer->start();
        m_consumer->set("refresh", 1);
    } else {
        emit paused();
        m_producer->set_speed(0);
        m_producer->seek(m_consumer->position() + 1);
        m_consumer->purge();
        m_consumer->start();
    }
}

bool GLWidget::playZone(bool loop)
{
    if (!m_producer || m_proxy->zoneOut() <= m_proxy->zoneIn()) {
        pCore->displayMessage(i18n("Select a zone to play"), InformationMessage, 500);
        return false;
    }
    m_producer->seek(m_proxy->zoneIn());
    m_producer->set_speed(0);
    m_consumer->purge();
    m_producer->set("out", m_proxy->zoneOut());
    m_producer->set_speed(1.0);
    restartConsumer();
    m_consumer->set("scrub_audio", 0);
    m_consumer->set("refresh", 1);
    m_isZoneMode = true;
    m_isLoopMode = loop;
    return true;
}

bool GLWidget::restartConsumer()
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


bool GLWidget::loopClip(QPoint inOut)
{
    if (!m_producer || inOut.y() <= inOut.x()) {
        pCore->displayMessage(i18n("Select a clip to play"), InformationMessage, 500);
        return false;
    }
    m_loopIn = inOut.x();
    m_producer->seek(inOut.x());
    m_producer->set_speed(0);
    m_consumer->purge();
    m_producer->set("out", inOut.y());
    m_producer->set_speed(1.0);
    restartConsumer();
    m_consumer->set("scrub_audio", 0);
    m_consumer->set("refresh", 1);
    m_isZoneMode = false;
    m_isLoopMode = true;
    return true;
}

void GLWidget::resetZoneMode()
{
    if (!m_isZoneMode && !m_isLoopMode) {
        return;
    }
    m_producer->set("out", m_producer->get_length());
    m_loopIn = 0;
    m_isZoneMode = false;
    m_isLoopMode = false;
}

MonitorProxy *GLWidget::getControllerProxy()
{
    return m_proxy;
}

int GLWidget::getCurrentPos() const
{
    return m_proxy->getPosition();
}

void GLWidget::setRulerInfo(int duration, const std::shared_ptr<MarkerListModel> &model)
{
    rootObject()->setProperty("duration", duration);
    if (model != nullptr) {
        // we are resetting marker/snap model, reset zone
        rootContext()->setContextProperty("markersModel", model.get());
    }
}

void GLWidget::startConsumer()
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

void GLWidget::stop()
{
    m_refreshTimer.stop();
    // why this lock?
    QMutexLocker locker(&m_mltMutex);
    if (m_producer) {
        if (m_isZoneMode || m_isLoopMode) {
            resetZoneMode();
        }
        m_producer->set_speed(0.0);
    }
    if (m_consumer) {
        m_consumer->purge();
        if (!m_consumer->is_stopped()) {
            m_consumer->stop();
        }
    }
}

double GLWidget::playSpeed() const
{
    if (m_producer) {
        return m_producer->get_speed();
    }
    return 0.0;
}

void GLWidget::setDropFrames(bool drop)
{
    // why this lock?
    QMutexLocker locker(&m_mltMutex);
    if (m_consumer) {
        int dropFrames = 1;
        if (!drop) {
            dropFrames = -dropFrames;
        }
        m_consumer->stop();
        m_consumer->set("real_time", dropFrames);
        if (m_consumer->start() == -1) {
            qCWarning(KDENLIVE_LOG) << "ERROR, Cannot start monitor";
        }
    }
}

int GLWidget::volume() const
{
    if ((!m_consumer) || (!m_producer)) {
        return -1;
    }
    if (m_consumer->get("mlt_service") == QStringLiteral("multi")) {
        return ((int)100 * m_consumer->get_double("0.volume"));
    }
    return ((int)100 * m_consumer->get_double("volume"));
}

void GLWidget::setVolume(double volume)
{
    if (m_consumer) {
        if (m_consumer->get("mlt_service") == QStringLiteral("multi")) {
            m_consumer->set("0.volume", volume);
        } else {
            m_consumer->set("volume", volume);
        }
    }
}

int GLWidget::duration() const
{
    if (!m_producer) {
        return 0;
    }
    return m_producer->get_playtime();
}

void GLWidget::setConsumerProperty(const QString &name, const QString &value)
{
    QMutexLocker locker(&m_mltMutex);
    if (m_consumer) {
        m_consumer->set(name.toUtf8().constData(), value.toUtf8().constData());
        if (m_consumer->start() == -1) {
            qCWarning(KDENLIVE_LOG) << "ERROR, Cannot start monitor";
        }
    }
}

void GLWidget::updateScaling()
{
#if LIBMLT_VERSION_INT >= QT_VERSION_CHECK(6,20,0)
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
    int pWidth = previewHeight * pCore->getCurrentDar() / pCore->getCurrentSar();
    if (pWidth% 2 > 0) {
        pWidth ++;
    }
    m_profileSize = QSize(pWidth, previewHeight);
    if (m_consumer) {
        m_consumer->set("width", m_profileSize.width());
        m_consumer->set("height", m_profileSize.height());
        resizeGL(width(), height());
    }
#else
    int previewHeight = pCore->getCurrentFrameSize().height();
    int pWidth = previewHeight * pCore->getCurrentDar() / pCore->getCurrentSar();
    if (pWidth% 2 > 0) {
        pWidth ++;
    }
    m_profileSize = QSize(pWidth, previewHeight);
    if (m_consumer) {
        resizeGL(width(), height());
    }
#endif
}

void GLWidget::switchRuler(bool show)
{
    m_rulerHeight = show ? QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.5 : 0;
    m_displayRulerHeight = m_rulerHeight;
    resizeGL(width(), height());
    emit m_proxy->rulerHeightChanged();
}
