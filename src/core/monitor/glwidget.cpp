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
#include "glwidget.h"
#include "project/producerwrapper.h"
#include "kdenlivesettings.h"
#include <QGLPixelBuffer>
#include <QDesktopWidget>

#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "GL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); exit(1); } }


GLWidget::GLWidget(Mlt::Profile *profile, QWidget *parent)
    : QGLWidget(parent)
    , MltController(profile)
    , showFrameSemaphore(3)
    , m_image_width(0)
    , m_image_height(0)
    , m_shader(0)
    , m_fbo(0)
    , m_isInitialized(false)
    , m_image_format(mlt_image_yuv422)
    , m_showPlay(false)
{
    m_display_ratio = 4.0/3.0;
    m_texture[0] = m_texture[1] = m_texture[2] = 0;
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    QSettings settings;
    m_isActive = false;
    m_type = MLTOPENGL;
    activate();
}

void GLWidget::activate()
{
    m_isActive = true;
}

GLWidget::~GLWidget()
{
    makeCurrent();
    if (m_texture[0]) glDeleteTextures(3, m_texture);
    delete m_fbo;
}


void GLWidget::closeConsumer()
{
    MltController::closeConsumer();
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(40, 30);
}

QSize GLWidget::sizeHint() const
{
    return QSize(400, 300);
}

void GLWidget::initializeGL()
{
    QPalette palette;
    glewInit();
    glClearColor(KdenliveSettings::window_background().redF(),
                 KdenliveSettings::window_background().greenF(),
                 KdenliveSettings::window_background().blueF(),
                 1.0f);
    //qglClearColor(palette.color(QPalette::Window));
    glShadeModel(GL_FLAT);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //QFont m_big = font();
    //makeCurrent();
    /*m_big.setPixelSize(80);
    m_fbo2 = new QGLFramebufferObject(500, 150);
        m_fbo2->bind();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        QPainter painter;
        painter.begin(m_fbo2);
        painter.setFont(m_big);
        painter.setPen(QColor(255, 100, 100, 150));
        painter.drawText(QRect(0, 0, 500, 150), "Test Text");
        painter.end();
        m_fbo2->release();*/

    
    /*QImage img("/home/two/Pictures/frame.png");
    QImage t = QGLWidget::convertToGLFormat(img);
    
    glGenTextures( 1, &cubeTexture[0] );
    glBindTexture( GL_TEXTURE_2D, cubeTexture[0] );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits() );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );*/
    
    m_condition.wakeAll();
    m_isInitialized = true;
}


void GLWidget::resizeGL(int width, int height)
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
    if (isValid()) {
        makeCurrent();
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}


void GLWidget::resizeEvent(QResizeEvent* event)
{
    setUpdatesEnabled(true);
    resizeGL(event->size().width(), event->size().height());
    QGLWidget::resizeEvent(event);
    event->accept();
}

void GLWidget::showEvent(QShowEvent* event)
{
    event->accept();
    initializeGL();
    resizeGL(width(), height());
}

//void GLWidget::paintGL()
void GLWidget::paintEvent(QPaintEvent *event)
{
    makeCurrent();
    if (m_texture[0]) {
        QPainter p;
        p.begin(this);
        p.setClipRect(event->rect());
        p.beginNativePainting();
        m_shader->bind();
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0); glVertex2i(x, y);
        glTexCoord2i(1, 0); glVertex2i(x + w, y);
        glTexCoord2i(1, 1); glVertex2i(x + w, y + h);
        glTexCoord2i(0, 1); glVertex2i(x, y + h);
        glEnd();
        p.endNativePainting();

        if (!m_overlayText.isEmpty()) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH,   0);
            glPixelStorei(GL_UNPACK_ALIGNMENT,    4);
            QFontMetrics f(font());
            QRectF textRect = p.boundingRect(x, y , w - 15, h - 10, Qt::AlignRight | Qt::AlignBottom, m_overlayText);
            QRectF bgRect = textRect.adjusted(-(5 + textRect.height()) , -2, 5, 2);
            QColor c(Qt::white);
            c.setAlpha(100);
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            m_overlayZone = bgRect;
            p.drawRoundedRect(bgRect, 3, 3);
            bgRect.setWidth(textRect.height());
            p.setBrush(m_overlayColor);
            p.drawEllipse(bgRect.adjusted(2, 2, 0, -2));
            p.setPen(Qt::black);
            p.drawText(textRect, m_overlayText);
            glPixelStorei  (GL_UNPACK_ROW_LENGTH, m_image_width);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glBindTexture  (GL_TEXTURE_2D, m_texture[1]);
        }
        p.end();
    }
    //doneCurrent();
    /*glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();*/

    //p.endNativePainting();
    //p.fillRect(5, 5, 100, 20, QColor(200, 100, 100, 100));
    /*p.setRenderHints(QPainter::TextAntialiasing, false);
    QFont ft(font());
    ft.setStyleHint(QFont::Monospace, QFont::NoAntialias);
    p.setFont(ft);*/
    //p.drawText(10, 10, "Hello TOTO");
    //p.fillRect(105, 5, 100, 20, QColor(200, 100, 100, 100));
    //p.end();
    /*glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0f);
    glDisable(GL_BLEND);
    swapBuffers();
    glFlush();*/
    return;
    

    /*glMatrixMode(GL_PROJECTION);
glPushMatrix();
glLoadIdentity();
glOrtho(0.0, width(), 0.0, height(), -1.0, 1.0);
glMatrixMode(GL_MODELVIEW);
glPushMatrix();


glLoadIdentity();
glDisable(GL_LIGHTING);*/
    
    
    /*glDisable(GL_DEPTH_TEST);
    glClearColor(0.0,0.0,0.0,0.0);*/
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    QFont m_big = font();
    m_big.setPixelSize(80);
    if (m_fbo) delete (m_fbo);
    QGLFramebufferObjectFormat fmt;
    //fmt.setSamples(4); // or 4 or disable this line
    fmt.setInternalTextureFormat(GL_RGBA32I);
    m_fbo2 = new QGLFramebufferObject(500, 150);
    
    m_fbo2->bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    QPainter painter;
    painter.begin(m_fbo2);
    painter.setFont(m_big);
    painter.setPen(QColor(255, 100, 100, 150));
    painter.drawText(QRect(0, 0, 500, 150), "Test Text");
    painter.end();
    m_fbo2->release();


    glBindTexture(GL_TEXTURE_2D, m_fbo2->texture());
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    /*glTexCoord2i(1, 0); glVertex2i(x + w, y);
            glTexCoord2i(1, 1); glVertex2i(x + w, y + h);
            glTexCoord2i(0, 1); glVertex2i(x, y + h);*/

    glTexCoord2i(1, 0); glVertex2i(x + w, y);
    glTexCoord2i(1, 1); glVertex2i(x + w, y + h);
    glTexCoord2i(0, 1); glVertex2i(0, y + h);
    glTexCoord2i(0, 0); glVertex2i(0, y);
    glEnd();
    //drawTexture(QPointF(20.0, 20.0), m_fbo2->texture());
    //p.endNativePainting();
    //glClearColor(0, 0, 0, 0);
    //glClearDepth(1.0f);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //p.drawText(20, 20, "Hello Guys!");
    //p.end();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    return;
    //int textureID;
    //m_shader->release();


    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    //glBlendEquation(GL_FUNC_ADD);
    //glBlendFunc( GL_ONE, GL_ONE);
    /*QImage img("/home/two/Pictures/frame.png");
QImage newimg = QGLWidget::convertToGLFormat(img);
if (cubeTexture[0])
glDeleteTextures(1, cubeTexture);
glGenTextures(1, cubeTexture);*/
    //glBindTexture(GL_TEXTURE_2D, cubeTexture[0]);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    /*glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, newimg.width(), newimg.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, newimg.bits());
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);*/
    //glPushMatrix();
    //glBegin(GL_QUADS);   // in theory triangles are better

    /*glTexCoord2i(0,0); glVertex2i(0,0);
glTexCoord2i(1,0); glVertex2i(width(), 0);
glTexCoord2i(1,1); glVertex2i(width(), height());
glTexCoord2i(0,1); glVertex2i(0,height());*/

    /*glTexCoord2i(0, 0); glVertex2i(x, y);
            glTexCoord2i(1, 0); glVertex2i(x + w, y);
            glTexCoord2i(1, 1); glVertex2i(x + w, y + h);
            glTexCoord2i(0, 1); glVertex2i(x, y + h);
*/
    //glEnd();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    return;
    //QPainter p(this);
    //    p.endNativePainting();
    //p.end();
    return;
    
    //return;
    if (!m_overlayText.isEmpty()) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0,width(),0,height());
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);
        //glBlendFunc( GL_ONE, GL_ONE);
        /*QFontMetrics fm(font());
    QRect r = fm.boundingRect(m_overlay);
    p.fillRect(5, 5, r.width() + 8, r.height() + 4, QColor(255, 150, 100, 140));*/
        QImage img("/home/two/Pictures/frame.png");
        QImage newimg = QGLWidget::convertToGLFormat(img);
        //cubeTexture = bindTexture(newimg);
        //glBindTexture(GL_TEXTURE_2D, cubeTexture);
        /*drawTexture(QRectF(-frame_resolution.width()/2,
          -frame_resolution.height()/2,
          frame_resolution.width(),
          frame_resolution.height()),
      texture);*/
        //QImage newimg = QGLWidget::convertToGLFormat(img);


        /*glBindTexture(GL_TEXTURE_2D,texture[0]);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glBindTexture(GL_TEXTURE_2D,texture[0]);*/
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits() );
        //glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0 , newimg.width(), newimg.height(),  GL_RGBA, GL_UNSIGNED_BYTE, newimg.bits() );
        glBegin(GL_QUADS);   // in theory triangles are better
        glTexCoord2i(0,0); glVertex2i(0,height() / 2);
        glTexCoord2i(0,1); glVertex2i(0,0);
        glTexCoord2i(1,1); glVertex2i(width() / 2,0);
        glTexCoord2i(1,0); glVertex2i(width() / 2,height() / 2);


        /*glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, newimg.width(), newimg.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, newimg.bits() );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glPushMatrix();
    glBegin(GL_QUADS);
            glTexCoord2i(0, 0); glVertex2i(x, y);
            glTexCoord2i(1, 0); glVertex2i(x + w, y);
            glTexCoord2i(1, 1); glVertex2i(x + w, y + h);
            glTexCoord2i(0, 1); glVertex2i(x, y + h);*/
        glEnd();
        glDisable(GL_BLEND);
        //glPopMatrix();
        //p.drawImage(0, 0, newimg);
        //p.setPen(Qt::white);
        //p.drawText(r, m_overlay);

    }
    
    //p.end();
    //glFinish();
    //swapBuffers();
    
    return;
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
        glDisable(GL_BLEND);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

void GLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    //QGLWidget::mouseReleaseEvent(event);
    m_showPlay = !m_showPlay;
    switchPlay();
    emit stateChanged();
    event->accept();
}

void GLWidget::wheelEvent(QWheelEvent* event)
{
    int factor = 1;
    if (event->modifiers() & Qt::ControlModifier) factor = (int) profile().fps();
    if (event->delta() > 0) nextFrame(factor);
    else previousFrame(factor);
    event->accept();
}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    //QGLWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton)
        m_dragStart = event->pos();
    emit dragStarted();
    event->accept();
}

void GLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    switchFullScreen();
    event->accept();
}

void GLWidget::enterEvent( QEvent * event )
{
    QGLWidget::enterEvent(event);
    if (m_producer && m_producer->get_speed() == 0) {
        m_showPlay = true;
    }
    else m_showPlay = false;
}

void GLWidget::slotSetZone(const QPoint &zone)
{
    if (m_zone != zone) {
        m_zone = zone;
        if (m_producer->get_speed() == 0)
            refreshConsumer();
    }
}

void GLWidget::slotSetMarks(const QMap <int,QString> &marks)
{
    if (m_markers != marks) {
        m_markers = marks;
        if (m_producer->get_speed() == 0)
            refreshConsumer();
    }
}

void GLWidget::switchFullScreen()
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

void GLWidget::leaveEvent( QEvent * event )
{
    QGLWidget::leaveEvent(event);
    if (m_showPlay) {
        m_showPlay = false;
    }
    /*if (m_producer->get_speed() == 0) {
    doneCurrent();
    }*/
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->modifiers() == Qt::ShiftModifier && m_producer) {
        emit seekTo(m_producer->get_length() * event->x() / width());
        return;
    }
    if (event->buttons() == Qt::NoButton) {
        if (m_overlayZone.contains(event->pos())) setCursor(Qt::PointingHandCursor);
        else setCursor(Qt::ArrowCursor);
    }
    //TODO: handle drag
    return;
    if ((event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance())
        return;
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/mlt+xml", "");
    drag->setMimeData(mimeData);
    drag->exec(Qt::LinkAction);
}

void GLWidget::createShader()
{
    if (!m_shader) {
        makeCurrent();
        m_shader = new QGLShaderProgram(this);
        m_shader->addShaderFromSourceCode(QGLShader::Fragment,
                                          "uniform sampler2D Ytex, Utex, Vtex;"
                                          "void main(void) {"
                                          "  float r, g, b;"
                                          "  vec4 txl, ux, vx;"
                                          "  float nx = gl_TexCoord[0].x;"
                                          "  float ny = gl_TexCoord[0].y;"
                                          "  float y = texture2D(Ytex, vec2(nx, ny)).r;"
                                          "  float u = texture2D(Utex, vec2(nx, ny)).r;"
                                          "  float v = texture2D(Vtex, vec2(nx, ny)).r;"

                                          "  y = 1.1643 * (y - 0.0625);"
                                          "  u = u - 0.5;"
                                          "  v = v - 0.5;"

                                          "  r = y + 1.5958  * v;"
                                          "  g = y - 0.39173 * u - 0.81290 * v;"
                                          "  b = y + 2.017   * u;"

                                          "  gl_FragColor = vec4(r, g, b, 1.0);"
                                          "}");
        m_shader->bind();
        doneCurrent();
    }
}

void GLWidget::destroyShader()
{
    if (m_shader) {
        makeCurrent();
        m_shader->release();
        delete m_shader;
        m_shader = 0;
        doneCurrent();
    }
}


void GLWidget::refreshConsumer()
{
    MltController::refreshConsumer();
}

void GLWidget::showFrame(Mlt::QFrame frame)
{
    if (frame.frame()->get_int("rendered")) {
        m_image_width = 0;
        m_image_height = 0;
        //makeCurrent();
        int position = frame.frame()->get_position();
        if (m_markers.contains(-position)) {
            // Zone in or out
            m_overlayText = m_markers.value(-position);
            m_overlayColor = QColor(255, 90, 90);
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
        
        const uint8_t* image = frame.frame()->get_image(m_image_format, m_image_width, m_image_height);
        /*glPushAttrib(GL_ALL_ATTRIB_BITS);
glMatrixMode(GL_PROJECTION);
glPushMatrix();
glMatrixMode(GL_MODELVIEW);
glPushMatrix();*/
        // Copy each plane of YUV to a texture bound to shader program˙.
        makeCurrent();
        if (m_texture[0])
            glDeleteTextures(3, m_texture);
        //m_shader->bind();
        glPixelStorei  (GL_UNPACK_ROW_LENGTH, m_image_width);
        glGenTextures  (3, m_texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture  (GL_TEXTURE_2D, m_texture[0]);
        m_shader->setUniformValue(m_shader->uniformLocation("Ytex"), 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D   (GL_TEXTURE_2D, 0, GL_LUMINANCE, m_image_width, m_image_height, 0,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, image);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture  (GL_TEXTURE_2D, m_texture[1]);
        m_shader->setUniformValue(m_shader->uniformLocation("Utex"), 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D   (GL_TEXTURE_2D, 0, GL_LUMINANCE, m_image_width/2, m_image_height/4, 0,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, image + m_image_width * m_image_height);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture  (GL_TEXTURE_2D, m_texture[2]);
        m_shader->setUniformValue(m_shader->uniformLocation("Vtex"), 2);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D   (GL_TEXTURE_2D, 0, GL_LUMINANCE, m_image_width/2, m_image_height/4, 0,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, image + m_image_width * m_image_height + m_image_width/2 * m_image_height/2);
        //m_shader->release();
        //glDraw();
        doneCurrent();
        update();
    }
    showFrameSemaphore.release();
}

void GLWidget::renderImage(const QString &id, ProducerWrapper *producer, QList <int> positions, int width, int height)
{
    setUpdatesEnabled(false);

    // Position might be negative to indicate the in point on the imageRendered signal.
    if (width == -1) {
        height = 100;
        width = height * profile().dar();
    }
    int current = producer->position();
    while (!positions.isEmpty()) {
        int position = positions.takeFirst();
        producer->seek(qAbs(position));
        Mlt::Frame* frame = producer->get_frame();
        QImage image = MltController::image(frame, width, height);
        emit imageRendered(id, position, image);
        delete frame;
    }
    producer->seek(current);
    setUpdatesEnabled(true);
}

int GLWidget::reOpen(bool isMulti)
{
    //QMutexLocker lock(&m_mutex);
    activate();
    return 0;
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

void GLWidget::slotGetThumb(ProducerWrapper *producer, int position)
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

int GLWidget::open(ProducerWrapper* producer, bool isMulti, bool isLive)
{
    //QMutexLocker lock(&m_mutex);
    //setFixedSize(producer->get_profile()->height * producer->get_profile()->display_aspect_num / producer->get_profile()->display_aspect_den / 2.0, producer->get_profile()->height / 2.0);
    int error = MltController::open(producer, isMulti, isLive);//new ProducerWrapper(profile(), producer->property("resource")), isMulti); //producer, isMulti);
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
    //emit producerChanged();
    return error;
}

int GLWidget::reconfigure(bool isMulti)
{
    int error = 0;
    // use SDL for audio, OpenGL for video
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
        // Warning: Only rtaudio allows simultaneous consumers, SDL freezes.
        
        if (isMulti)
            m_consumer = new Mlt::FilteredConsumer(profile(), "multi");
        else {
            m_consumer = new Mlt::FilteredConsumer(profile(), serviceName.toAscii().constData());
        }

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

            /*if (!profile().progressive())
                m_consumer->set("progressive", property("progressive").toBool());
            m_consumer->set("rescale", property("rescale").toString().toAscii().constData());
            m_consumer->set("deinterlace_method", property("deinterlace_method").toString().toAscii().constData());*/
            m_consumer->set("buffer", 25);
            m_consumer->set("prefill", 1);
            m_consumer->set("terminate_on_pause", 1);
            m_consumer->set("scrub_audio", 1);
        }
        m_image_format = mlt_image_yuv420p;
        createShader();
        emit started();
        if (m_isLive) {
            //switchPlay();
            emit stateChanged();
        }
        else m_producer->set_speed(0.0);
        m_consumer->start();
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
void GLWidget::on_frame_show(mlt_consumer, void* self, mlt_frame frame_ptr)
{
    GLWidget* widget = static_cast<GLWidget*>(self);
    if (widget->showFrameSemaphore.tryAcquire()) {
        Mlt::Frame frame(frame_ptr);
        emit widget->frameReceived(Mlt::QFrame(frame));
    }
}

#include "glwidget.moc"
