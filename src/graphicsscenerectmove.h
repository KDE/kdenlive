#ifndef GRAPHICSVIEWRECTMOVE_H
#define GRAPHICSVIEWRECTMOVE_H

#include <QGraphicsScene>

enum resizeModes {NoResize, TopLeft, BottomLeft, TopRight, BottomRight, Left, Right, Up, Down};
enum TITLETOOL { TITLE_SELECT = 0, TITLE_RECTANGLE = 1, TITLE_TEXT = 2, TITLE_IMAGE = 3 };

class GraphicsSceneRectMove: public QGraphicsScene {
    Q_OBJECT
public:
    GraphicsSceneRectMove(QObject* parent = 0);

    void setSelectedItem(QGraphicsItem *item);
    void setScale(double s);
    void setZoom(double s);
    void setTool(TITLETOOL tool);
    TITLETOOL tool();
    void clearTextSelection();

protected:
    virtual void keyPressEvent(QKeyEvent * keyEvent);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    virtual void wheelEvent(QGraphicsSceneWheelEvent * wheelEvent);

private:
    void setCursor(QCursor);
    double zoom;
    QGraphicsItem* m_selectedItem;
    resizeModes resizeMode;
    QPointF m_sceneClickPoint;
    TITLETOOL m_tool;
    QPoint m_clickPoint;

signals:
    void itemMoved();
    void sceneZoom(bool);
    void newRect(QGraphicsRectItem *);
    void newText(QGraphicsTextItem *);
    void actionFinished();
};

#endif
