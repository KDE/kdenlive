/*
 * Copyright (c) 2011 Meltytech, LLC
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

#include "sdlwidget.h"
#include "kdenlivesettings.h"
#include "project/producerwrapper.h"
#include <mlt++/Mlt.h>
#include <QtGui>
#include <QWidget>
#include <QDesktopWidget>


VideoContainer::VideoContainer(QWidget *parent) :
    QFrame(parent)
    //, m_monitor(monitor)
{
    setFrameShape(QFrame::NoFrame);
    //setFocusPolicy(Qt::ClickFocus);
    //setEnabled(false);
    setMouseTracking(true);
    setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

// virtual
void VideoContainer::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() != Qt::RightButton) {
	emit switchPlay();
	kDebug()<<"/ / / //  /AAAARGH / // / / / / /";
        /*if (m_monitor->isActive()) {
            m_monitor->slotPlay();
        }*/
    }
}

// virtual
void VideoContainer::keyPressEvent(QKeyEvent *event)
{
    // Exit fullscreen with Esc key
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        switchFullScreen();
        event->setAccepted(true);
    } else event->setAccepted(false);
}

// virtual
void VideoContainer::wheelEvent(QWheelEvent * event)
{
    int factor = 1;
    if (event->modifiers() & Qt::ControlModifier) factor = 25;
    if (event->delta() > 0) emit wheel(factor);
    else emit wheel(-factor);
    event->accept();
}

void VideoContainer::mouseDoubleClickEvent(QMouseEvent * event)
{
    switchFullScreen();
    event->accept();
}

void VideoContainer::mouseMoveEvent(QMouseEvent* event)
{
    if (event->modifiers() == Qt::ShiftModifier) {
        emit seekTo((double) event->x() / width());
	event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void VideoContainer::switchFullScreen()
{
    // TODO: disable screensaver?
    Qt::WindowFlags flags = windowFlags();
    if (!isFullScreen()) {
        // Check if we ahave a multiple monitor setup
        setUpdatesEnabled(false);
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
        setUpdatesEnabled(true);
        show();
#endif
        setEnabled(true);
    } else {
        setUpdatesEnabled(false);
        flags ^= (Qt::Window | Qt::SubWindow); //clear the flags...
        flags |= m_baseFlags; //then we reset the flags (window and subwindow)
        setWindowFlags(flags);
        setWindowState(windowState()  ^ Qt::WindowFullScreen);   // reset
        setUpdatesEnabled(true);
        //setEnabled(false);
        show();
    }
}

VideoSurface::VideoSurface(QWidget* parent) :
    QWidget(parent)
{
    // MonitorRefresh is used as container for the SDL display (it's window id is passed to SDL)
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    //setUpdatesEnabled(false);
}

void VideoSurface::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    //WARNING: This might trigger unnecessary refreshes from MLT's producer, but without this,
    // as soon as monitor is covered by a popup menu or another window, image is corrupted.
    emit refreshMonitor();
}

SDLWidget::SDLWidget(Mlt::Profile *profile, QWidget *parent)
    : QWidget(parent)
    , MltController(profile)
    , m_videoSurface(NULL)
{
    //setAttribute(Qt::WA_NativeWindow);
    m_videoBox = new VideoContainer(this);
    connect(m_videoBox, SIGNAL(switchPlay()), this, SIGNAL(requestPlayPause()));
    connect(m_videoBox, SIGNAL(seekTo(double)), this, SLOT(slotSeek(double)));
    connect(m_videoBox, SIGNAL(wheel(int)), this, SLOT(slotWheel(int)));
    QVBoxLayout *lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_videoBox);
    setLayout(lay);
    // Required for SDL embeddding
    //setAttribute(Qt::WA_NativeWindow);
    /*setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setUpdatesEnabled(false);*/
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_isActive = true;
    m_type = MLTSDL;
}

void SDLWidget::slotSeek(double ratio)
{
    if (m_producer) emit seekTo((int) (ratio * m_producer->get_length()));
}

void SDLWidget::slotWheel(int factor)
{
    if (!m_producer) return;
    if (factor > 0) nextFrame(factor);
    else previousFrame(-factor);
}

int SDLWidget::open(ProducerWrapper* producer, bool isMulti, bool isLive)
{
    show();
    int error = MltController::open(producer, isMulti, isLive);
    if (!error)
        error = reconfigure(isMulti);
    m_producer->set_speed(0);
    //m_consumer->start();
    //refreshConsumer();
    emit producerChanged();
    return error;
}

void SDLWidget::reStart2()
{
    //Mlt::Event *event = m_consumer->setup_wait_for("consumer-sdl-paused");
    if (!m_consumer->is_stopped()) {
	m_consumer->stop();
	//m_consumer->wait_for(event);
	m_consumer->purge();
    }
    //delete event;
    m_consumer->start();
    refreshConsumer();
}

void SDLWidget::reStart()
{
    QTimer::singleShot(100, this, SLOT(reStart2()));
}

void SDLWidget::seek(int position)
{
    if (m_consumer->is_stopped()) m_consumer->start();
    MltController::seek(position);
}

void SDLWidget::pause()
{
    if (!m_consumer->is_stopped()) 
	m_consumer->stop();
    MltController::pause();
}

void SDLWidget::slotGetThumb(ProducerWrapper *producer, int position)
{
    //setUpdatesEnabled(false);
    if (!m_producer || m_producer->get_speed() != 0) {
	// No thumbnail when playing
	emit gotThumb(position, QImage());
	return;
    }

    QImage result = MltController::thumb(producer, position);
    emit gotThumb(position, result);
    //setUpdatesEnabled(true);
}

void SDLWidget::createVideoSurface()
{
    QVBoxLayout *lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    m_videoSurface = new VideoSurface;
    lay->addWidget(m_videoSurface);
    m_videoBox->setLayout(lay);
}

int SDLWidget::reconfigure(bool isMulti)
{
    int error = 0;
    QString serviceName = "sdl_preview";
    //QString serviceName = property("mlt_service").toString();
    if (!m_consumer || !m_consumer->is_valid()) {
	if (!m_videoSurface) createVideoSurface();
        if (serviceName.isEmpty())
            serviceName = "sdl_preview";
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
        // Make an event handler for when a frame's image should be displayed
        m_consumer->listen("consumer-frame-show", this, (mlt_listener) on_frame_show);
        //m_consumer->set("real_time", property("realtime").toBool()? 1 : -1);

        if (isMulti) {
            m_consumer->set("terminate_on_pause", 0);
            // Embed the SDL window in our GUI.
            m_consumer->set("0.window_id", (int) m_videoSurface->winId());
            // Set the background color
            m_consumer->set("0.window_background", palette().color(QPalette::Window).name().toAscii().constData());
            if (!profile().progressive())
                m_consumer->set("progressive", property("progressive").toBool());
            m_consumer->set("0.rescale", property("rescale").toString().toAscii().constData());
            m_consumer->set("0.deinterlace_method", property("deinterlace_method").toString().toAscii().constData());
            m_consumer->set("0.scrub_audio", 1);
        }
        else {
            // Embed the SDL window in our GUI.
	  //kDebug()<<"// SURFACE ID: "<<this->winId();
            m_consumer->set("window_id", (int) m_videoSurface->winId());
            // Set the background color
            m_consumer->set("window_background", KdenliveSettings::window_background().name().toAscii().constData());
            //if (!profile().progressive())
            //    m_consumer->set("progressive", property("progressive").toBool());
            //m_consumer->set("rescale", property("rescale").toString().toAscii().constData());
            //m_consumer->set("deinterlace_method", property("deinterlace_method").toString().toAscii().constData());
	    m_consumer->set("progressive", 1);
	    m_consumer->set("resize", 1);
	    m_consumer->set("terminate_on_pause", 1);
	    m_consumer->set("rescale", "nearest");
            m_consumer->set("scrub_audio", 1);
        }
        // Connect the producer to the consumer - tell it to "run" later
        m_consumer->connect(*m_producer);
    }
    else {
        // Cleanup on error
        error = 2;
        MltController::closeConsumer();
        MltController::close();
    }
    return error;
}

void SDLWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_dragStart = event->pos();
    emit dragStarted();
}

void SDLWidget::mouseMoveEvent(QMouseEvent* event)
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

// MLT consumer-frame-show event handler
void SDLWidget::on_frame_show(mlt_consumer, void* self, mlt_frame frame_ptr)
{
    SDLWidget* widget = static_cast<SDLWidget*>(self);
    Mlt::Frame frame(frame_ptr);
    emit widget->frameReceived(Mlt::QFrame(frame));
}

void SDLWidget::renderImage(const QString &id, ProducerWrapper *producer, QList <int> positions, int width, int height)
{
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
}
