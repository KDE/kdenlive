/*
 * Copyright (c) 2011-2016 Meltytech, LLC
 * Original author: Dan Dennedy <dan@dennedy.org>
 * Modified for Kdenlive: Jean-Baptiste Mardelle
 *
 * GL shader based on BSD licensed code from Peter Bengtsson:
 * http://www.fourcc.org/source/YUV420P-OpenGL-GLSLang.c
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

#include <KMessageBox>
#include <QApplication>
#include <QOpenGLFunctions_3_2_Core>
#include <QPainter>
#include <QQmlContext>
#include <QQuickItem>
#include <klocalizedstring.h>

#include "core.h"
#include "glwidget.h"
#include "kdenlivesettings.h"
#include "mltcontroller/bincontroller.h"
#include "profiles/profilemodel.hpp"
#include "qml/qmlaudiothumb.h"
#include "timeline2/view/qml/timelineitems.h"
#include <mlt++/Mlt.h>

#ifndef GL_UNPACK_ROW_LENGTH
#ifdef GL_UNPACK_ROW_LENGTH_EXT
#define GL_UNPACK_ROW_LENGTH GL_UNPACK_ROW_LENGTH_EXT
#else
#error GL_UNPACK_ROW_LENGTH undefined
#endif
#endif

#ifdef QT_NO_DEBUG
#define check_error(fn)                                                                                                                                        \
    {                                                                                                                                                          \
    }
#else
#define check_error(fn)                                                                                                                                        \
    {                                                                                                                                                          \
        uint err = fn->glGetError();                                                                                                                            \
        if (err != GL_NO_ERROR) {                                                                                                                              \
            qCCritical(KDENLIVE_LOG) << "GL error" << hex << err << dec << "at" << __FILE__ << ":" << __LINE__;                                                \
        }                                                                                                                                                      \
    }
#endif

#ifndef GL_TIMEOUT_IGNORED
#define GL_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFFull
#endif

#ifndef Q_OS_WIN
using ClientWaitSync_fp = GLenum (*)(GLsync, GLbitfield, GLuint64);
static ClientWaitSync_fp ClientWaitSync = nullptr;
#endif

using namespace Mlt;

#define SEEK_INACTIVE (-1)

GLWidget::GLWidget(int id, QObject *parent)
    : QQuickView((QWindow *)parent)
    , sendFrameForAnalysis(false)
    , m_glslManager(nullptr)
    , m_consumer(nullptr)
    , m_producer(nullptr)
    , m_id(id)
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
    , m_openGLSync(false)
    , m_sendFrame(false)
    , m_isZoneMode(false)
    , m_isLoopMode(false)
    , m_offset(QPoint(0, 0))
    , m_shareContext(nullptr)
    , m_audioWaveDisplayed(false)
    , m_fbo(nullptr)
{
    m_texture[0] = m_texture[1] = m_texture[2] = 0;
    qRegisterMetaType<Mlt::Frame>("Mlt::Frame");
    qRegisterMetaType<SharedFrame>("SharedFrame");

    qmlRegisterType<QmlAudioThumb>("AudioThumb", 1, 0, "QmlAudioThumb");
    setPersistentOpenGLContext(true);
    setPersistentSceneGraph(true);
    setClearBeforeRendering(false);
    setResizeMode(QQuickView::SizeRootObjectToView);

    m_monitorProfile = new Mlt::Profile();
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(50);
    m_blackClip.reset(new Mlt::Producer(*m_monitorProfile, "color:black"));
    m_blackClip->set("kdenlive:id", "black");
    m_blackClip->set("out", 3);
    connect(&m_refreshTimer, &QTimer::timeout, this, &GLWidget::refresh);
    m_producer = &*m_blackClip;

    if (KdenliveSettings::gpu_accel()) {
        m_glslManager = new Mlt::Filter(*m_monitorProfile, "glsl.manager");
    }
    if (((m_glslManager != nullptr) && !m_glslManager->is_valid())) {
        delete m_glslManager;
        m_glslManager = nullptr;
        KdenliveSettings::setGpu_accel(false);
        // Need to destroy MLT global reference to prevent filters from trying to use GPU.
        mlt_properties_set_data(mlt_global_properties(), "glslManager", nullptr, 0, nullptr, nullptr);
        emit gpuNotSupported();
    }
    connect(this, &QQuickWindow::sceneGraphInitialized, this, &GLWidget::initializeGL, Qt::DirectConnection);
    connect(this, &QQuickWindow::beforeRendering, this, &GLWidget::paintGL, Qt::DirectConnection);
    registerTimelineItems();
    m_proxy = new MonitorProxy(this);
    connect(m_proxy, &MonitorProxy::seekRequestChanged, this, &GLWidget::requestSeek);
    rootContext()->setContextProperty("controller", m_proxy);
}

GLWidget::~GLWidget()
{
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
    delete m_monitorProfile;
}

void GLWidget::updateAudioForAnalysis()
{
    if (m_frameRenderer) {
        m_frameRenderer->sendAudioForAnalysis = KdenliveSettings::monitor_audio();
    }
}

void GLWidget::initializeGL()
{
    if (m_isInitialized || !isVisible() || (openglContext() == nullptr)) return;
    if (!m_offscreenSurface.isValid()) {
        m_offscreenSurface.setFormat(openglContext()->format());
        m_offscreenSurface.create();
        openglContext()->makeCurrent(this);
    }
    initializeOpenGLFunctions();
    qCDebug(KDENLIVE_LOG) << "OpenGL vendor: " << QString::fromUtf8((const char *)glGetString(GL_VENDOR));
    qCDebug(KDENLIVE_LOG) << "OpenGL renderer: " << QString::fromUtf8((const char *)glGetString(GL_RENDERER));
    qCDebug(KDENLIVE_LOG) << "OpenGL Threaded: " << openglContext()->supportsThreadedOpenGL();
    qCDebug(KDENLIVE_LOG) << "OpenGL ARG_SYNC: " << openglContext()->hasExtension("GL_ARB_sync");
    qCDebug(KDENLIVE_LOG) << "OpenGL OpenGLES: " << openglContext()->isOpenGLES();

    if ((m_glslManager != nullptr) && openglContext()->isOpenGLES()) {
        delete m_glslManager;
        m_glslManager = nullptr;
        KdenliveSettings::setGpu_accel(false);
        // Need to destroy MLT global reference to prevent filters from trying to use GPU.
        mlt_properties_set_data(mlt_global_properties(), "glslManager", nullptr, 0, nullptr, nullptr);
        emit gpuNotSupported();
    }
    createShader();

#if !defined(Q_OS_WIN)
    // getProcAddress is not working for me on Windows.
    if (KdenliveSettings::gpu_accel()) {
        m_openGLSync = false;
        if ((m_glslManager != nullptr) && openglContext()->hasExtension("GL_ARB_sync")) {
            ClientWaitSync = (ClientWaitSync_fp)openglContext()->getProcAddress("glClientWaitSync");
            if (ClientWaitSync) {
                m_openGLSync = true;
            } else {
                qCDebug(KDENLIVE_LOG) << "  / / // NO GL SYNC, ERROR";
                emit gpuNotSupported();
                delete m_glslManager;
                m_glslManager = nullptr;
            }
        }
    }
#endif

    openglContext()->doneCurrent();
    if (m_glslManager) {
        // Create a context sharing with this context for the RenderThread context.
        // This is needed because openglContext() is active in another thread
        // at the time that RenderThread is created.
        // See this Qt bug for more info: https://bugreports.qt.io/browse/QTBUG-44677
        m_shareContext = new QOpenGLContext;
        m_shareContext->setFormat(openglContext()->format());
        m_shareContext->setShareContext(openglContext());
        m_shareContext->create();
    }
    m_frameRenderer = new FrameRenderer(openglContext(), &m_offscreenSurface);
    m_frameRenderer->sendAudioForAnalysis = KdenliveSettings::monitor_audio();
    openglContext()->makeCurrent(this);
    // openglContext()->blockSignals(false);
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &GLWidget::frameDisplayed, Qt::QueuedConnection);
    connect(m_frameRenderer, &FrameRenderer::textureReady, this, &GLWidget::updateTexture, Qt::DirectConnection);
    connect(m_frameRenderer, &FrameRenderer::frameDisplayed, this, &GLWidget::onFrameDisplayed, Qt::QueuedConnection);

    connect(m_frameRenderer, &FrameRenderer::audioSamplesSignal, this, &GLWidget::audioSamplesSignal, Qt::QueuedConnection);
    connect(this, &GLWidget::textureUpdated, this, &GLWidget::update, Qt::QueuedConnection);
    m_initSem.release();
    m_isInitialized = true;
    reconfigure();
}

void GLWidget::resizeGL(int width, int height)
{
    int x, y, w, h;
    height -= m_proxy->rulerHeight();
    double this_aspect = (double)width / height;
    double video_aspect = m_monitorProfile->dar();

    // Special case optimisation to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if ((int)(this_aspect * 1000) == (int)(video_aspect * 1000)) {
        w = width;
        h = height;
    }
    // Use OpenGL to normalise sample aspect ratio
    else if (height * video_aspect > width) {
        w = width;
        h = width / video_aspect;
    } else {
        w = height * video_aspect;
        h = height;
    }
    x = (width - w) / 2;
    y = (height - h) / 2;
    m_rect.setRect(x, y, w, h);
    double scalex = (double)m_rect.width() / m_monitorProfile->width() * m_zoom;
    double scaley = (double)m_rect.width() / ((double)m_monitorProfile->height() * m_monitorProfile->dar() / m_monitorProfile->width()) /
                    m_monitorProfile->width() * m_zoom;
    QPoint center = m_rect.center();
    QQuickItem *rootQml = rootObject();
    if (rootQml) {
        rootQml->setProperty("center", center);
        rootQml->setProperty("scalex", scalex);
        rootQml->setProperty("scaley", scaley);
        if (rootQml->objectName() == QLatin1String("rootsplit")) {
            // Adjust splitter pos
            rootQml->setProperty("splitterPos", x + (rootQml->property("realpercent").toDouble() * w));
        }
    }
    emit rectChanged();
}

void GLWidget::resizeEvent(QResizeEvent *event)
{
    QQuickView::resizeEvent(event);
    resizeGL(event->size().width(), event->size().height());
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
    if (m_glslManager) {
        m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, "uniform sampler2D tex;"
                                                                   "varying highp vec2 coordinates;"
                                                                   "void main(void) {"
                                                                   "  gl_FragColor = texture2D(tex, coordinates);"
                                                                   "}");
        m_shader->link();
        m_textureLocation[0] = m_shader->uniformLocation("tex");
    } else {
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
    m_projectionLocation = m_shader->uniformLocation("projection");
    m_modelViewLocation = m_shader->uniformLocation("modelView");
    m_vertexLocation = m_shader->attributeLocation("vertex");
    m_texCoordLocation = m_shader->attributeLocation("texCoord");
}

static void uploadTextures(QOpenGLContext *context, const SharedFrame &frame, GLuint texture[])
{
    int width = frame.get_image_width();
    int height = frame.get_image_height();
    const uint8_t *image = frame.get_image();
    QOpenGLFunctions *f = context->functions();

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

void GLWidget::paintGL()
{
    QOpenGLFunctions *f = openglContext()->functions();
    int width = this->width() * devicePixelRatio();
    int height = this->height() * devicePixelRatio();

    f->glDisable(GL_BLEND);
    f->glDisable(GL_DEPTH_TEST);
    f->glDepthMask(GL_FALSE);
    f->glViewport(0, 0, width, height);
    check_error(f);
    QColor color(KdenliveSettings::window_background());
    f->glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    f->glClear(GL_COLOR_BUFFER_BIT);
    check_error(f);

    if (!((m_glslManager != nullptr) || openglContext()->supportsThreadedOpenGL())) {
        m_mutex.lock();
        if (!m_sharedFrame.is_valid()) {
            m_mutex.unlock();
            return;
        }
        uploadTextures(openglContext(), m_sharedFrame, m_texture);
        m_mutex.unlock();
    } else if (m_glslManager) {
        m_mutex.lock();
        if (m_sharedFrame.is_valid()) {
            m_texture[0] = *((const GLuint *)m_sharedFrame.get_image());
        }
    }

    if (!m_texture[0]) {
        if (m_glslManager) m_mutex.unlock();
        return;
    }

    // Bind textures.
    for (uint i = 0; i < 3; ++i) {
        if (m_texture[i] != 0u) {
            f->glActiveTexture(GL_TEXTURE0 + i);
            f->glBindTexture(GL_TEXTURE_2D, m_texture[i]);
            check_error(f);
        }
    }
    // Init shader program.
    m_shader->bind();
    if (m_glslManager) {
        m_shader->setUniformValue(m_textureLocation[0], 0);
    } else {
        m_shader->setUniformValue(m_textureLocation[0], 0);
        m_shader->setUniformValue(m_textureLocation[1], 1);
        m_shader->setUniformValue(m_textureLocation[2], 2);
        m_shader->setUniformValue(m_colorspaceLocation, m_monitorProfile->colorspace());
    }
    check_error(f);

    // Setup an orthographic projection.
    QMatrix4x4 projection;
    projection.scale(2.0f / (float)width, 2.0f / (float)height);
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
    vertices << QVector2D(float(-width) / 2.0f, float(-height) / 2.0f + m_proxy->rulerHeight());
    vertices << QVector2D(float(-width) / 2.0f, float(height) / 2.0f + m_proxy->rulerHeight());
    vertices << QVector2D(float(width) / 2.0f, float(-height) / 2.0f + m_proxy->rulerHeight());
    vertices << QVector2D(float(width) / 2.0f, float(height) / 2.0f + m_proxy->rulerHeight());
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
    f->glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
    check_error(f);

    if (m_sendFrame && m_analyseSem.tryAcquire(1)) {
        // Render RGB frame for analysis
        int fullWidth = m_monitorProfile->width();
        int fullHeight = m_monitorProfile->height();
        if ((m_fbo == nullptr) || m_fbo->size() != QSize(fullWidth, fullHeight)) {
            delete m_fbo;
            QOpenGLFramebufferObjectFormat fmt;
            fmt.setSamples(1);
            fmt.setInternalTextureFormat(GL_RGB);                             // GL_RGBA32F);  // which one is the fastest ?
            m_fbo = new QOpenGLFramebufferObject(fullWidth, fullHeight, fmt); // GL_TEXTURE_2D);
        }
        m_fbo->bind();
        f->glViewport(0, 0, fullWidth, fullHeight);

        QMatrix4x4 projection2;
        projection2.scale(2.0f / (float)width, 2.0f / (float)height);
        m_shader->setUniformValue(m_projectionLocation, projection2);

        f->glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
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
    f->glActiveTexture(GL_TEXTURE0);
    check_error(f);
    if (m_glslManager) {
        glFinish();
        check_error(f);
        m_mutex.unlock();
    }
}

void GLWidget::slotZoomScene(double value)
{
    int val = value;
    if (val >= 3) {
        setZoom(value - 2.0f);
    } else if (val == 2) {
        setZoom(0.5);
    } else if (val == 1) {
        setZoom(0.25);
    } else if (val == 0) {
        setZoom(0.125);
    }
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    if (((event->modifiers() & Qt::ControlModifier) != 0u) && ((event->modifiers() & Qt::ShiftModifier) != 0u)) {
        if (event->delta() > 0) {
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
        return;
    }
    emit mouseSeek(event->delta(), (uint)event->modifiers());
    event->accept();
}

void GLWidget::requestSeek()
{
    if (m_producer == nullptr) {
        return;
    }
    if (m_proxy->seekPosition() != SEEK_INACTIVE) {
        if (!qFuzzyIsNull(m_producer->get_speed())) {
            m_consumer->purge();
        }
        m_producer->seek(m_proxy->seekPosition());
        if (m_consumer->is_stopped()) {
            m_consumer->start();
        }
        m_consumer->set("refresh", 1);
    }
}

void GLWidget::seek(int pos)
{
    // Testing puspose only
    if (m_proxy->seekPosition() == SEEK_INACTIVE) {
        m_proxy->setSeekPosition(pos);
        if (!qFuzzyIsNull(m_producer->get_speed())) {
            m_consumer->purge();
        }
        m_producer->seek(pos);
        if (m_consumer->is_stopped()) {
            m_consumer->start();
        }
        m_consumer->set("refresh", 1);
    } else {
        m_proxy->setSeekPosition(pos);
    }
}

void GLWidget::requestRefresh()
{
    if ((m_producer != nullptr) && qFuzzyIsNull(m_producer->get_speed())) {
        m_refreshTimer.start();
    }
}

void GLWidget::refresh()
{
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mutex);
    if (m_consumer->is_stopped()) {
        m_consumer->start();
    }
    m_consumer->set("refresh", 1);
}

bool GLWidget::checkFrameNumber(int pos)
{
    emit seekPosition(pos);
    //TODO: cleanup and move logic to proper proxy class
    m_proxy->setPosition(pos);
    if (pos == m_proxy->seekPosition()) {
        m_proxy->setSeekPosition(SEEK_INACTIVE);
        return true;
    }
    const double speed = m_producer->get_speed();
    if (m_proxy->seekPosition() != SEEK_INACTIVE) {
        m_producer->set_speed(0);
        m_producer->seek(m_proxy->seekPosition());
        if (qFuzzyIsNull(speed)) {
            m_consumer->set("refresh", 1);
        } else {
            m_producer->set_speed(speed);
        }
    } else if (qFuzzyIsNull(speed)) {
        if (m_isLoopMode) {
            if (pos >= m_producer->get_int("out") - 1) {
                m_consumer->purge();
                m_producer->seek(m_proxy->zoneIn());
                m_producer->set_speed(1.0);
                m_consumer->set("refresh", 1);
            }
            return true;
        } else {
            return pos < m_producer->get_int("out") - 1.;
        }
    } else if (speed < 0. && pos <= 0) {
        m_producer->set_speed(0);
        return false;
    }
    return true;
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
    if (m_glslManager) {
        // clearFrameRenderer();
        m_glslManager->fire_event("init glsl");
        if (m_glslManager->get_int("glsl_supported") == 0) {
            delete m_glslManager;
            m_glslManager = nullptr;
            // Need to destroy MLT global reference to prevent filters from trying to use GPU.
            mlt_properties_set_data(mlt_global_properties(), "glslManager", nullptr, 0, nullptr, nullptr);
            emit gpuNotSupported();
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

void GLWidget::slotSwitchAudioOverlay(bool enable)
{
    KdenliveSettings::setDisplayAudioOverlay(enable);
    if (m_audioWaveDisplayed && !enable) {
        if ((m_producer != nullptr) && m_producer->get_int("video_index") != -1) {
            // We have a video producer, disable filter
            removeAudioOverlay();
        }
    }
    if (enable && !m_audioWaveDisplayed && m_producer != nullptr) {
        createAudioOverlay(m_producer->get_int("video_index") == -1);
    }
}

int GLWidget::setProducer(Mlt::Producer *producer, bool isActive, int position)
{
    int error = 0;
    QString currentId;
    int consumerPosition = 0;
    currentId = m_producer->parent().get("kdenlive:id");
    if (producer != nullptr) {
        m_producer = producer;
    } else {
        if (currentId == QLatin1String("black")) {
            return 0;
        }
        if (m_audioWaveDisplayed) {
            removeAudioOverlay();
        }
        m_producer = &*m_blackClip;
    }
    if (m_producer) {
        m_producer->set_speed(0);
        if (m_consumer) {
            consumerPosition = m_consumer->position();
            if (!m_consumer->is_stopped()) {
                m_consumer->stop();
            }
        }
        error = reconfigure();
        if (error == 0) {
            // The profile display aspect ratio may have changed.
            resizeGL(width(), height());
        }
    } else {
        return error;
    }
    if (!m_consumer) {
        return error;
    }
    consumerPosition = m_consumer->position();
    if (m_producer->get_int("video_index") == -1) {
        // This is an audio only clip, attach visualization filter. Currently, the filter crashes MLT when Movit accel is used
        if (!m_audioWaveDisplayed) {
            createAudioOverlay(true);
        } else if (m_consumer) {
            if (KdenliveSettings::gpu_accel()) {
                removeAudioOverlay();
            } else {
                adjustAudioOverlay(true);
            }
        }
    } else if (m_audioWaveDisplayed && (m_consumer != nullptr)) {
        // This is not an audio clip, hide wave
        if (KdenliveSettings::displayAudioOverlay()) {
            adjustAudioOverlay(m_producer->get_int("video_index") == -1);
        } else {
            removeAudioOverlay();
        }
    } else if (KdenliveSettings::displayAudioOverlay()) {
        createAudioOverlay(false);
    }
    if (position == -1 && m_producer->parent().get("kdenlive:id") == currentId) {
        position = consumerPosition;
    }
    if (position != -1) {
        m_producer->seek(position);
    }
    if (isActive) {
        startConsumer();
    }
    // emit durationChanged(m_producer->get_length() - 1, m_producer->get_in());
    m_proxy->setPosition(m_producer->position());
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

void GLWidget::createAudioOverlay(bool isAudio)
{
    if (!m_consumer) {
        return;
    }
    if (isAudio && KdenliveSettings::gpu_accel()) {
        // Audiowaveform filter crashes on Movit + audio clips)
        return;
    }
    Mlt::Filter f(*m_monitorProfile, "audiowaveform");
    if (f.is_valid()) {
        // f.set("show_channel", 1);
        f.set("color.1", "0xffff0099");
        f.set("fill", 1);
        if (isAudio) {
            // Fill screen
            f.set("rect", "0,0,100%,100%");
        } else {
            // Overlay on lower part of the screen
            f.set("rect", "0,80%,100%,20%");
        }
        m_consumer->attach(f);
        m_audioWaveDisplayed = true;
    }
}

void GLWidget::removeAudioOverlay()
{
    Mlt::Service sourceService(m_consumer->get_service());
    // move all effects to the correct producer
    int ct = 0;
    Mlt::Filter *filter = sourceService.filter(ct);
    while (filter != nullptr) {
        QString srv = filter->get("mlt_service");
        if (srv == QLatin1String("audiowaveform")) {
            sourceService.detach(*filter);
            delete filter;
            break;
        } else {
            ct++;
        }
        filter = sourceService.filter(ct);
    }
    m_audioWaveDisplayed = false;
}

void GLWidget::adjustAudioOverlay(bool isAudio)
{
    Mlt::Service sourceService(m_consumer->get_service());
    // move all effects to the correct producer
    int ct = 0;
    Mlt::Filter *filter = sourceService.filter(ct);
    while (filter != nullptr) {
        QString srv = filter->get("mlt_service");
        if (srv == QLatin1String("audiowaveform")) {
            if (isAudio) {
                filter->set("rect", "0,0,100%,100%");
            } else {
                filter->set("rect", "0,80%,100%,20%");
            }
            break;
        } else {
            ct++;
        }
        filter = sourceService.filter(ct);
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
    QString serviceName = property("mlt_service").toString();
    if ((m_consumer == nullptr) || !m_consumer->is_valid() || strcmp(m_consumer->get("mlt_service"), "multi") != 0) {
        if (m_consumer) {
            m_consumer->purge();
            m_consumer->stop();
            delete m_consumer;
        }
        m_consumer = new Mlt::FilteredConsumer(*profile, "multi");
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
        // buid sub consumers
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
        QStringList paramList = params.split(' ', QString::SkipEmptyParts);
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
        if (m_glslManager) {
            if (m_openGLSync) {
                m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_gl_frame_show);
            } else {
                m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_gl_nosync_frame_show);
            }
        } else {
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_frame_show);
        }
        m_consumer->connect(*m_producer);
        m_consumer->start();
        return 0;
    }
    return -1;
}

int GLWidget::reconfigure(Mlt::Profile *profile)
{
    int error = 0;
    // use SDL for audio, OpenGL for video
    QString serviceName = property("mlt_service").toString();
    if (profile) {
        reloadProfile();
        m_blackClip.reset(new Mlt::Producer(*profile, "color:black"));
        m_blackClip->set("kdenlive:id", "black");
    }
    if ((m_consumer == nullptr) || !m_consumer->is_valid() || strcmp(m_consumer->get("mlt_service"), "multi") == 0) {
        if (m_consumer) {
            m_consumer->purge();
            m_consumer->stop();
            delete m_consumer;
        }
        QString audioBackend = KdenliveSettings::audiobackend();
        if (serviceName.isEmpty() || serviceName != audioBackend) {
            m_consumer = new Mlt::FilteredConsumer(*m_monitorProfile, audioBackend.toLatin1().constData());
            if (m_consumer->is_valid()) {
                serviceName = audioBackend;
                setProperty("mlt_service", serviceName);
            } else {
                // Warning, audio backend unavailable on system
                delete m_consumer;
                m_consumer = nullptr;
                QStringList backends = {"sdl2_audio", "sdl_audio", "rtaudio"};
                for (const QString &bk : backends) {
                    if (bk == audioBackend) {
                        // Already tested
                        continue;
                    }
                    m_consumer = new Mlt::FilteredConsumer(*m_monitorProfile, bk.toLatin1().constData());
                    if (m_consumer->is_valid()) {
                        if (audioBackend == KdenliveSettings::sdlAudioBackend()) {
                            // switch sdl audio backend
                            KdenliveSettings::setSdlAudioBackend(bk);
                        }
                        qDebug()<<"++++++++\nSwitching audio backend to: "<<bk<<"\n++++++++++";
                        KdenliveSettings::setAudiobackend(bk);
                        serviceName = bk;
                        setProperty("mlt_service", serviceName);
                        break;
                    } else {
                        delete m_consumer;
                        m_consumer = nullptr;
                    }
                }
                if (!m_consumer) {
                    qWarning() << "WARNING, NO AUDIO BACKEND FOUND";
                    return -1;
                }
            }
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
            m_consumer->connect(*m_producer);
            //m_producer->set_speed(0.0);
        }
        int dropFrames = realTime();
        if (!KdenliveSettings::monitor_dropframes()) {
            dropFrames = -dropFrames;
        }
        m_consumer->set("real_time", dropFrames);
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
            m_consumer->set("mlt_image_format", "yuv422");
        }

        delete m_displayEvent;
        if (m_glslManager) {
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_gl_frame_show);
        } else {
            m_displayEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)on_frame_show);
        }

        int volume = KdenliveSettings::volume();
        if (serviceName.startsWith(QLatin1String("sdl_audio"))) {
            QString audioDevice = KdenliveSettings::audiodevicename();
            if (!audioDevice.isEmpty()) {
                m_consumer->set("audio_device", audioDevice.toUtf8().constData());
            }

            QString audioDriver = KdenliveSettings::audiodrivername();
            if (!audioDriver.isEmpty()) {
                m_consumer->set("audio_driver", audioDriver.toUtf8().constData());
            }
        }
        /*if (!m_monitorProfile->progressive())
            m_consumer->set("progressive", property("progressive").toBool());*/
        m_consumer->set("volume", volume / 100.0);
        // m_consumer->set("progressive", 1);
        m_consumer->set("rescale", KdenliveSettings::mltinterpolation().toUtf8().constData());
        m_consumer->set("deinterlace_method", KdenliveSettings::mltdeinterlacer().toUtf8().constData());
#ifdef Q_OS_WIN
            m_consumer->set("audio_buffer", 2048);
#else
            m_consumer->set("audio_buffer", 512);
#endif
        m_consumer->set("buffer", 25);
        m_consumer->set("prefill", 1);
        m_consumer->set("scrub_audio", 1);
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

float GLWidget::scale() const
{
    return (double)m_rect.width() / m_monitorProfile->width() * m_zoom;
}

Mlt::Profile *GLWidget::profile()
{
    return m_monitorProfile;
}

void GLWidget::reloadProfile()
{
    auto &profile = pCore->getCurrentProfile();
    m_monitorProfile->get_profile()->description = qstrdup(profile->description().toUtf8().constData());
    m_monitorProfile->set_colorspace(profile->colorspace());
    m_monitorProfile->set_frame_rate(profile->frame_rate_num(), profile->frame_rate_den());
    m_monitorProfile->set_height(profile->height());
    m_monitorProfile->set_width(profile->width());
    m_monitorProfile->set_progressive(static_cast<int>(profile->progressive()));
    m_monitorProfile->set_sample_aspect(profile->sample_aspect_num(), profile->sample_aspect_den());
    m_monitorProfile->set_display_aspect(profile->display_aspect_num(), profile->display_aspect_den());
    m_monitorProfile->set_explicit(1);
    // The profile display aspect ratio may have changed.
    resizeGL(width(), height());
    refreshSceneLayout();
}

QSize GLWidget::profileSize() const
{
    return QSize(m_monitorProfile->width(), m_monitorProfile->height());
}

QRect GLWidget::displayRect() const
{
    return m_rect;
}

QPoint GLWidget::offset() const
{
    return QPoint(m_offset.x() - ((int)((float)m_monitorProfile->width() * m_zoom) - width()) / 2,
                            m_offset.y() - ((int)((float)m_monitorProfile->height() * m_zoom) - height()) / 2);
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
    m_mutex.lock();
    m_sharedFrame = frame;
    m_sendFrame = sendFrameForAnalysis;
    m_mutex.unlock();
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

int GLWidget::realTime() const
{
    if (m_glslManager) {
        return 1;
    }
    return KdenliveSettings::mltthreads();
}

Mlt::Consumer *GLWidget::consumer()
{
    return m_consumer;
}

void GLWidget::updateGamma()
{
    reconfigure();
}

const QString GLWidget::sceneList(const QString &root, const QString &fullPath)
{
    QString playlist;
    qCDebug(KDENLIVE_LOG) << " * * *Setting document xml root: " << root;
    Mlt::Consumer xmlConsumer(*m_monitorProfile, fullPath.isEmpty() ? "xml:kdenlive_playlist" : fullPath.toUtf8().constData());
    if (!root.isEmpty()) {
        xmlConsumer.set("root", root.toUtf8().constData());
    }
    if (!xmlConsumer.is_valid()) {
        return QString();
    }
    m_producer->optimise();
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("store", "kdenlive");
    // Disabling meta creates cleaner files, but then we don't have access to metadata on the fly (meta channels, etc)
    // And we must use "avformat" instead of "avformat-novalidate" on project loading which causes a big delay on project opening
    // xmlConsumer.set("no_meta", 1);
    Mlt::Producer prod(m_producer->get_producer());
    if (!prod.is_valid()) {
        return QString();
    }
    xmlConsumer.connect(prod);
    xmlConsumer.run();
    playlist = fullPath.isEmpty() ? QString::fromUtf8(xmlConsumer.get("kdenlive_playlist")) : fullPath;
    return playlist;
}

void GLWidget::updateTexture(GLuint yName, GLuint uName, GLuint vName)
{
    m_texture[0] = yName;
    m_texture[1] = uName;
    m_texture[2] = vName;
    m_sendFrame = sendFrameForAnalysis;
    emit textureUpdated();
    // update();
}

// MLT consumer-frame-show event handler
void GLWidget::on_frame_show(mlt_consumer, void *self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    if (frame.get_int("rendered") != 0) {
        GLWidget *widget = static_cast<GLWidget *>(self);
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
        GLWidget *widget = static_cast<GLWidget *>(self);
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
        GLWidget *widget = static_cast<GLWidget *>(self);
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
    // delete m_context;
}

void RenderThread::run()
{
    if (m_context) {
        m_context->makeCurrent(m_surface);
    }
    m_function(m_data);
    if (m_context) {
        m_context->doneCurrent();
        delete m_context;
    }
}

FrameRenderer::FrameRenderer(QOpenGLContext *shareContext, QSurface *surface)
    : QThread(nullptr)
    , m_semaphore(3)
    , m_context(nullptr)
    , m_surface(surface)
    , m_gl32(nullptr)
    , sendAudioForAnalysis(false)
{
    Q_ASSERT(shareContext);
    m_renderTexture[0] = m_renderTexture[1] = m_renderTexture[2] = 0;
    m_displayTexture[0] = m_displayTexture[1] = m_displayTexture[2] = 0;
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
    int width = 0;
    int height = 0;
    mlt_image_format format = mlt_image_yuv420p;
    frame.get_image(format, width, height);
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
        int width = 0;
        int height = 0;

        frame.set("movit.convert.use_texture", 1);
        mlt_image_format format = mlt_image_glsl_texture;
        frame.get_image(format, width, height);
        m_context->makeCurrent(m_surface);
        GLsync sync = (GLsync)frame.get_data("movit.convert.fence");
        if (sync) {
#ifdef Q_OS_WIN
            // On Windows, use QOpenGLFunctions_3_2_Core instead of getProcAddress.
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
            if (ClientWaitSync) {
                ClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
                check_error(m_context->functions());
            }
#endif // Q_OS_WIN
        }

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
        int width = 0;
        int height = 0;

        frame.set("movit.convert.use_texture", 1);
        mlt_image_format format = mlt_image_glsl_texture;
        frame.get_image(format, width, height);
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

void GLWidget::setAudioThumb(int channels, const QVariantList &audioCache)
{
    if (rootObject()) {
        QmlAudioThumb *audioThumbDisplay = rootObject()->findChild<QmlAudioThumb *>(QStringLiteral("audiothumb"));
        if (audioThumbDisplay) {
            QImage img(width(), height() / 6, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            if (!audioCache.isEmpty() && channels > 0) {
                int audioLevelCount = audioCache.count() - 1;
                // simplified audio
                QPainter painter(&img);
                QRectF mappedRect(0, 0, img.width(), img.height());
                int channelHeight = mappedRect.height();
                double value;
                double scale = (double)width() / (audioLevelCount / channels);
                if (scale < 1) {
                    painter.setPen(QColor(80, 80, 150, 200));
                    for (int i = 0; i < img.width(); i++) {
                        int framePos = i / scale;
                        value = audioCache.at(qMin(framePos * channels, audioLevelCount)).toDouble() / 256;
                        for (int channel = 1; channel < channels; channel++) {
                            value = qMax(value, audioCache.at(qMin(framePos * channels + channel, audioLevelCount)).toDouble() / 256);
                        }
                        painter.drawLine(i, mappedRect.bottom() - (value * channelHeight), i, mappedRect.bottom());
                    }
                } else {
                    QPainterPath positiveChannelPath;
                    positiveChannelPath.moveTo(0, mappedRect.bottom());
                    for (int i = 0; i < audioLevelCount / channels; i++) {
                        value = audioCache.at(qMin(i * channels, audioLevelCount)).toDouble() / 256;
                        for (int channel = 1; channel < channels; channel++) {
                            value = qMax(value, audioCache.at(qMin(i * channels + channel, audioLevelCount)).toDouble() / 256);
                        }
                        positiveChannelPath.lineTo(i * scale, mappedRect.bottom() - (value * channelHeight));
                    }
                    positiveChannelPath.lineTo(mappedRect.right(), mappedRect.bottom());
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QBrush(QColor(80, 80, 150, 200)));
                    painter.drawPath(positiveChannelPath);
                }
                painter.end();
            }
            audioThumbDisplay->setImage(img);
        }
    }
}

void GLWidget::refreshSceneLayout()
{
    if (!rootObject()) {
        return;
    }
    rootObject()->setProperty("profile", QPoint(m_monitorProfile->width(), m_monitorProfile->height()));
    rootObject()->setProperty("scalex", (double)m_rect.width() / m_monitorProfile->width() * m_zoom);
    rootObject()->setProperty("scaley",
                              (double)m_rect.width() / (((double)m_monitorProfile->height() * m_monitorProfile->dar() / m_monitorProfile->width())) /
                                  m_monitorProfile->width() * m_zoom);
}

void GLWidget::switchPlay(bool play, double speed)
{
    // QMutexLocker locker(&m_mutex);
    m_proxy->setSeekPosition(SEEK_INACTIVE);
    if ((m_producer == nullptr) || (m_consumer == nullptr)) {
        return;
    }
    if (m_isZoneMode) {
        resetZoneMode();
    }
    if (play) {
        double currentSpeed = m_producer->get_speed();
        /*if (m_name == Kdenlive::ClipMonitor && m_consumer->position() == m_producer->get_out()) {
            m_producer->seek(0);
        }*/
        if (m_consumer->get_int("real_time") != realTime()) {
            m_consumer->set("real_time", realTime());
            m_consumer->set("buffer", 25);
            m_consumer->set("prefill", 1);
            // Changes to real_time require a consumer restart if running.
            if (!m_consumer->is_stopped()) {
                m_consumer->stop();
            }
        }
        if (qFuzzyIsNull(currentSpeed)) {
            m_consumer->start();
            m_consumer->set("refresh", 1);
        } else {
            m_consumer->purge();
        }
        m_producer->set_speed(speed);
    } else {
        m_consumer->set("real_time", -1);
        m_consumer->set("buffer", 0);
        m_consumer->set("prefill", 0);
        m_producer->set_speed(0.0);
        m_producer->seek(m_consumer->position() + 1);
        m_consumer->purge();
    }
}

bool GLWidget::playZone(bool loop)
{
    if (!m_producer) {
        return false;
    }
    m_proxy->setSeekPosition(SEEK_INACTIVE);
    m_producer->seek(m_proxy->zoneIn());
    m_producer->set_speed(0);
    m_consumer->purge();
    m_producer->set("out", m_proxy->zoneOut());
    m_producer->set_speed(1.0);
    if (m_consumer->is_stopped()) {
        m_consumer->start();
    }
    m_consumer->set("refresh", 1);
    m_isZoneMode = true;
    m_isLoopMode = loop;
    return true;
}

bool GLWidget::loopClip()
{
    if (!m_producer) {
        return false;
    }
    m_proxy->setSeekPosition(SEEK_INACTIVE);
    m_producer->seek(0);
    m_producer->set_speed(0);
    m_consumer->purge();
    m_producer->set("out", m_producer->get_playtime());
    m_producer->set_speed(1.0);
    if (m_consumer->is_stopped()) {
        m_consumer->start();
    }
    m_consumer->set("refresh", 1);
    m_isZoneMode = true;
    m_isLoopMode = true;
    return true;
}

void GLWidget::resetZoneMode()
{
    if (!m_isZoneMode && !m_isLoopMode) {
        return;
    }
    m_producer->set("out", m_producer->get_length());
    m_isZoneMode = false;
    m_isLoopMode = false;
}

MonitorProxy *GLWidget::getControllerProxy()
{
    return m_proxy;
}

int GLWidget::getCurrentPos() const
{
    return m_proxy->seekPosition() == SEEK_INACTIVE ? m_consumer->position() : m_proxy->seekPosition();
}

void GLWidget::setRulerInfo(int duration, std::shared_ptr<MarkerListModel> model)
{
    rootObject()->setProperty("duration", duration);
    if (model != nullptr) {
        // we are resetting marker/snap model, reset zone
        m_proxy->resetZone();
        rootContext()->setContextProperty("markersModel", model.get());
    }
}

void GLWidget::startConsumer()
{
    if (m_consumer == nullptr) {
        return;
    }
    if (m_consumer->is_stopped() && m_consumer->start() == -1) {
        // ARGH CONSUMER BROKEN!!!!
        KMessageBox::error(
            qApp->activeWindow(),
            i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
        if (m_displayEvent) {
            delete m_displayEvent;
        }
        m_displayEvent = nullptr;
        delete m_consumer;
        m_consumer = nullptr;
        return;
    }
    m_consumer->set("refresh", 1);
}

void GLWidget::stop()
{
    m_refreshTimer.stop();
    m_proxy->setSeekPosition(SEEK_INACTIVE);
    QMutexLocker locker(&m_mutex);
    if (m_producer) {
        if (m_isZoneMode) {
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
    QMutexLocker locker(&m_mutex);
    if (m_consumer) {
        int dropFrames = realTime();
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
    if ((m_consumer == nullptr) || (m_producer == nullptr)) {
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
    if (m_producer == nullptr) {
        return 0;
    }
    return m_producer->get_playtime();
}
