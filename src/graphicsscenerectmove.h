#ifndef GRAPHICSVIEWRECTMOVE_H
#define GRAPHICSVIEWRECTMOVE_H

#include <QGraphicsScene>

enum resizeModes {NoResize, TopLeft, BottomLeft, TopRight, BottomRight, Left, Right, Up, Down};

class GraphicsSceneRectMove: public QGraphicsScene {
    Q_OBJECT
public:
    GraphicsSceneRectMove(QObject* parent = 0);

    void setSelectedItem(QGraphicsItem *item);
    void setScale(double s);
    void setZoom(double s);

protected:
    virtual void keyPressEvent(QKeyEvent * keyEvent);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
    virtual void wheelEvent(QGraphicsSceneWheelEvent * wheelEvent);

private:
    void setCursor(QCursor);
    double zoom;
    QGraphicsItem* m_selectedItem;
    resizeModes resizeMode;
    QPointF m_clickPoint;

signals:
    void itemMoved();
    void sceneZoom(bool);
};

#endif
