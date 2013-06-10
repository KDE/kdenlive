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
#include "scenewidget.h"
#include "project/producerwrapper.h"
#include "monitorgraphicsscene.h"
#include <QGraphicsView>
#include <QDesktopWidget>

#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "GL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); exit(1); } }

MyGraphicsView::MyGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);//Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    //setViewport(static_cast<QGLWidget*>(parent));
}

MyGraphicsView::MyGraphicsView(QGraphicsScene *scene, QWidget *parent) : QGraphicsView(scene, parent)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);//Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    //setViewport(static_cast<QGLWidget*>(parent));
}

void MyGraphicsView::showEvent(QShowEvent* event)
{
    QGraphicsView::showEvent(event);
    if (scene()) static_cast <MonitorGraphicsScene*>(scene())->slotResized();
}

void MyGraphicsView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (scene()) static_cast <MonitorGraphicsScene*>(scene())->slotResized();
}

void MyGraphicsView::wheelEvent(QWheelEvent* event)
{
    if (horizontalScrollBar()->isVisible() || event->modifiers() & Qt::ControlModifier) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    emit gotWheelEvent(event);
}

void MyGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->modifiers() == Qt::ShiftModifier) {
        emit seekTo((double) event->x() / width());
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}


void MyGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
    QGraphicsView::mouseReleaseEvent(event);
    emit switchPlay();
}

void MyGraphicsView::mouseDoubleClickEvent(QMouseEvent * event)
{
    switchFullScreen();
    if (scene()) static_cast <MonitorGraphicsScene*>(scene())->slotResized();
    event->accept();
}

void MyGraphicsView::switchFullScreen()
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
        //setUpdatesEnabled(true);
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


SceneWidget::SceneWidget(Mlt::Profile *profile, QWidget *parent)
    : QGLWidget(parent)
    , MltController(profile)
    , m_image_width(0)
    , m_image_height(0)
    , m_shader(0)
    , m_fbo(0)
    , m_image_format(mlt_image_rgb24a)
    , m_showPlay(false)
    , m_view(NULL)
{
    /*setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);*/
    m_display_ratio = 4.0/3.0;
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    //QGLWidget *glWidget = new QGLWidget(this);
    //setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_view = new MyGraphicsView(parent);
    m_view->setViewport(this);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoScene = new MonitorGraphicsScene(profile->width(), profile->height(), parent);
    m_view->setScene(m_videoScene);
    m_videoScene->initializeGL(this);
    connect(m_view, SIGNAL(gotWheelEvent(QWheelEvent*)), this, SLOT(slotWheelEvent(QWheelEvent*)));
    connect(m_view, SIGNAL(switchPlay()), this, SLOT(slotSwitchPlay()));
    connect(m_view, SIGNAL(seekTo(double)), this, SLOT(slotSeekTo(double)));
    //kDebug()<<"// inint scene wdgt: "<<profile.width()<<"x"<< profile.height();
    m_isActive = true;
    m_type = MLTSCENE;
}

SceneWidget::~SceneWidget()
{
    //delete m_view;
    //delete m_videoScene;
}


void SceneWidget::initializeGL()
{
    makeCurrent();
    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
}



QSize SceneWidget::minimumSizeHint() const
{
    return QSize(40, 30);
}

QSize SceneWidget::sizeHint() const
{
    return QSize(400, 300);
}

void SceneWidget::slotSwitchPlay()
{
    switchPlay();
    emit stateChanged();
}

void SceneWidget::mouseReleaseEvent(QMouseEvent* event)
{
    m_showPlay = !m_showPlay;
    event->accept();
}

void SceneWidget::slotWheelEvent(QWheelEvent* event)
{
    int factor = 1;
    if (event->modifiers() & Qt::ControlModifier) factor = (int) profile().fps();
    if (event->delta() > 0) nextFrame(factor);
    else previousFrame(factor);
    event->accept();
}

void SceneWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_dragStart = event->pos();
    emit dragStarted();
    event->accept();
}


void SceneWidget::enterEvent( QEvent * event )
{
    if (m_producer && m_producer->get_speed() == 0) m_showPlay = true;
    else m_showPlay = false;
    event->accept();
    //glDraw();
}

void SceneWidget::leaveEvent( QEvent * event )
{
    if (m_showPlay) {
        m_showPlay = false;
        //glDraw();
    }
    event->accept();
}

void SceneWidget::slotSeekTo(double percentage)
{
    emit seekTo((int) m_producer->get_length() * percentage);
}

void SceneWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->modifiers() == Qt::ShiftModifier && m_producer) {
        emit seekTo(m_producer->get_length() * event->x() / width());
        return;
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

void SceneWidget::showFrame(Mlt::QFrame frame)
{
    m_videoScene->showFrame(frame.frame()->get_frame());
}

void SceneWidget::renderImage(const QString &id, ProducerWrapper *producer, QList <int> positions, int width, int height)
{
    if (width == -1) {
        height = 100;
        width = height * profile().dar();
    }
    while (!positions.isEmpty()) {
        int position = positions.takeFirst();
        producer->seek(qAbs(position));
        Mlt::Frame* frame = producer->get_frame();
        QImage img = MltController::image(frame, width, height);
        emit imageRendered(id, position, img);
        delete frame;
    }
}

void SceneWidget::slotGetThumb(ProducerWrapper *prod, int position)
{
    if (!m_producer || m_producer->get_speed() != 0) {
        // No thumbnail when playing
        emit gotThumb(position, QImage());
        return;
    }
    emit gotThumb(position, MltController::thumb(prod, position));
}

int SceneWidget::reOpen(bool isMulti)
{
    int error = 0;
    bool reconnect = !m_consumer || !m_consumer->is_valid();
    error = reconfigure(isMulti);
    if (!error) {
        if (reconnect)
            connect(this, SIGNAL(frameReceived(Mlt::QFrame)),
                    this, SLOT(showFrame(Mlt::QFrame)), Qt::UniqueConnection);
        resizeGL(width(), height());
        refreshConsumer();
    }
    return error;
}

int SceneWidget::open(ProducerWrapper* producer, bool isMulti, bool isLive)
{
    //m_originalProducer = producer;
    int error = MltController::open(producer, isMulti, isLive); //producer, isMulti);
    //int error = MltController::open(producer, isMulti);
    fprintf(stderr, " +OPENING PRODUCER + + +\n");
    if (!error) {
        bool reconnect = !m_consumer || !m_consumer->is_valid();
        error = reconfigure(isMulti);
        if (!error) {
            if (reconnect)
                connect(this, SIGNAL(frameReceived(Mlt::QFrame)),
                        this, SLOT(showFrame(Mlt::QFrame)), Qt::UniqueConnection);
            resizeGL(width(), height());
            refreshConsumer();
            update();
        }
    }
    emit producerChanged();
    return error;
}

int SceneWidget::reconfigure(bool isMulti)
{
    int error = 0;
    // use SDL for audio, OpenGL for video
    QString serviceName = property("mlt_service").toString();
    if (!m_consumer || !m_consumer->is_valid()) {
        if (serviceName.isEmpty()) {
            m_consumer = new Mlt::FilteredConsumer(profile(), "sdl_audio");
            if (m_consumer->is_valid())
                serviceName = "sdl_audio";
            else
                serviceName = "rtaudio";
            delete m_consumer;
        }
        // Warning: Only rtaudio allows simultaneous consumers, SDL freezes.
        serviceName = "rtaudio";
        if (isMulti)
            m_consumer = new Mlt::FilteredConsumer(profile(), "multi");
        else
            m_consumer = new Mlt::FilteredConsumer(profile(), serviceName.toAscii().constData());

        Mlt::Filter* filter = new Mlt::Filter(profile(), "audiolevel");
        if (filter->is_valid())
            m_consumer->attach(*filter);
        delete filter;
    }
    if (m_consumer->is_valid()) {
        // Connect the producer to the consumer - tell it to "run" later
        m_consumer->connect(*m_producer);
        // Make an event handler for when a frame's image should be displayed
        m_consumer->listen("consumer-frame-show", this, (mlt_listener) on_frame_show);
        m_consumer->set("real_time", property("realtime").toBool()? 1 : -1);
        //if (!sceneMode) m_consumer->set("mlt_image_format", "yuv422");
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
            m_consumer->set("0.buffer", 1);
        }
        else {
            if (serviceName == "sdl_audio")
                m_consumer->set("audio_buffer", 512);

            if (!profile().progressive())
                m_consumer->set("progressive", property("progressive").toBool());
            m_consumer->set("rescale", property("rescale").toString().toAscii().constData());
            m_consumer->set("deinterlace_method", property("deinterlace_method").toString().toAscii().constData());
            m_consumer->set("buffer", 1);
            m_consumer->set("scrub_audio", 1);
        }
        m_image_format = mlt_image_rgb24a;
        emit started();
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
void SceneWidget::on_frame_show(mlt_consumer, void* self, mlt_frame frame_ptr)
{
    SceneWidget* widget = static_cast<SceneWidget*>(self);
    Mlt::Frame frame(frame_ptr);
    emit widget->frameReceived(Mlt::QFrame(frame));
}

#include "scenewidget.moc"
