/*
 * Copyright (c) 2011 Meltytech, LLC
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

#include <KDebug>
#include <QtGui>
#include <GL/glew.h>
#include <mlt++/Mlt.h>
#include "glslwidget.h"
#include "project/producerwrapper.h"
#include "kdenlivesettings.h"
#include <QGLPixelBuffer>
#include <QDesktopWidget>
#include <KLocale>

#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "GL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); exit(1); } }


GLSLWidget::GLSLWidget(Mlt::Profile *profile, QWidget *parent)
    : QGLWidget(parent)
    , MltController(profile)
    , showFrameSemaphore(3)
    , m_image_width(0)
    , m_image_height(0)
    , m_glslManager(0)
    , m_fbo(0)
    , m_isInitialized(false)
    , m_threadStartEvent(0)
    , m_threadStopEvent(0)
    , m_image_format(mlt_image_glsl_texture)
    , m_showPlay(false)
    , m_renderContext(0)
    , m_createThumb(false)
{
    m_display_ratio = 4.0/3.0;
    m_texture[0] = 0;
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    m_type = MLTGLSL;
    activate();
}

void GLSLWidget::activate()
{
    if (!m_glslManager)
        m_glslManager = new Mlt::Filter(profile(), "glsl.manager");
    if ((m_glslManager && !m_glslManager->is_valid())) {
        delete m_glslManager;
        m_glslManager = 0;
    }
    if (m_glslManager && !m_renderContext) {
        m_renderContext = new QGLWidget(this, this);
        m_renderContext->resize(0, 0);
    }
    m_isActive = true;
}

GLSLWidget::~GLSLWidget()
{
    makeCurrent();
    delete m_fbo;
    closeConsumer();
    delete m_threadStartEvent;
    delete m_threadStopEvent;
}


void GLSLWidget::closeConsumer()
{
    MltController::closeConsumer();
    if (m_glslManager) {
        //Mlt::Service service(m_producer->get_service());
        //service.set("disable", "1");
        stopGlsl();
        delete m_renderContext;
        m_renderContext = 0;
        makeCurrent();
        delete m_glslManager;
        m_glslManager = 0;
        // Need to destroy MLT global reference to prevent filters from trying to use GPU.
        mlt_properties_set_data(mlt_global_properties(), "glslManager", NULL, 0, NULL, NULL);
    }
    m_isActive = false;
}

QSize GLSLWidget::minimumSizeHint() const
{
    return QSize(40, 30);
}

QSize GLSLWidget::sizeHint() const
{
    return QSize(400, 300);
}

void GLSLWidget::initializeGL()
{
    QPalette palette;
    QSettings settings;
    if (!m_glslManager) // settings.value("player/gpu", false).toBool() &&
        emit gpuNotSupported();
    glewInit();
    glClearColor(KdenliveSettings::window_background().redF(),
                 KdenliveSettings::window_background().greenF(),
                 KdenliveSettings::window_background().blueF(),
                 1.0f);
    glShadeModel(GL_FLAT);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    m_condition.wakeAll();
    m_isInitialized = true;
}


void GLSLWidget::resizeGL(int width, int height)
{
    double this_aspect = (double) width / height;

    // Special case optimisation to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if ((int) (this_aspect * 1000) == (int) (m_display_ratio * 1000))
    {
        w = width;
        h = height;
    }
    // Use OpenGL to normalise sample aspect ratio
    else if (height * m_display_ratio > width)
    {
        w = width;
        h = width / m_display_ratio;
    }
    else
    {
        w = height * m_display_ratio;
        h = height;
    }
    x = (width - w) / 2;
    y = (height - h) / 2;
    makeCurrent();
    if (isValid()) {
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}


void GLSLWidget::resizeEvent(QResizeEvent* event)
{
    resizeGL(event->size().width(), event->size().height());
    event->accept();
}

void GLSLWidget::showEvent(QShowEvent* event)
{
    initializeGL();
    resizeGL(width(), height());
    event->accept();
}

void GLSLWidget::paintGL()
{
    //makeCurrent();
    if (m_texture[0]) {
        if (m_glslManager && m_fbo && m_image_format == mlt_image_glsl_texture) {
            //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBindTexture(GL_TEXTURE_2D, m_fbo->texture());
            check_error();
        }
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0); glVertex2i(x, y);
        glTexCoord2i(1, 0); glVertex2i(x + w, y);
        glTexCoord2i(1, 1); glVertex2i(x + w, y + h);
        glTexCoord2i(0, 1); glVertex2i(x, y + h);
        glEnd();
    }
    //glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    //return;
    
    if (m_showPlay) {
        //glColor4f(1,1,1, 0.5);
        glEnable(GL_BLEND);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc( GL_ONE, GL_ONE);
        //glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
        //glBlendFunc( GL_SRC_ALPHA, GL_ONE);
        glColor4f(0.2,0.2,0.2, 0.6);
        //glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);
        float x1,y1,x2,y2;
        double radius = 0.15;
        x1=0.49;
        y1=0.5;
        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(x1,y1);
        glVertex2f(x + x1 * w,y + y1 * h);
        const float PI = 3.1415926535897932;
        for (float angle = 0.0; angle < 2.0*PI; angle += 2.0*PI/30.0) {
            x2 = x1+sin(angle)*radius / w * h;
            y2 = y1+cos(angle)*radius;
            glTexCoord2f(x2, y2);
            glVertex2f(x + x2 * w,y + y2 * h);
        }
        glEnd();
        //glDisable(GL_BLEND);
        //glBlendFunc( GL_SRC_ALPHA, GL_ONE);
        glColor4f(0.5,0.5,0.5, 0.7);
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        //glBlendFunc( GL_ONE, GL_DST_ALPHA);
        //glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);
        glBegin(GL_TRIANGLES);
        glTexCoord2f(0.45, 0.4);glVertex2f( x + 0.45 * w, y + 0.4 * h);
        glTexCoord2f(0.56, 0.5);glVertex2f( x + 0.56 * w, y + 0.5 * h);
        glTexCoord2f(0.45, 0.6);glVertex2f( x + 0.45 * w, y + 0.6 * h);
        glEnd();
        glBlendEquation(GL_FUNC_ADD);
        glDisable(GL_BLEND);

        glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
    }
}

void GLSLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    switchFullScreen();
    event->accept();
}

void GLSLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    QGLWidget::mouseReleaseEvent(event);
    m_showPlay = !m_showPlay;
    emit requestPlayPause();
}

void GLSLWidget::wheelEvent(QWheelEvent* event)
{
    int factor = 1;
    if (event->modifiers() & Qt::ControlModifier) factor = (int) profile().fps();
    if (event->delta() > 0) nextFrame(factor);
    else previousFrame(factor);
}

void GLSLWidget::mousePressEvent(QMouseEvent* event)
{
    QGLWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton)
        m_dragStart = event->pos();
    emit dragStarted();
}

void GLSLWidget::enterEvent( QEvent * event )
{
    if (m_producer && m_producer->get_speed() == 0) {
        m_showPlay = true;
        if (m_isActive) glDraw();
        //update();
    }
    else m_showPlay = false;
    QGLWidget::enterEvent(event);
}

void GLSLWidget::leaveEvent( QEvent * event )
{
    if (m_showPlay) {
        m_showPlay = false;
        if (m_isActive) glDraw();
        //update();
    }
    //event->setAccepted(true);
    QGLWidget::leaveEvent(event);
}

void GLSLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->modifiers() == Qt::ShiftModifier && m_producer) {
        emit seekTo(m_producer->get_length() * event->x() / width());
        return;
    }
    if (event->buttons() == Qt::NoButton) {
        if (m_overlayZone.contains(event->pos())) setCursor(Qt::PointingHandCursor);
        else setCursor(Qt::ArrowCursor);
    }
    
    
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if ((event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance())
        return;
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/mlt+xml", "");
    drag->setMimeData(mimeData);
    drag->exec(Qt::LinkAction);
}

void GLSLWidget::startGlsl()
{
    if (!m_isInitialized) {
        m_mutex.lock();
        m_condition.wait(&m_mutex);
        m_mutex.unlock();
    }
    if (m_glslManager && m_renderContext && m_renderContext->isValid()) {
        m_renderContext->makeCurrent();
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

static void onThreadStarted(mlt_properties /*owner*/, GLSLWidget* self)
{
    self->startGlsl();
}

void GLSLWidget::stopGlsl()
{
    //kDebug()<<" * * * *STOPPING GLSL--------------";
    m_texture[0] = 0;
    m_renderContext->doneCurrent();
}

static void onThreadStopped(mlt_properties /*owner*/, GLSLWidget* self)
{
    self->stopGlsl();
}

void GLSLWidget::refreshConsumer()
{
    //if (!m_isActive) return;
    if (m_glslManager && m_consumer->is_stopped()) {
        this->makeCurrent();
        if (m_consumer) m_consumer->start();
    }
    MltController::refreshConsumer();
    glDraw();
}

void GLSLWidget::showFrame(Mlt::QFrame frame)
{
    //if (!m_isActive) return;
    if (frame.frame()->get_int("rendered")) {
        m_image_width = 0;
        m_image_height = 0;

	int position = frame.frame()->get_position();
	if (!m_zone.isNull()) {
	    if (m_zone.x() == position) {
		// Zone in or out
		m_overlayText = i18n("Zone in");
		m_overlayColor = QColor(255, 90, 90);
	    }
	    else if (m_zone.y() == position) {
		// Zone in or out
		m_overlayText = i18n("Zone out");
		m_overlayColor = QColor(255, 90, 90);
	    }
	    else {
		m_overlayText.clear();
		m_overlayZone = QRectF();
	    }
        }
        else if (m_markers.contains(position)) {
            // Marker
            m_overlayText = m_markers.value(position);
            m_overlayColor = QColor(60, 60, 255);
        }
        else {
            m_overlayText.clear();
            m_overlayZone = QRectF();
        }
        //makeCurrent();
        if (m_glslManager && m_image_format == mlt_image_glsl_texture) {
            frame.frame()->set("movit.convert.use_texture", 1);
            const GLuint* textureId = (GLuint*) frame.frame()->get_image(m_image_format, m_image_width, m_image_height);
	    if (!textureId) {
		kDebug()<<" * * *CANNOT GET GLU TEXTURE * * ** \n";
		return;
	    }
            m_texture[0] = *textureId;
            if (!m_fbo || m_fbo->width() != m_image_width || m_fbo->height() != m_image_height) {
                delete m_fbo;
                m_fbo = new QGLFramebufferObject(m_image_width, m_image_height, GL_TEXTURE_2D);
            }
            glPushAttrib(GL_VIEWPORT_BIT);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();

            glBindFramebuffer( GL_FRAMEBUFFER, m_fbo->handle());
            check_error();

            glViewport( 0, 0, m_image_width, m_image_height );
            glMatrixMode( GL_PROJECTION );
            glLoadIdentity();
            glOrtho(0.0, m_image_width, 0.0, m_image_height, -1.0, 1.0);
            glMatrixMode( GL_MODELVIEW );
            glLoadIdentity();

            glActiveTexture( GL_TEXTURE0 );
            check_error();
            glBindTexture( GL_TEXTURE_2D, m_texture[0]);
            check_error();

            glBegin( GL_QUADS );
            glTexCoord2i( 0, 0 ); glVertex2i( 0, 0 );
            glTexCoord2i( 0, 1 ); glVertex2i( 0, m_image_height );
            glTexCoord2i( 1, 1 ); glVertex2i( m_image_width, m_image_height );
            glTexCoord2i( 1, 0 ); glVertex2i( m_image_width, 0 );
            glEnd();
            check_error();
	    if (!m_overlayText.isEmpty()) {
		glEnable(GL_BLEND);
		QPainter p(m_fbo);
		p.setRenderHints(QPainter::TextAntialiasing, true);
		QFont f = font();
		f.setPixelSize(18 * m_image_height / h);
		f.setStyleStrategy(QFont::PreferAntialias);
		p.setFont(f);
		p.scale(1, -1);
		//p.setLayoutDirection(Qt::RightToLeft);
		
		QRectF textRect = p.boundingRect(0, -20 - f.pixelSize(), m_image_width - 15, -m_image_height / 2 + 10, Qt::AlignRight | Qt::AlignTop, m_overlayText);
		QRectF bgRect = textRect.adjusted(-(5 + textRect.height()) , -2, 5, 2);
		
		QColor c(Qt::white);
		c.setAlpha(100);
		p.setBrush(c);
		p.setPen(Qt::NoPen);
		// TODO: calculate transform to get correct zone
		m_overlayZone = bgRect;
		p.drawRoundedRect(bgRect, 3, 3);
		bgRect.setWidth(textRect.height());
		p.setBrush(m_overlayColor);
		p.drawEllipse(bgRect.adjusted(2, 2, 0, -2));
		p.setPen(Qt::black);
		p.drawText(textRect, m_overlayText);
		
		/*p.fillRect(15, -180, 600, -120, QColor(200, 100, 100, 120));
		p.setPen(Qt::white);
		//QString overlay = QString("Overlay test: %1").arg(frame.frame()->get_position());
		p.drawText(20, -200, m_overlayText);*/
		p.end();
		glDisable(GL_BLEND);
	    }
	    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
            check_error();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopAttrib();
        }
        glDraw();
    }
    showFrameSemaphore.release();
}

void GLSLWidget::renderImage(const QString &id, ProducerWrapper *producer, QList <int> positions, int width, int height)
{
    //setUpdatesEnabled(false);
    if (m_glslManager) {
        //if (m_consumer && !m_consumer->is_stopped()) m_consumer->stop();
        m_renderContext->makeCurrent();
    }
    // Position might be negative to indicate the in point on the imageRendered signal.
    if (width == -1) {
        height = 100;
        width = height * profile().dar();
    }
    while (!positions.isEmpty()) {
        int position = positions.takeFirst();
        producer->seek(qAbs(position));
	
        Mlt::Frame* frame = producer->get_frame();
        QImage image = MltController::image(frame, width, height);
        emit imageRendered(id, position, image);
        delete frame;
    }

    if (m_glslManager) {
        this->makeCurrent();
        //if (m_consumer) m_consumer->start();
    }
    //setUpdatesEnabled(true);
}

int GLSLWidget::reOpen(bool isMulti)
{
    //QMutexLocker lock(&m_mutex);
    activate();
    int error = 0;//MltController::open(new ProducerWrapper(profile(), resource()), isMulti); //producer, isMulti);
    bool reconnect = !m_consumer || !m_consumer->is_valid();
    error = reconfigure(isMulti);
    if (!error) {
        if (reconnect)
            connect(this, SIGNAL(frameReceived(Mlt::QFrame)),
                    this, SLOT(showFrame(Mlt::QFrame)), Qt::UniqueConnection);
        resizeGL(width(), height());
        //refreshConsumer();
    }
    return error;
}

void GLSLWidget::slotGetThumb(ProducerWrapper *producer, int position)
{
    //setUpdatesEnabled(false);
    //if (!m_isActive) return;
    if (!m_producer/* || m_producer->get_speed() != 0*/) {
        // No thumbnail when playing
        emit gotThumb(position, QImage());
        return;
    }
    //if (m_consumer && !m_consumer->is_stopped()) m_consumer->stop();
    m_renderContext->makeCurrent();
    
    //QPixmap pix = this->renderPixmap(300, 200);
    //pix.save("/tmp/mypix.png");
    //m_createThumb = false;
    
    QImage result = MltController::thumb(producer, position);
    
    this->makeCurrent();
    //if (m_consumer) m_consumer->start();
    emit gotThumb(position, result);
    //setUpdatesEnabled(true);
}

int GLSLWidget::open(ProducerWrapper* producer, bool isMulti, bool isLive)
{
    //QMutexLocker lock(&m_mutex);
    //m_originalProducer = producer;
    int error = MltController::open(producer, isMulti, isLive);
    if (!error) {
        bool reconnect = !m_consumer || !m_consumer->is_valid();
        error = reconfigure(isMulti);
        if (!error) {
            if (reconnect)
                connect(this, SIGNAL(frameReceived(Mlt::QFrame)),
                        this, SLOT(showFrame(Mlt::QFrame)), Qt::UniqueConnection);
            resizeGL(width(), height());
            refreshConsumer();
        }
    }
    //emit producerChanged();
    return error;
}

int GLSLWidget::reconfigure(bool isMulti)
{
    int error = 0;
    // use SDL for audio, OpenGL for video
    //QString serviceName = property("mlt_service").toString();
    // Warning: Only rtaudio allows simultaneous consumers, SDL freezes.
    QString serviceName = "rtaudio";
    if (!m_consumer || !m_consumer->is_valid()) {
        if (serviceName.isEmpty()) {
            m_consumer = new Mlt::FilteredConsumer(profile(), "sdl_audio");
            if (m_consumer->is_valid())
                serviceName = "sdl_audio";
            else
                serviceName = "rtaudio";
            delete m_consumer;
        }
        if (isMulti)
            m_consumer = new Mlt::FilteredConsumer(profile(), "multi");
        else
            m_consumer = new Mlt::FilteredConsumer(profile(), serviceName.toAscii().constData());

        Mlt::Filter* filter = new Mlt::Filter(profile(), "audiolevel");
        if (filter->is_valid())
            m_consumer->attach(*filter);
        delete filter;
        delete m_threadStartEvent;
        m_threadStartEvent = 0;
        delete m_threadStopEvent;
        m_threadStopEvent = 0;
    }
    if (m_consumer->is_valid()) {
        // Connect the producer to the consumer - tell it to "run" later
        QString producer_service = m_producer->get("mlt_service");
        m_consumer->connect(*m_producer);
        if (producer_service == "webvfx") {
            Mlt::Service srv(m_producer->get_service());
            srv.set("consumer", m_consumer->get_consumer(), 0, NULL, NULL);
        }
        // Make an event handler for when a frame's image should be displayed
        m_consumer->listen("consumer-frame-show", this, (mlt_listener) on_frame_show);
        m_consumer->set("real_time", property("realtime").toBool()? 1 : -1);
        m_consumer->set("mlt_image_format", "yuv422");
        m_display_ratio = profile().dar();

        if (isMulti) {
            m_consumer->set("terminate_on_pause", 0);
            m_consumer->set("0", serviceName.toAscii().constData());
            if (serviceName == "sdl_audio")
                m_consumer->set("0.audio_buffer", 512);

            if (!profile().progressive())
                m_consumer->set("0.progressive", property("progressive").toBool());
            m_consumer->set("0.rescale", property("rescale").toString().toAscii().constData());
            m_consumer->set("0.deinterlace_method", property("deinterlace_method").toString().toAscii().constData());
            m_consumer->set("0.buffer", 25);
            m_consumer->set("0.prefill", 1);
        }
        else {
            if (serviceName == "sdl_audio")
                m_consumer->set("audio_buffer", 512);

            if (!profile().progressive())
                m_consumer->set("progressive", property("progressive").toBool());
            m_consumer->set("rescale", property("rescale").toString().toAscii().constData());
            m_consumer->set("deinterlace_method", property("deinterlace_method").toString().toAscii().constData());
            m_consumer->set("buffer", 25);
            m_consumer->set("prefill", 1);
            m_consumer->set("scrub_audio", 1);
        }
        if (m_glslManager) {
            if (!m_threadStartEvent)
                m_threadStartEvent = m_consumer->listen("consumer-thread-started", this, (mlt_listener) onThreadStarted);
            if (!m_threadStopEvent)
                m_threadStopEvent = m_consumer->listen("consumer-thread-stopped", this, (mlt_listener) onThreadStopped);
            if (!serviceName.startsWith("decklink") && !isMulti)
                m_consumer->set("mlt_image_format", "glsl");
            m_image_format = mlt_image_glsl_texture;
        }
        m_consumer->start();
        if (!m_isLive) m_producer->set_speed(0.0);
    }
    else {
        // Cleanup on error
        error = 2;
        MltController::closeConsumer();
        MltController::close();
    }
    return error;
}

// MLT consumer-frame-show event handler
void GLSLWidget::on_frame_show(mlt_consumer, void* self, mlt_frame frame_ptr)
{
    GLSLWidget* widget = static_cast<GLSLWidget*>(self);
    if (widget->showFrameSemaphore.tryAcquire()) {
        Mlt::Frame frame(frame_ptr);
        emit widget->frameReceived(Mlt::QFrame(frame));
    }
}


void GLSLWidget::switchFullScreen()
{
    // TODO: disable screensaver?
    Qt::WindowFlags flags = windowFlags();
    if (!isFullScreen()) {
        // Check if we ahave a multiple monitor setup
        //setUpdatesEnabled(false);
#if QT_VERSION >= 0x040600
        int monitors = QApplication::desktop()->screenCount();
#else
        int monitors = QApplication::desktop()->numScreens();
#endif
        if (monitors > 1) {
            QRect screenres;
            // Move monitor widget to the second screen (one screen for Kdenlive, the other one for the Monitor widget
            int currentScreen = QApplication::desktop()->screenNumber(this);
            if (currentScreen < monitors - 1)
                screenres = QApplication::desktop()->screenGeometry(currentScreen + 1);
            else
                screenres = QApplication::desktop()->screenGeometry(currentScreen - 1);
            move(QPoint(screenres.x(), screenres.y()));
            resize(screenres.width(), screenres.height());
        }

        m_baseFlags = flags & (Qt::Window | Qt::SubWindow);
        flags |= Qt::Window;
        flags ^= Qt::SubWindow;
        setWindowFlags(flags);
#ifdef Q_WS_X11
        // This works around a bug with Compiz
        // as the window must be visible before we can set the state
        show();
        raise();
        setWindowState(windowState() | Qt::WindowFullScreen);   // set
#else
        setWindowState(windowState() | Qt::WindowFullScreen);   // set
        //setUpdatesEnabled(true);
        show();
#endif
        setEnabled(true);
    } else {
        //setUpdatesEnabled(false);
        flags ^= (Qt::Window | Qt::SubWindow); //clear the flags...
        flags |= m_baseFlags; //then we reset the flags (window and subwindow)
        setWindowFlags(flags);
        setWindowState(windowState()  ^ Qt::WindowFullScreen);   // reset
        //setUpdatesEnabled(true);
        //setEnabled(false);
        show();
    }
}

void GLSLWidget::slotSetZone(const QPoint &zone)
{
    if (m_zone != zone) {
        m_zone = zone;
        if (m_producer->get_speed() == 0)
            refreshConsumer();
    }
}

void GLSLWidget::slotSetMarks(const QMap <int,QString> &marks)
{
    if (m_markers != marks) {
        m_markers = marks;
        if (m_producer->get_speed() == 0)
            refreshConsumer();
    }
}


#include "glslwidget.moc"
