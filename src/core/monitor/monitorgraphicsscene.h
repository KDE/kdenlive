/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MONITORGRAPHICSSCENE_H
#define MONITORGRAPHICSSCENE_H

#include <QGraphicsScene>
#include <QMutex>
#include "mltcontroller.h"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <qt4/QtGui/qgraphicsitem.h>

#include <QElapsedTimer>

class QGLWidget;
namespace Mlt
{
    class Frame;
}
typedef QAtomicPointer<Mlt::Frame> AtomicFramePointer;

class MyTextItem : public QGraphicsSimpleTextItem
{
public:
    MyTextItem(QGraphicsItem * parent = 0);
protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  
};


/**
 * @class MonitorGraphicsScene
 * @brief Draws Mlt::Frame using OpenGL in a zoomable scene.
 * 
 * Upon consumer setup a pointer to an atomic pointer is provided.
 * When a new frame should be shown it needs to stored in the atomic pointer
 * and then update() should be called.
 */


class MonitorGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

public:
    /** @brief Constructor. */
    MonitorGraphicsScene(int width, int height, QObject* parent = 0);
    virtual ~MonitorGraphicsScene();

    /** @brief Returns the current zoom level. */
    qreal zoom() const;

    /**
     * @brief Performs OpenGL setup.
     * @param glWidget pointer to the OpenGL widget which is used as viewport for this scene's view
     */
    void initializeGL(QGLWidget *glWidget);

    void showFrame(mlt_frame frame_ptr);

public slots:
    /** 
     * @brief Sets the current zoom level.
     * @param level level of zoom. 1 means a 100% on the image
     */
    void setZoom(qreal level = 1);
    /** @brief Sets the zoom to fit the dimensions of the viewport. */
    void zoomFit();
    void zoomProfileFit();
    /**
     * @brief Zooms in.
     * @param by amount to zoom in by
     */
    void zoomIn(qreal by = .05);
    /**
     * @brief Zooms out.
     * @param by amount to zoom out by
     */
    void zoomOut(qreal by = .05);
    void slotResized();

signals:
    void zoomChanged(qreal level);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    //bool eventFilter(QObject *object, QEvent *event);

private:
    GLuint m_texture;
    GLubyte *m_textureBuffer;
    GLuint m_pbo;
    //Mlt::QFrame *m_frame;
    bool m_imageSizeChanged;
    uint8_t *m_image;
    int m_imageSize;
    bool m_hasNewImage;
    QGraphicsRectItem *m_imageRect;
    QGraphicsRectItem *m_profileRect;
    qreal m_zoom;
    bool m_needsResize;
    QMutex m_mutex;
    AtomicFramePointer m_frame;
    QGLWidget* m_glWidget;
    MyTextItem *m_overlayItem;

    QElapsedTimer m_timer;
    int m_frameCount;
};

#endif
