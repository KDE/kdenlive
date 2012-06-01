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

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <QElapsedTimer>

class QGLWidget;
namespace Mlt
{
    class Frame;
}
typedef QAtomicPointer<Mlt::Frame> AtomicFramePointer;


class MonitorGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

public:
    MonitorGraphicsScene(QObject* parent = 0);
    virtual ~MonitorGraphicsScene();

    qreal zoom() const;

    void initializeGL(QGLWidget *glWidget);
    void setFramePointer(AtomicFramePointer *frame);

public slots:
    void setZoom(qreal level = 1);
    void zoomFit();
    void zoomIn(qreal by = .05);
    void zoomOut(qreal by = .05);

signals:
    void zoomChanged(qreal level);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

private:
    GLuint m_texture;
    GLubyte *m_textureBuffer;
    GLuint m_pbo;
    AtomicFramePointer *m_frame;
    bool m_imageSizeChanged;
    uint8_t *m_image;
    int m_imageSize;
    bool m_hasNewImage;
    QGraphicsRectItem *m_imageRect;
    qreal m_zoom;
    bool m_needsResize;

    QElapsedTimer m_timer;
    int m_frameCount;
};

#endif
