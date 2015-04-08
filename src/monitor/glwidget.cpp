/*
 * Copyright (c) 2011-2014 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
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

#include <QtWidgets>
#include <QOpenGLFunctions_3_2_Core>
#include <QUrl>
#include <QOffscreenSurface>
#include <QtQml>
#include <QQuickItem>

#include <mlt++/Mlt.h>
#include "glwidget.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mltcontroller/bincontroller.h"
//#include "qmltypes/qmlutilities.h"
//#include "qmltypes/qmlfilter.h"
//#include "mainwindow.h"

#define USE_GL_SYNC // Use glFinish() if not defined.

#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { qCritical() << "GL error"  << hex << err << dec << "at" << __FILE__ << ":" << __LINE__; } }

#ifndef GL_TIMEOUT_IGNORED
#define GL_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFFull
#endif

#ifndef Q_OS_WIN
typedef GLenum (*ClientWaitSync_fp) (GLsync sync, GLbitfield flags, GLuint64 timeout);
static ClientWaitSync_fp ClientWaitSync = 0;
#endif

using namespace Mlt;

GLWidget::GLWidget(bool accel, QObject *parent)
    : QQuickView((QWindow*) parent)
    , m_shader(0)
    , m_glslManager(0)
    , m_initSem(0)
    , m_isInitialized(false)
    , m_threadStartEvent(0)
    , m_threadStopEvent(0)
    , m_threadCreateEvent(0)
    , m_threadJoinEvent(0)
    , m_frameRenderer(0)
    , m_zoom(1.0f)
    , m_offset(QPoint(0, 0))
    , m_consumer(0)
    , m_producer(0)
    , m_audioWaveDisplayed(false)
    , m_fbo(NULL)
    , sendFrameForAnalysis(false)
{
    qDebug() << "begin";
    m_texture[0] = m_texture[1] = m_texture[2] = 0;
    qRegisterMetaType<Mlt::Frame>("Mlt::Frame");
    qRegisterMetaType<SharedFrame>("SharedFrame");

    setPersistentOpenGLContext(true);
    setPersistentSceneGraph(true);
    setClearBeforeRendering(false);
    setResizeMode(QQuickView::SizeRootObjectToView);
    //rootContext->setContextProperty("settings", &ShotcutSettings::singleton());
    /*rootContext()->setContextProperty("application", &QmlApplication::singleton());
    rootContext()->setContextProperty("profile", &QmlProfile::singleton());
    rootContext()->setContextProperty("view", new QmlView(this));*/
    
/*    QDir importPath = QmlUtilities::qmlDir();
    importPath.cd("modules");
    engine()->addImportPath(importPath.path());
    QmlUtilities::setCommonProperties((QQuickView*)this);*/

    if (KdenliveSettings::gpu_accel())
        m_glslManager = new Mlt::Filter(*pCore->binController()->profile(), "glsl.manager");
    if ((m_glslManager && !m_glslManager->is_valid())) {
        delete m_glslManager;
        m_glslManager = 0;
    }

    connect(this, SIGNAL(sceneGraphInitialized()), SLOT(initializeGL()), Qt::DirectConnection);
    connect(this, SIGNAL(sceneGraphInitialized()), SLOT(setBlankScene()), Qt::QueuedConnection);
    connect(this, SIGNAL(beforeRendering()), SLOT(paintGL()), Qt::DirectConnection);
    qDebug() << "end";
}

GLWidget::~GLWidget()
{
    //stop();
    delete m_glslManager;
    delete m_threadStartEvent;
    delete m_threadStopEvent;
    delete m_threadCreateEvent;
    delete m_threadJoinEvent;
    if (m_frameRenderer && m_frameRenderer->isRunning()) {
        QMetaObject::invokeMethod(m_frameRenderer, "cleanup");
        m_frameRenderer->quit();
        m_frameRenderer->wait();
        m_frameRenderer->deleteLater();
    }
    delete m_shader;
}

void GLWidget::initializeGL()
{
    qDebug() << "begin";
    if (m_isInitialized) return;

    initializeOpenGLFunctions();
    openglContext()->blockSignals(true);
    /*openglContext()->doneCurrent();
    openglContext()->setShareContext(pCore->glShareWidget()->context());
    openglContext()->setFormat(requestedFormat());
    openglContext()->create();
    openglContext()->makeCurrent(this);*/
    createShader();

#if defined(USE_GL_SYNC) && !defined(Q_OS_WIN)
    // getProcAddress is not working for me on Windows.
    if (KdenliveSettings::gpu_accel()) {
        if (m_glslManager && openglContext()->hasExtension("GL_ARB_sync")) {
            ClientWaitSync = (ClientWaitSync_fp) openglContext()->getProcAddress("glClientWaitSync");
        }
        if (!ClientWaitSync) {
            emit gpuNotSupported();
            delete m_glslManager;
            m_glslManager = 0;
        }
    }
#endif

    openglContext()->doneCurrent();
    m_frameRenderer = new FrameRenderer(openglContext());
    openglContext()->makeCurrent(this);//openglContext()->surface());
    openglContext()->blockSignals(false);
    connect(m_frameRenderer, SIGNAL(frameDisplayed(const SharedFrame&)), this, SIGNAL(frameDisplayed(const SharedFrame&)), Qt::QueuedConnection);
    connect(m_frameRenderer, SIGNAL(analyseFrame(QImage)), this, SIGNAL(analyseFrame(QImage)), Qt::QueuedConnection);
    connect(m_frameRenderer, SIGNAL(textureReady(GLuint,GLuint,GLuint)), SLOT(updateTexture(GLuint,GLuint,GLuint)), Qt::DirectConnection);
    connect(this, SIGNAL(textureUpdated()), SLOT(update()), Qt::QueuedConnection);

    m_initSem.release();
    m_isInitialized = true;
    
    //QObject *frameRect = rootObject()->findChild<QObject*>("framerect");
    

    qDebug() << "end";
}

void GLWidget::effectRectChanged()
{
    QObject *item = rootObject()->findChild<QObject*>("framerect");
    QObject *base = rootObject()->findChild<QObject*>("frame");
    double scale = rootObject()->property("scale").toDouble();
    int x = (int) ((item->property("x").toInt() - base->property("x").toInt()) / scale + 0.5);
    int y = (int) ((item->property("y").toInt() - base->property("y").toInt()) / scale + 0.5);
    int w = (int) (item->property("width").toInt() / scale + 0.5);
    int h = (int) (item->property("height").toInt() / scale + 0.5);
    QRect r(x, y, w, h);
    emit effectChanged(r);
}

void GLWidget::setBlankScene()
{
    //setSource(QmlUtilities::blankVui());
}

void GLWidget::resizeGL(int width, int height)
{
    int x, y, w, h;
    double this_aspect = (double) width / height;
    double video_aspect = pCore->binController()->profile()->dar();

    // Special case optimisation to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if ((int) (this_aspect * 1000) == (int) (video_aspect * 1000))
    {
        w = width;
        h = height;
    }
    // Use OpenGL to normalise sample aspect ratio
    else if (height * video_aspect > width)
    {
        w = width;
        h = width / video_aspect;
    }
    else
    {
        w = height * video_aspect;
        h = height;
    }
    x = (width - w) / 2;
    y = (height - h) / 2;
    m_rect.setRect(x, y, w, h);
    double scale = (double) m_rect.width() / m_consumer->profile()->width() * m_zoom;
    QPoint center = m_rect.center();
    rootObject()->setProperty("center", center);
    rootObject()->setProperty("scale", scale);
    emit rectChanged();
}

void GLWidget::resizeEvent(QResizeEvent* event)
{
    QQuickView::resizeEvent(event);
    resizeGL(event->size().width(), event->size().height());
}

void GLWidget::createShader()
{
    m_shader = new QOpenGLShaderProgram;
    m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                     "uniform mat4 projection;"
                                      "uniform mat4 modelView;"
                                     "attribute vec4 vertex;"
                                     "attribute vec2 texCoord;"
                                     "varying vec2 coordinates;"
                                     "void main(void) {"
                                     "  gl_Position = projection * modelView * vertex;"
                                     "  coordinates = texCoord;"
                                     "}");
    if (m_glslManager) {
        m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                          "uniform sampler2D tex;"
                                          "varying vec2 coordinates;"
                                          "void main(void) {"
                                          "  gl_FragColor = texture2D(tex, coordinates);"
                                          "}");
        m_shader->link();
        m_textureLocation[0] = m_shader->uniformLocation("tex");
    } else {
        m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                          "uniform sampler2D Ytex, Utex, Vtex;"
                                          "uniform int colorspace;"
                                          "varying vec2 coordinates;"
                                          "void main(void) {"
                                          "  vec3 texel;"
                                          "  texel.r = texture2D(Ytex, coordinates).r - 0.0625;" // Y
                                          "  texel.g = texture2D(Utex, coordinates).r - 0.5;"    // U
                                          "  texel.b = texture2D(Vtex, coordinates).r - 0.5;"    // V
                                          "  mat3 coefficients;"
                                          "  if (colorspace == 601) {"
                                          "    coefficients = mat3("
                                          "      1.1643,  1.1643,  1.1643," // column 1
                                          "      0.0,    -0.39173, 2.017," // column 2
                                          "      1.5958, -0.8129,  0.0);" // column 3
                                          "  } else {" // ITU-R 709
                                          "    coefficients = mat3("
                                          "      1.1643, 1.1643, 1.1643," // column 1
                                          "      0.0,   -0.213,  2.112," // column 2
                                          "      1.793, -0.533,  0.0);" // column 3
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

void GLWidget::paintGL()
{
    int width = this->width() * devicePixelRatio();
    int height = this->height() * devicePixelRatio();

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glViewport(0, 0, width, height);
    check_error();
    QColor color(KdenliveSettings::window_background()); //= QPalette().color(QPalette::Window);
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);
    check_error();

    if (!m_texture[0]) return;

    // Bind textures.
    for (int i = 0; i < 3; ++i) {
        if (m_texture[i]) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, m_texture[i]);
            check_error();
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
        m_shader->setUniformValue(m_colorspaceLocation, pCore->binController()->profile()->colorspace());
    }
    check_error();

    // Setup an orthographic projection.
    QMatrix4x4 projection;
    projection.scale(2.0f / width, 2.0f / height);
    m_shader->setUniformValue(m_projectionLocation, projection);
    check_error();

    // Set model view.
    QMatrix4x4 modelView;
    if (m_zoom != 1.0) {
        if (offset().x() || offset().y())
            modelView.translate(-offset().x() * devicePixelRatio(),
                                 offset().y() * devicePixelRatio());
        modelView.scale(zoom(), zoom());
    }
    m_shader->setUniformValue(m_modelViewLocation, modelView);
    check_error();

    // Provide vertices of triangle strip.
    QVector<QVector2D> vertices;
    width = m_rect.width() * devicePixelRatio();
    height = m_rect.height() * devicePixelRatio();
    vertices << QVector2D(float(-width)/2.0f, float(-height)/2.0f);
    vertices << QVector2D(float(-width)/2.0f, float( height)/2.0f);
    vertices << QVector2D(float( width)/2.0f, float(-height)/2.0f);
    vertices << QVector2D(float( width)/2.0f, float( height)/2.0f);
    m_shader->enableAttributeArray(m_vertexLocation);
    check_error();
    m_shader->setAttributeArray(m_vertexLocation, vertices.constData());
    check_error();

    // Provide texture coordinates.
    QVector<QVector2D> texCoord;
    texCoord << QVector2D(0.0f, 1.0f);
    texCoord << QVector2D(0.0f, 0.0f);
    texCoord << QVector2D(1.0f, 1.0f);
    texCoord << QVector2D(1.0f, 0.0f);
    m_shader->enableAttributeArray(m_texCoordLocation);
    check_error();
    m_shader->setAttributeArray(m_texCoordLocation, texCoord.constData());
    check_error();

    // Render
    glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
    check_error();

    // Render RGB frame for analysis
    if (sendFrameForAnalysis) {
        if (!m_fbo || m_fbo->size() != QSize(width, height)) {
            delete m_fbo;
            QOpenGLFramebufferObjectFormat f;
            f.setSamples(0);
            f.setInternalTextureFormat(GL_RGB); //GL_RGBA32F);  // which one is the fastest ?
            m_fbo = new QOpenGLFramebufferObject(width, height, f); //GL_TEXTURE_2D);
        }
        m_fbo->bind();
        glViewport(0, 0, width, height);
        projection.scale((double) this->width() / width, (double) this->height() / height);
        m_shader->setUniformValue(m_projectionLocation, projection);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
        check_error();
        m_fbo->release();
        emit analyseFrame(m_fbo->toImage());
    }
    // Cleanup
    m_shader->disableAttributeArray(m_vertexLocation);
    m_shader->disableAttributeArray(m_texCoordLocation);
    m_shader->release();
    for (int i = 0; i < 3; ++i) {
        if (m_texture[i]) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            check_error();
        }
    }
    glActiveTexture(GL_TEXTURE0);
    check_error();
}

void GLWidget::wheelEvent(QWheelEvent * event)
{
    if (event->modifiers() & Qt::ControlModifier && event->modifiers() & Qt::ShiftModifier) {
        if (event->delta() > 0) {
            if (m_zoom == 1.0f) {
                setZoom(2.0f);
            }
            else if (m_zoom == 2.0f) {
                setZoom(3.0f);
            }
            else if (m_zoom < 1.0f) {
                setZoom(m_zoom * 2);
            }
        }
        else {
            if (m_zoom == 3.0f) {
                setZoom(2.0f);
            }
            else if (m_zoom == 2.0f) {
                setZoom(1.0f);
            }
            else {
                setZoom(m_zoom / 2);
            }
        }
        return;
    }
    emit mouseSeek(event->delta(), event->modifiers() & Qt::ControlModifier);
    event->accept();
}


void GLWidget::mousePressEvent(QMouseEvent* event)
{
    QQuickView::mousePressEvent(event);
    if (event->isAccepted() || effectSceneVisible()) return;
    if (event->button() == Qt::LeftButton)
        m_dragStart = event->pos();
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    QQuickView::mouseMoveEvent(event);
    if (event->isAccepted() || effectSceneVisible()) return;
/*    if (event->modifiers() == Qt::ShiftModifier && m_producer) {
        emit seekTo(m_producer->get_length() *  event->x() / width());
        return;
    }*/
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if (m_dragStart == QPoint() ||  (event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance())
        return;
    emit startDrag();
}

void GLWidget::keyPressEvent(QKeyEvent* event)
{
    event->ignore();
    return;
    QQuickView::keyPressEvent(event);
    if (event->isAccepted()) return;
    //MAIN.keyPressEvent(event);
}

void GLWidget::createThread(RenderThread **thread, thread_function_t function, void *data)
{
#ifdef Q_OS_WIN
    // On Windows, MLT event consumer-thread-create is fired from the Qt main thread.
    while (!m_isInitialized)
        qApp->processEvents();
#else
    if (!m_isInitialized) {
        m_initSem.acquire();
    }
#endif
    (*thread) = new RenderThread(function, data, m_glslManager? openglContext() : 0);
    (*thread)->start();
}

static void onThreadCreate(mlt_properties owner, GLWidget* self,
    RenderThread** thread, int* priority, thread_function_t function, void* data )
{
    Q_UNUSED(owner)
    Q_UNUSED(priority)
    self->createThread(thread, function, data);
}

static void onThreadJoin(mlt_properties owner, GLWidget* self, RenderThread* thread)
{
    Q_UNUSED(owner)
    Q_UNUSED(self)
    if (thread) {
        thread->quit();
        thread->wait();
        delete thread;
    }
}

void GLWidget::startGlsl()
{
    if (m_glslManager) {
        qDebug()<<"++++++++++++++++++++++  STARTING GLSL +++++++++++++++++++++++++++++";
        m_glslManager->fire_event("init glsl");
        if (!m_glslManager->get_int("glsl_supported")) {
            delete m_glslManager;
            m_glslManager = 0;
            // Need to destroy MLT global reference to prevent filters from trying to use GPU.
            mlt_properties_set_data(mlt_global_properties(), "glslManager", NULL, 0, NULL, NULL);
            emit gpuNotSupported();
        }
        else {
            emit started();
        }
    }
}

static void onThreadStarted(mlt_properties owner, GLWidget* self)
{
    Q_UNUSED(owner)
    self->startGlsl();
}

void GLWidget::stopGlsl()
{
    qDebug()<<"++++++++++++++++++++++  STOPPING GLSL +++++++++++++++++++++++++++++";
    m_glslManager->fire_event("close glsl");
    m_texture[0] = 0;
}

static void onThreadStopped(mlt_properties owner, GLWidget* self)
{
    Q_UNUSED(owner)
    self->stopGlsl();
}

void GLWidget::slotSwitchAudioOverlay(bool enable)
{
    KdenliveSettings::setDisplayAudioOverlay(enable);
    if (m_audioWaveDisplayed && enable == false) {
        if (m_producer && m_producer->get_int("video_index") != -1) {
            // We have a video producer, disable filter
            removeAudioOverlay();
        }
    }
    if (enable && !m_audioWaveDisplayed) {
        createAudioOverlay(m_producer->get_int("video_index") == -1);
    }
}

int GLWidget::setProducer(Mlt::Producer* producer, bool isMulti)
{
    int error = 0;//Controller::setProducer(producer, isMulti);
    /*if (m_producer) {
        delete m_producer;
        m_producer = NULL;
    }*/
    m_producer = producer;
    if (!error && producer) {
        error = reconfigure(isMulti);
        if (!error) {
            // The profile display aspect ratio may have changed.
            resizeGL(width(), height());
        }
    }
    if (m_producer->get_int("video_index") == -1 && !KdenliveSettings::gpu_accel()) {
        // This is an audio only clip, attach visualization filter. Currently, the filter crashes MLT when Movit accel is used
        if (!m_audioWaveDisplayed) {
            createAudioOverlay(true);
        }
        else {
            adjustAudioOverlay(true);
        }
    }
    else if (m_audioWaveDisplayed) {
        // This is not an audio clip, hide wave
        if (KdenliveSettings::displayAudioOverlay()) {
            adjustAudioOverlay(m_producer->get_int("video_index") == -1);
        }
        else {
            removeAudioOverlay();
        }
    }
    return error;
}

void GLWidget::createAudioOverlay(bool isAudio)
{
    Mlt::Filter f(*pCore->binController()->profile(), "audiowaveform");
    if (f.is_valid()) {
        //f.set("show_channel", 1);
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
    while (filter) {
        QString srv = filter->get("mlt_service");
        if (srv == "audiowaveform") {
            sourceService.detach(*filter);
            delete filter;
            break;
        } else ct++;
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
    while (filter) {
        QString srv = filter->get("mlt_service");
        if (srv == "audiowaveform") {
            if (isAudio) {
                filter->set("rect", "0,0,100%,100%");
            }
            else {
                filter->set("rect", "0,80%,100%,20%");
            }
            break;
        } else ct++;
        filter = sourceService.filter(ct);
    }
}


int GLWidget::reconfigure(bool isMulti)
{
    int error = 0;
    // use SDL for audio, OpenGL for video
    QString serviceName = property("mlt_service").toString();
    if (!m_consumer || !m_consumer->is_valid()) {
        if (serviceName.isEmpty()) {
            m_consumer = new Mlt::FilteredConsumer(*pCore->binController()->profile(), "sdl_audio");
            if (m_consumer->is_valid())
                serviceName = "sdl_audio";
            else
                serviceName = "rtaudio";
            delete m_consumer;
        }
        if (isMulti)
            m_consumer = new Mlt::FilteredConsumer(*pCore->binController()->profile(), "multi");
        else
            m_consumer = new Mlt::FilteredConsumer(*pCore->binController()->profile(), serviceName.toLatin1().constData());

        delete m_threadStartEvent;
        m_threadStartEvent = 0;
        delete m_threadStopEvent;
        m_threadStopEvent = 0;

        delete m_threadCreateEvent;
        m_threadCreateEvent = m_consumer->listen("consumer-thread-create", this, (mlt_listener) onThreadCreate);
        delete m_threadJoinEvent;
        m_threadJoinEvent = m_consumer->listen("consumer-thread-join", this, (mlt_listener) onThreadJoin);
    }
    if (m_consumer->is_valid()) {
        m_consumer->stop();
        // Connect the producer to the consumer - tell it to "run" later
        m_consumer->connect(*m_producer);
        // Make an event handler for when a frame's image should be displayed
        m_consumer->listen("consumer-frame-show", this, (mlt_listener) on_frame_show);
        //m_consumer->set("real_time", MLT.realTime());
        m_consumer->set("mlt_image_format", "yuv422");
        //TODO:
        //m_consumer->set("color_trc", Settings.playerGamma().toLatin1().constData());

        if (isMulti) {
            m_consumer->set("terminate_on_pause", 0);
            m_consumer->set("0", serviceName.toLatin1().constData());
            if (serviceName == "sdl_audio")
#ifdef Q_OS_WIN
                m_consumer->set("0.audio_buffer", 2048);
#else
                m_consumer->set("0.audio_buffer", 512);
#endif
            /*if (!m_consumer->profile()->progressive())
                m_consumer->set("0.progressive", property("progressive").toBool());*/
            
            m_consumer->set("0.deinterlace_method", KdenliveSettings::mltdeinterlacer().toUtf8().constData());
            m_consumer->set("0.rescale", KdenliveSettings::mltinterpolation().toUtf8().constData());
            m_consumer->set("0.buffer", 25);
            m_consumer->set("0.prefill", 1);
            if (property("keyer").isValid())
                m_consumer->set("0.keyer", property("keyer").toInt());
        }
        else {
            if (serviceName == "sdl_audio")
#ifdef Q_OS_WIN
                m_consumer->set("audio_buffer", 2048);
#else
                m_consumer->set("audio_buffer", 512);
#endif
            /*if (!m_consumer->profile()->progressive())
                m_consumer->set("progressive", property("progressive").toBool());*/
            m_consumer->set("deinterlace_method", KdenliveSettings::mltdeinterlacer().toUtf8().constData());
            m_consumer->set("rescale", KdenliveSettings::mltinterpolation().toUtf8().constData());
            m_consumer->set("buffer", 25);
            m_consumer->set("prefill", 1);
            m_consumer->set("scrub_audio", 1);
            if (property("keyer").isValid())
                m_consumer->set("keyer", property("keyer").toInt());
        }
        if (m_glslManager) {
            if (!m_threadStartEvent)
                m_threadStartEvent = m_consumer->listen("consumer-thread-started", this, (mlt_listener) onThreadStarted);
            /*if (!m_threadStopEvent)
                m_threadStopEvent = m_consumer->listen("consumer-thread-stopped", this, (mlt_listener) onThreadStopped);*/
            if (!serviceName.startsWith("decklink") && !isMulti)
                m_consumer->set("mlt_image_format", "glsl");
        } else {
            emit started();
        }
    }
    else {
        // Cleanup on error
        error = 2;
        //Controller::closeConsumer();
        //Controller::close();
    }
    rootObject()->setProperty("framebase", QRect(0, 0, m_consumer->profile()->width(), m_consumer->profile()->height()));
    rootObject()->setProperty("framerect", QRect(0, 0, m_consumer->profile()->width(), m_consumer->profile()->height()));
    return error;
}

bool GLWidget::effectSceneVisible() const
{
    QObject *item = rootObject()->findChild<QObject*>("effectscene");
    return item->property("visible").toBool();
}

void GLWidget::slotShowEffectScene(bool show)
{
    if (show) {
        QObject *item = rootObject();
        QObject::connect(item, SIGNAL(qmlSignal()), this, SLOT(effectRectChanged()), Qt::UniqueConnection);
    }
    QObject *item = rootObject()->findChild<QObject*>("effectscene");
    item->setProperty("visible", show);
}

float GLWidget::zoom() const 
{ 
    return m_zoom;// * m_consumer->profile()->width() / m_rect.width();
}

QSize GLWidget::profileSize() const
{
    return QSize(m_consumer->profile()->width(), m_consumer->profile()->height());
}

QPoint GLWidget::offset() const
{
    return QPoint(m_offset.x() - (m_consumer->profile()->width()  * m_zoom -  width()) / 2,
                  m_offset.y() - (m_consumer->profile()->height() * m_zoom - height()) / 2);
}

void GLWidget::setZoom(float zoom)
{
    double zoomRatio = zoom / m_zoom;
    m_zoom = zoom;
    emit zoomChanged();
    double scale = rootObject()->property("scale").toDouble() * zoomRatio;
    rootObject()->setProperty("scale", scale);
    update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    QQuickView::mouseReleaseEvent(event);
    m_dragStart = QPoint();
    if (event->button() != Qt::RightButton && !effectSceneVisible()) {
        emit monitorPlay();
    }
}

void GLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    emit switchFullScreen();
    event->accept();
}

void GLWidget::setOffsetX(int x)
{
    m_offset.setX(x);
    emit offsetChanged();
    update();
}

void GLWidget::setOffsetY(int y)
{
    m_offset.setY(y);
    emit offsetChanged();
    update();
}

/*void GLWidget::setCurrentFilter(QmlFilter* filter, QmlMetadata* meta)
{
    rootContext()->setContextProperty("filter", filter);
    if (meta && QFile::exists(meta->vuiFilePath().toLocalFile())) {
        setSource(meta->vuiFilePath());
    } else {
        setBlankScene();
    }
}*/

Mlt::Consumer *GLWidget::consumer()
{
    return m_consumer;
}

void GLWidget::updateTexture(GLuint yName, GLuint uName, GLuint vName)
{
    m_texture[0] = yName;
    m_texture[1] = uName;
    m_texture[2] = vName;
    emit textureUpdated();
}

// MLT consumer-frame-show event handler
void GLWidget::on_frame_show(mlt_consumer, void* self, mlt_frame frame_ptr)
{
    GLWidget* widget = static_cast<GLWidget*>(self);
    int timeout = (widget->consumer()->get_int("real_time") > 0)? 0: 1000;
    if (widget->m_frameRenderer && widget->m_frameRenderer->semaphore()->tryAcquire(1, timeout)) {
        Mlt::Frame frame(frame_ptr);
        if (frame.get_int("rendered")) {
            QMetaObject::invokeMethod(widget->m_frameRenderer, "showFrame", Qt::QueuedConnection, Q_ARG(Mlt::Frame, frame));
        }
    }
}


RenderThread::RenderThread(thread_function_t function, void *data, QOpenGLContext *context)
    : QThread(0)
    , m_function(function)
    , m_data(data)
    , m_context(0)
    , m_surface(0)
{
    qDebug()<<"=========\nSTARTING RDR THREAD\n================";
    if (context) {
        m_context = new QOpenGLContext;
        m_context->setFormat(context->format());
        m_context->setShareContext(context);
        m_context->create();
        m_surface = new QOffscreenSurface;
        m_surface->setFormat(m_context->format());
        m_surface->create();
        m_context->moveToThread(this);
    }
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
    qDebug()<<"=========\nRDR THREAD DONE!!\n================";
}

FrameRenderer::FrameRenderer(QOpenGLContext* shareContext)
     : QThread(0)
     , m_semaphore(3)
     , m_frame()
     , m_context(0)
     , m_surface(0)
     , m_gl32(0)
{
    Q_ASSERT(shareContext);
    m_renderTexture[0] = m_renderTexture[1] = m_renderTexture[2] = 0;
    m_displayTexture[0] = m_displayTexture[1] = m_displayTexture[2] = 0;
    m_context = new QOpenGLContext;
    m_context->setFormat(shareContext->format());
    m_context->setShareContext(shareContext);
    m_context->create();
    m_surface = new QOffscreenSurface;
    m_surface->setFormat(m_context->format());
    m_surface->create();
    m_context->moveToThread(this);
    setObjectName("FrameRenderer");
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
    if (m_context->isValid()) {
        int width = 0;
        int height = 0;

        m_context->makeCurrent(m_surface);

        if (KdenliveSettings::gpu_accel()) {
            frame.set("movit.convert.use_texture", 1);
            mlt_image_format format = mlt_image_glsl_texture;
            const GLuint* textureId = (GLuint*) frame.get_image(format, width, height);

#ifdef USE_GL_SYNC
            GLsync sync = (GLsync) frame.get_data("movit.convert.fence");
            if (sync) {
#ifdef Q_OS_WIN
                // On Windows, use QOpenGLFunctions_3_2_Core instead of getProcAddress.
                if (!m_gl32) {
                    m_gl32 = m_context->versionFunctions<QOpenGLFunctions_3_2_Core>();
                    if (m_gl32)
                        m_gl32->initializeOpenGLFunctions();
                }
                if (m_gl32) {
                    m_gl32->glClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
                    check_error();
                }
#else
                if (ClientWaitSync) {
                    ClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
                    check_error();
                }
#endif // Q_OS_WIN
            }
#else
            glFinish();
#endif // USE_GL_FENCE
            emit textureReady(*textureId);
        }
        else {
            mlt_image_format format = mlt_image_yuv420p;
            const uint8_t* image = frame.get_image(format, width, height);

            // Upload each plane of YUV to a texture.
            if (m_renderTexture[0] && m_renderTexture[1] && m_renderTexture[2])
                glDeleteTextures(3, m_renderTexture);
            glGenTextures(3, m_renderTexture);
            check_error();
            glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

            glBindTexture  (GL_TEXTURE_2D, m_renderTexture[0]);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            check_error();
            glTexImage2D   (GL_TEXTURE_2D, 0, GL_RED, width, height, 0,
                            GL_RED, GL_UNSIGNED_BYTE, image);
            check_error();

            glBindTexture  (GL_TEXTURE_2D, m_renderTexture[1]);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            check_error();
            glTexImage2D   (GL_TEXTURE_2D, 0, GL_RED, width/2, height/4, 0,
                            GL_RED, GL_UNSIGNED_BYTE, image + width * height);
            check_error();

            glBindTexture  (GL_TEXTURE_2D, m_renderTexture[2]);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            check_error();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            check_error();
            glTexImage2D   (GL_TEXTURE_2D, 0, GL_RED, width/2, height/4, 0,
                            GL_RED, GL_UNSIGNED_BYTE, image + width * height + width/2 * height/2);
            check_error();

            glBindTexture(GL_TEXTURE_2D, 0);
            check_error();
            glFinish();

            for (int i = 0; i < 3; ++i)
                qSwap(m_renderTexture[i], m_displayTexture[i]);
            emit textureReady(m_displayTexture[0], m_displayTexture[1], m_displayTexture[2]);
        }

        m_context->doneCurrent();
        // Save this frame for future use and to keep a reference to the GL Texture.
        m_frame = SharedFrame(frame);

        // The frame is now done being modified and can be shared with the rest
        // of the application.
        emit frameDisplayed(m_frame);
    }
    m_semaphore.release();
}

SharedFrame FrameRenderer::getDisplayFrame()
{
    return m_frame;
}

void FrameRenderer::cleanup()
{
    if (m_renderTexture[0] && m_renderTexture[1] && m_renderTexture[2]) {
        m_context->makeCurrent(m_surface);
        glDeleteTextures(3, m_renderTexture);
        if (m_displayTexture[0] && m_displayTexture[1] && m_displayTexture[2])
            glDeleteTextures(3, m_displayTexture);
        m_context->doneCurrent();
        m_renderTexture[0] = m_renderTexture[1] = m_renderTexture[2] = 0;
        m_displayTexture[0] = m_displayTexture[1] = m_displayTexture[2] = 0;
    }
}


