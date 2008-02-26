#ifndef GRAPHICSVIEWRECTMOVE_H
#define GRAPHICSVIEWRECTMOVE_H

#include <QGraphicsScene>

class GraphicsSceneRectMove: public QGraphicsScene {
public:
	GraphicsSceneRectMove(QObject* parent=0);
	void mouseMoveEvent(QGraphicsSceneMouseEvent*);
	void mousePressEvent(QGraphicsSceneMouseEvent*);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
private:
	void setCursor(QCursor);
};

#endif
