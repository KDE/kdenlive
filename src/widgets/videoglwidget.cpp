/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *               2014 by Jean-Nicolas Artaud (jeannicolasartaud@gmail.com) *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include <QtGui>
#include <QtOpenGL>
#ifdef Q_WS_MAC
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif
#include "widgets/videoglwidget.h"
#include "kdenlivesettings.h"

//#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "GL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); exit(1); } }

extern "C" {
GLAPI GLenum APIENTRY glClientWaitSync (GLsync sync, GLbitfield flags, GLuint64 timeout);
}
#include <mlt++/Mlt.h>

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT GL_TEXTURE_RECTANGLE_NV
#endif

VideoGLWidget::VideoGLWidget(QWidget *parent, QGLWidget *share)
    : QGLWidget(parent, share)
    , sendFrameForAnalysis(false)
    , analyseAudio(false)
    , x(0)
    , y(0)
    , w(width())
    , h(height())
    , m_image_width(0)
    , m_image_height(0)
    , m_texture(0)
    , m_frame_texture(0)
    , m_display_ratio(4.0 / 3.0)
    , m_backgroundColor(KdenliveSettings::window_background())
    , m_fbo(NULL)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

VideoGLWidget::~VideoGLWidget()
{
    makeCurrent();
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    // m_frame will be cleaned up when the profile is closed by Render.
}

QSize VideoGLWidget::minimumSizeHint() const
{
    return QSize(40, 30);
}

QSize VideoGLWidget::sizeHint() const
{
    return QSize(400, 300);
}

void VideoGLWidget::setImageAspectRatio(double ratio)
{
    m_display_ratio = ratio;
    resizeGL(width(), height());
}

void VideoGLWidget::initializeGL()
{
    qglClearColor(m_backgroundColor);
    glShadeModel(GL_FLAT);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    /*glShadeModel(GL_FLAT);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);*/
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void VideoGLWidget::resizeEvent(QResizeEvent* event)
{
    resizeGL(event->size().width(),event->size().height());
}

void VideoGLWidget::resizeGL(int width, int height)
{
    double this_aspect = (double) width / height;

    // Special case optimisation to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if ((int)(this_aspect * 1000) == (int)(m_display_ratio * 1000)) {
        w = width;
        h = height;
    }
    // Use OpenGL to normalise sample aspect ratio
    else if (height * m_display_ratio > width) {
        w = width;
        h = width / m_display_ratio;
    } else {
        w = height * m_display_ratio;
        h = height;
    }
    x = (width - w) / 2;
    y = (height - h) / 2;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, height, 0);
    glMatrixMode(GL_MODELVIEW);
    glClear(GL_COLOR_BUFFER_BIT);
}

void VideoGLWidget::prepareMonitor()
{
    //Make sure we don't inherit a texture from another monitor
    m_frame_texture = 0;
}

/*void VideoGLWidget::paintEvent(QPaintEvent *event)
{

    updateGL(); // This calls for initializeGL and then paintGL and draws a nice openGL 3D scene


    QPainter painter(this);
    //painter.begin();
    painter.drawText(10, 10, "HELLO");
    // Draw something with QPainter..
}*/

void VideoGLWidget::paintGL()
{
    if (m_texture) {
#ifdef Q_WS_MAC
        glClear(GL_COLOR_BUFFER_BIT);
#endif
        glEnable(GL_TEXTURE_RECTANGLE_EXT);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0);
        glVertex2i(x, y);
        glTexCoord2i(m_image_width, 0);
        glVertex2i(x + w, y);
        glTexCoord2i(m_image_width, m_image_height);
        glVertex2i(x + w, y + h);
        glTexCoord2i(0, m_image_height);
        glVertex2i(x, y + h);
        glEnd();
        glDisable(GL_TEXTURE_RECTANGLE_EXT);
    }

    if (!m_frame_texture) return;

    if (!m_overlay.isEmpty() || sendFrameForAnalysis) {
        if (!m_fbo || m_fbo->width() != w || m_fbo->height() != h) {
            delete m_fbo;
            m_fbo = new QGLFramebufferObject(w, h, GL_TEXTURE_2D);
        }
        glPushAttrib(GL_VIEWPORT_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        m_fbo->bind();
        //check_error();
        glViewport( 0, 0, w, h);
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        glOrtho(0.0, w, 0.0, h, -1.0, 1.0);
        m_fbo->drawTexture(QRectF(0, 0, w, h), m_frame_texture);
        emit frameUpdated(m_fbo->toImage());
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        if (!m_overlay.isEmpty()) {
            glEnable(GL_BLEND);
            QPainter p(m_fbo);
            QFont f = font();
            f.setWeight(QFont::Bold);
            QFontInfo ft(f);
            int lineHeight = ft.pixelSize() + 2;
            f.setPixelSize(lineHeight);
            p.setFont(f);
            QRectF rect = p.boundingRect(0, 0, w, lineHeight * 2, Qt::AlignLeft | Qt::AlignHCenter, m_overlay);
            p.setPen(Qt::NoPen);
            QRectF bgrect(-lineHeight / 4, 10, rect.width() + 2.25 * lineHeight, lineHeight * 1.5);
            p.setBrush(QColor(170, 0, 0, 110));
            p.drawRoundedRect(bgrect, lineHeight / 4, lineHeight / 4);
            p.setPen(Qt::white);
            p.drawText(lineHeight, 10 + lineHeight, m_overlay);
            p.end();
            glDisable(GL_BLEND);
        }
        m_fbo->release();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

#ifdef Q_WS_MAC
        glClear(GL_COLOR_BUFFER_BIT);
#endif
        glEnable(GL_TEXTURE_2D);
        if (!m_overlay.isEmpty() || sendFrameForAnalysis) glBindTexture(GL_TEXTURE_2D, m_fbo->texture());
        else glBindTexture(GL_TEXTURE_2D, m_frame_texture);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0);
        glVertex2i(x, y + h);
        glTexCoord2i(1, 0);
        glVertex2i(x + w, y + h);
        glTexCoord2i(1, 1);
        glVertex2i(x + w, y);
        glTexCoord2i(0, 1);
        glVertex2i(x, y);
        glEnd();
    }

    else {
#ifdef Q_WS_MAC
        glClear(GL_COLOR_BUFFER_BIT);
#endif
        glEnable(GL_TEXTURE_2D);
        if (!m_overlay.isEmpty() || sendFrameForAnalysis) glBindTexture(GL_TEXTURE_2D, m_fbo->texture());
        else glBindTexture(GL_TEXTURE_2D, m_frame_texture);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0);
        glVertex2i(x, y);
        glTexCoord2i(1, 0);
        glVertex2i(x + w, y);
        glTexCoord2i(1, 1);
        glVertex2i(x + w, y + h);
        glTexCoord2i(0, 1);
        glVertex2i(x, y + h);
        glEnd();
    }

    if (false) {
            // just for fun, test drawing a "play" triangle on top of video
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

void VideoGLWidget::showImage(const QImage &image)
{
    m_image_width = image.width();
    m_image_height = image.height();
    makeCurrent();
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    //delete m_frame;
    //m_frame = NULL;
    m_frame_texture = 0;

    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_image_width);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, m_image_width, m_image_height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, image.bits());
    updateGL();
}

void VideoGLWidget::showImage(Mlt::Frame* frame, GLuint texnum, const QString overlay)
{
    GLsync sync = (GLsync) frame->get("movit.convert.fence");
    glClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    m_frame_texture = texnum;
    m_overlay = overlay;
    updateGL();
    if (analyseAudio) {
        mlt_audio_format audio_format = mlt_audio_s16;
        //FIXME: should not be hardcoded..
        int freq = 48000;
        int num_channels = 2;
        int samples = 0;
        int16_t* data = (int16_t*)frame->get_audio(audio_format, freq, num_channels, samples);
        if (data && samples > 0) {
            // Data format: [ c00 c10 c01 c11 c02 c12 c03 c13 ... c0{samples-1} c1{samples-1} for 2 channels.
            // So the vector is of size samples*channels.
            QVector<int16_t> sampleVector(samples*num_channels);
            memcpy(sampleVector.data(), data, samples*num_channels*sizeof(int16_t));
            emit audioSamplesSignal(sampleVector, freq, num_channels, samples);
        }
    }
    delete frame;
}

void VideoGLWidget::checkOverlay(const QString &overlay)
{
    if (overlay == m_overlay) return;
    m_overlay = overlay;
    updateGL();
}

void VideoGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    // TODO: disable screensaver?
    Qt::WindowFlags flags = windowFlags();
    if (!isFullScreen()) {
        // Check if we ahave a multiple monitor setup
        int monitors = QApplication::desktop()->screenCount();
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
        show();
#endif
    } else {
        flags ^= (Qt::Window | Qt::SubWindow); //clear the flags...
        flags |= m_baseFlags; //then we reset the flags (window and subwindow)
        setWindowFlags(flags);
        setWindowState(windowState()  ^ Qt::WindowFullScreen);   // reset
        show();
    }
    event->accept();
}


#include "videoglwidget.moc"
