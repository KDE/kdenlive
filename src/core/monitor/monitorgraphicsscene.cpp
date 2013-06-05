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


MyTextItem::MyTextItem(QGraphicsItem * parent): QGraphicsSimpleTextItem(parent)
{
    setBrush(Qt::white);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

void MyTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
        painter->setBrush(QColor(200, 100, 100, 100));
	painter->setPen(Qt::NoPen);
        painter->drawRect(boundingRect());
        QGraphicsSimpleTextItem::paint(painter,option,widget); 
}

MonitorGraphicsScene::MonitorGraphicsScene(int width, int height, QObject* parent) :
    QGraphicsScene(parent)
    , m_frame(NULL)
    , m_imageSizeChanged(true)
    , m_textureBuffer(0)
    , m_texture(0)
    , m_zoom(1)
    , m_needsResize(true)
    , m_pbo(0)
    , m_glWidget(NULL)
{
    m_imageRect = addRect(QRectF(0, 0, 0, 0));
    m_profileRect = addRect(QRectF(0, 0, width, height));
    m_profileRect->setPen(QColor(255, 50, 50));
    m_overlayItem = new MyTextItem();
    addItem(m_overlayItem);
    m_overlayItem->setPos(10, 10);
}

MonitorGraphicsScene::~MonitorGraphicsScene()
{
    //m_glWidget->makeCurrent();
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
    m_glWidget = glWidget;
    //m_glWidget->makeCurrent();
    /*glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);*/
}

void MonitorGraphicsScene::showFrame(mlt_frame frame_ptr)
{
    Mlt::Frame *frm = new Mlt::Frame(frame_ptr);
    m_frame.fetchAndStoreAcquire(frm);
    if (m_imageRect->rect().height() == 0) {
	zoomProfileFit();
    }
    update();
}

void MonitorGraphicsScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect);
    if (KDE_ISUNLIKELY(painter->paintEngine()->type() != QPaintEngine::OpenGL && painter->paintEngine()->type() != QPaintEngine::OpenGL2)) {
        kWarning() << "not used with a OpenGL viewport";
        return;
    }
    if (!m_glWidget) return;
    m_glWidget->makeCurrent();
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
    if (&m_frame) {
        frame = m_frame.fetchAndStoreOrdered(0);
    }

    if(KDE_ISUNLIKELY(!m_texture && !frame)) {
        return;
    }

    if (frame) {
        int width = 0;
        int height = 0;
	m_overlayItem->setText(QString("Overlay test: %1").arg(frame->get_position()));
        mlt_image_format format = mlt_image_rgb24a;
	frame->set("rescale.interp", "bilinear");
        frame->set("deinterlace_method", "onefield");
        frame->set("top_field_first", -1);
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

        //delete frame;
    }
    if (frame) delete frame;

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
	QMatrix scaleMatrix;
	scaleMatrix = scaleMatrix.scale(m_zoom, m_zoom);
	views()[0]->setMatrix(scaleMatrix);
	m_needsResize = true;
	//update();
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

void MonitorGraphicsScene::zoomProfileFit()
{
    if (views()[0]) {
        qreal levelX = views()[0]->viewport()->height() / m_profileRect->rect().height();
        qreal levelY = views()[0]->viewport()->width() / m_profileRect->rect().width();
	m_zoom = qMin(levelX, levelY);
	QMatrix scaleMatrix;
	//views()[0]->fitInView(0, 0, m_profileRect->sceneBoundingRect().width(), m_profileRect->sceneBoundingRect().height(), Qt::KeepAspectRatio);
	scaleMatrix = scaleMatrix.scale(m_zoom, m_zoom);
	views()[0]->setMatrix(scaleMatrix);
	m_needsResize = true;
	//views()[0]->ensureVisible(0, 0, m_profileRect->sceneBoundingRect().width(), m_profileRect->sceneBoundingRect().height(), 0, 0);
	views()[0]->centerOn(m_profileRect);
	update();
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
    //m_glWidget->makeCurrent();
    QGraphicsView *view = views()[0];
    if (event->modifiers() == Qt::ControlModifier) {
        if (event->delta() > 0) {
            view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            zoomIn();
            view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        } else {zoomOut();
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

void MonitorGraphicsScene::slotResized()
{
    
    m_needsResize = true;
    
    //if (!m_glWidget) return;
    //m_glWidget->makeCurrent();
    //update();
}
/*bool MonitorGraphicsScene::eventFilter(QObject* object, QEvent* event)
{
    QGraphicsScene::eventFilter(object, event);
    if (views().count() && object == views()[0]) {
        if (event->type() == QEvent::Resize) {
	    kDebug()<<"// GoT SCENR RESIZ";
            m_needsResize = true;
	    m_glWidget->makeCurrent();
	    update();
        }
        //return false;
    } else {
        return QGraphicsScene::eventFilter(object, event);
    }
    return QObject::eventFilter(object, event);
    
}*/

#include "monitorgraphicsscene.moc"
