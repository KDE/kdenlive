#ifndef GRAPHICSVIEWRECTMOVE_H
#define GRAPHICSVIEWRECTMOVE_H

#include <QGraphicsScene>

class GraphicsSceneRectMove: public QGraphicsScene {
public:
    GraphicsSceneRectMove(QObject* parent = 0);
    void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    void wheelEvent(QGraphicsSceneWheelEvent * wheelEvent);
private:
    void setCursor(QCursor);
    double zoom;
};

#endif
