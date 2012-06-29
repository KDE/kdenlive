/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "monitorgraphicsscene.h"
#include "kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <QPainter>
#include <QPaintEngine>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsSceneWheelEvent>
#include <QScrollBar>
#include <QElapsedTimer>
#include <QGLWidget>

#include <GL/glu.h>
#include <GL/glext.h>

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT GL_TEXTURE_RECTANGLE_NV
#endif


#define USE_PBO

MonitorGraphicsScene::MonitorGraphicsScene(QObject* parent) :
    QGraphicsScene(parent),
    m_frame(NULL),
    m_imageSizeChanged(true),
    m_textureBuffer(0),
    m_texture(0),
    m_zoom(1),
    m_needsResize(true),
    m_pbo(0)
{
    m_imageRect = addRect(QRectF(0, 0, 0, 0));

//     m_frameCount = 0;
//     m_timer.start();
}

MonitorGraphicsScene::~MonitorGraphicsScene()
{
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
    }
    if (m_textureBuffer) {
        delete[] m_textureBuffer;
    }
    if (m_pbo) {
        glDeleteBuffers(1, &m_pbo);
    }
}

void MonitorGraphicsScene::initializeGL(QGLWidget* glWidget)
{
    glWidget->makeCurrent();

    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
}

void MonitorGraphicsScene::setFramePointer(AtomicFramePointer* frame)
{
    m_frame = frame;
}

void MonitorGraphicsScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect)

    if (KDE_ISUNLIKELY(painter->paintEngine()->type() != QPaintEngine::OpenGL && painter->paintEngine()->type() != QPaintEngine::OpenGL2)) {
        kWarning() << "not used with a OpenGL viewport";
        return;
    }

    glClearColor(KdenliveSettings::window_background().redF(),
                 KdenliveSettings::window_background().greenF(),
                 KdenliveSettings::window_background().blueF(),
                 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (KDE_ISUNLIKELY(m_needsResize)) {
        int width = views()[0]->viewport()->width();
        int height = views()[0]->viewport()->height();

        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0, width, height, 0);
        glMatrixMode(GL_MODELVIEW);
        m_needsResize = false;
    }

    Mlt::Frame *frame = 0;
    if (m_frame) {
        frame = m_frame->fetchAndStoreOrdered(0);
    }

    if(KDE_ISUNLIKELY(!m_texture && !frame)) {
        return;
    }

    if (frame) {
        int width = 0;
        int height = 0;

        mlt_image_format format = mlt_image_rgb24a;
        const uint8_t *image = frame->get_image(format, width, height);
        int size = width * height * 4;

        if (KDE_ISUNLIKELY(size != m_imageRect->rect().width() * m_imageRect->rect().height() * 4)) {
            // The frame dimensions have changed. We need to update the gl buffer sizes.

            m_imageRect->setRect(0, 0, width, height);

#ifdef USE_PBO
            if (m_textureBuffer) {
                delete[] m_textureBuffer;
                m_textureBuffer = 0;
            }
            if (m_texture) {
                glDeleteTextures(1, &m_texture);
            }
            if (m_pbo) {
                glDeleteBuffers(1, &m_pbo);
            }

            glGenBuffers(1, &m_pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_STREAM_DRAW);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            glGenTextures(1, &m_texture);
            glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
            glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)m_textureBuffer);
            
#else

            if (m_texture) {
                glDeleteTextures(1, &m_texture);
            }
            glGenTextures(1, &m_texture);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
            glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
            glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#endif
        }

#ifdef USE_PBO
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);

        // map the buffer object into client's memory
        // Note that glMapBufferARB() causes sync issue.
        // If GPU is working with this buffer, glMapBufferARB() will wait(stall)
        // for GPU to finish its job. To avoid waiting (stall), you can call
        // first glBufferDataARB() with NULL pointer before glMapBufferARB().
        // If you do that, the previous data in PBO will be discarded and
        // glMapBufferARB() returns a new allocated pointer immediately
        // even if GPU is still working with the previous data.
        glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_STREAM_DRAW);
        void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        memcpy(ptr, image, size);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
        glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#else
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
        glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image);
#endif

        delete frame;
    }

    QRectF imageRect = m_imageRect->rect();

    QPolygon viewRect = views()[0]->mapFromScene(imageRect);
    int left = viewRect.point(0).x();
    int right = viewRect.point(1).x();
    int top = viewRect.point(0).y();
    int bottom = viewRect.point(3).y();
    GLint texCoords[] = {0, 0,
                         imageRect.width(), 0,
                         imageRect.width(), imageRect.height(),
                         0, imageRect.height() };
    GLint vertices[] = {left, top,
                        right, top,
                        right, bottom,
                        left, bottom };

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
    glEnable(GL_TEXTURE_RECTANGLE_EXT);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);

    glTexCoordPointer(2, GL_INT, 0, texCoords);
    glVertexPointer(2, GL_INT, 0, vertices);

    glDrawArrays(GL_QUADS, 0, 4);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glDisable(GL_TEXTURE_RECTANGLE_EXT);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);

//     ++m_frameCount;
//     if (m_timer.elapsed() / 1000 >= 1) {
//         kDebug() << "fps" << m_frameCount;
//         m_frameCount = 0;
//         m_timer.restart();
//     }
}

void MonitorGraphicsScene::setZoom(qreal level)
{
    if(views()[0]) {
        m_zoom = level;
        views()[0]->resetTransform();
        views()[0]->scale(m_zoom, m_zoom);
        emit zoomChanged(level);
    }
}

qreal MonitorGraphicsScene::zoom() const
{
    return m_zoom;
}

void MonitorGraphicsScene::zoomFit()
{
    if (views()[0]) {
        qreal levelX = views()[0]->viewport()->height() / m_imageRect->rect().height();
        qreal levelY = views()[0]->viewport()->width() / m_imageRect->rect().width();
        setZoom(qMin(levelX, levelY));
        views()[0]->centerOn(m_imageRect);
    }
}

void MonitorGraphicsScene::zoomIn(qreal by)
{
    setZoom(qMin(3., m_zoom + by));
}

void MonitorGraphicsScene::zoomOut(qreal by)
{
    setZoom(qMax(0.05, m_zoom - by));
}

void MonitorGraphicsScene::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    QGraphicsView *view = views()[0];
    if (event->modifiers() == Qt::ControlModifier) {
        if (event->delta() > 0) {
            view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            zoomIn();
            view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QAbstractSlider::SliderAction action;
        if (event->delta() > 0)
            action = QAbstractSlider::SliderSingleStepSub;
        else
            action = QAbstractSlider::SliderSingleStepAdd;
        if (event->orientation() == Qt::Horizontal)
            views()[0]->horizontalScrollBar()->triggerAction(action);
        else
            views()[0]->verticalScrollBar()->triggerAction(action);
    }
}

bool MonitorGraphicsScene::eventFilter(QObject* object, QEvent* event)
{
    if (views().count() && object == views()[0]) {
        if (event->type() == QEvent::Resize) {
            m_needsResize = true;
        }
        return false;
    } else {
        return QGraphicsScene::eventFilter(object, event);
    }
}
