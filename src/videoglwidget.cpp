
#include <QtGui>
#include <QtOpenGL>
#include "videoglwidget.h"

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT GL_TEXTURE_RECTANGLE_NV
#endif

VideoGLWidget::VideoGLWidget(QWidget *parent)
    : QGLWidget(parent)
    , m_image_width(0)
    , m_image_height(0)
    , m_texture(0)
    , m_display_ratio(4.0 / 3.0)
    , m_backgroundColor(Qt::gray)
{
}

VideoGLWidget::~VideoGLWidget()
{
    makeCurrent();
    if (m_texture)
        glDeleteTextures(1, &m_texture);
}

QSize VideoGLWidget::minimumSizeHint() const
{
    return QSize(40, 30);
}

QSize VideoGLWidget::sizeHint() const
{
    return QSize(400, 300);
}

void VideoGLWidget::initializeGL()
{
    qglClearColor(m_backgroundColor);
    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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
}

void VideoGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT); // | GL_DEPTH_BUFFER_BIT // Depth is disabled, so shouldn'b be necessary to clear DEPTH_BUFFER
    if (m_texture) {
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
}

void VideoGLWidget::showImage(QImage image)
{
    m_image_width = image.width();
    m_image_height = image.height();

    makeCurrent();
    if (m_texture)
        glDeleteTextures(1, &m_texture);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_image_width);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, m_image_width, m_image_height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, image.bits());
    updateGL();
}

void VideoGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    // TODO: disable screensaver? or should we leave that responsibility to the
    // application?
    Qt::WindowFlags flags = windowFlags();
    if (!isFullScreen()) {
        //we only update that value if it is not already fullscreen
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
}

