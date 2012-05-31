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
#include <qpaintengine.h>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsSceneWheelEvent>
#include <QScrollBar>
#include <QElapsedTimer>
#include <QGLWidget>
#include <GL/glu.h>

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT GL_TEXTURE_RECTANGLE_NV
#endif


MonitorGraphicsScene::MonitorGraphicsScene(QObject* parent) :
    QGraphicsScene(parent),
    m_image(0),
    m_imageSize(0),
    m_hasNewImage(false),
    m_texture(0),
    m_zoom(1),
    m_needsResize(true)
{
    m_imageRect = addRect(QRectF(0, 0, 1920, 1080));
}

MonitorGraphicsScene::~MonitorGraphicsScene()
{
    if (m_image) {
        free(m_image);
    }
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
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
}

void MonitorGraphicsScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    QElapsedTimer timer;
    if (painter->paintEngine()->type() != QPaintEngine::OpenGL && painter->paintEngine()->type() != QPaintEngine::OpenGL2) {
        kWarning() << "not used with a OpenGL viewport";
        return;
    }

    glClearColor(KdenliveSettings::window_background().redF(),
                 KdenliveSettings::window_background().greenF(),
                 KdenliveSettings::window_background().blueF(),
                 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if(!m_texture && !m_image) {
        return;
    }

    if (m_needsResize) {
        int width = views()[0]->viewport()->width();
        int height = views()[0]->viewport()->height();

        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0, width, height, 0);
        glMatrixMode(GL_MODELVIEW);
        m_needsResize = false;
    }

    m_imageMutex.lock();
    QRectF imageRect = m_imageRect->rect();

    if (m_hasNewImage) {
        m_hasNewImage = false;
        if (m_texture) {
            glDeleteTextures(1, &m_texture);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, imageRect.width());
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_texture);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, imageRect.width(), imageRect.height(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, m_image);

    }
    m_imageMutex.unlock();

    QPolygon viewRect = views()[0]->mapFromScene(imageRect);

    glEnable(GL_TEXTURE_RECTANGLE_EXT);
//     timer.start();
    glBegin(GL_QUADS);
//     kDebug() << "drawBg:" << timer.elapsed();
    glTexCoord2i(0, 0);
    glVertex2i(viewRect.point(0).x(), viewRect.point(0).y());
    glTexCoord2i(imageRect.width(), 0);
    glVertex2i(viewRect.point(1).x(), viewRect.point(1).y());
    glTexCoord2i(imageRect.width(), imageRect.height());
    glVertex2i(viewRect.point(2).x(), viewRect.point(2).y());
    glTexCoord2i(0, imageRect.height());
    glVertex2i(viewRect.point(3).x(), viewRect.point(3).y());
    glEnd();
    glDisable(GL_TEXTURE_RECTANGLE_EXT);
}

void MonitorGraphicsScene::setImage(Mlt::Frame& frame)
{
    int width = 0;
    int height = 0;

    mlt_image_format format = mlt_image_rgb24a;
    const uint8_t *image = frame.get_image(format, width, height);
    int size = width * height * 4;

    m_imageMutex.lock();
    m_imageRect->setRect(0, 0, width, height);

    if (m_imageSize != size) {
        m_imageSize = size;
        if (m_image) {
            free(m_image);
            m_image = 0;
        }
        m_image = static_cast<uint8_t*>(malloc(size));
    }
    memcpy(m_image, image, size);
    m_hasNewImage = true;

    m_imageMutex.unlock();

    update();
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

//     QGraphicsScene::wheelEvent(event);
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

