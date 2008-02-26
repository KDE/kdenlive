#include "graphicsscenerectmove.h"
#include <KDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QCursor>

QGraphicsItem* selected=NULL;
int button=0;
int resizeMode=-1;
enum resizeMode {NoResize,TopLeft,BottomLeft,TopRight,BottomRight,Left,Right,Up,Down};
GraphicsSceneRectMove::GraphicsSceneRectMove(QObject *parent):QGraphicsScene(parent){
	//grabMouse();	
}

void GraphicsSceneRectMove::mouseMoveEvent(QGraphicsSceneMouseEvent* e){

	if (selected && selected->type()==3 && button==1){
		
		QGraphicsRectItem *gi=(QGraphicsRectItem*)selected;
		QRectF newrect=gi->rect();
		QPointF newpoint=e->scenePos();
		newpoint-=selected->scenePos();
		switch (resizeMode){
		case TopLeft:
			newrect.setTopLeft(newpoint);
			break;
		case BottomLeft:
			newrect.setBottomLeft(newpoint);
			break;
		case TopRight:
			newrect.setTopRight(newpoint);
			break;
		case BottomRight:
			newrect.setBottomRight(newpoint);
			break;
		case Left:
			newrect.setLeft(newpoint.x());
			break;
		case Right:
			newrect.setRight(newpoint.x());
			break;
		case Up:
			newrect.setTop(newpoint.y());
			break;
		case Down:
			newrect.setBottom(newpoint.y());
			break;
		}
		
		gi->setRect(newrect);
		gi->setPos(selected->scenePos());
	}
	
	QPointF p=e->scenePos();
	p+=QPoint(-2,-2);
	resizeMode=NoResize;
	selected=NULL;
	foreach(QGraphicsItem* g, items( QRectF( p , QSizeF(4,4) ).toRect() ) ){

		if (g->type()==3 ){
			
			QGraphicsRectItem *gi=(QGraphicsRectItem*)g;
			QRectF r=gi->rect();
			r.translate(gi->scenePos());
			
			if ( (r.toRect().topLeft()-=e->scenePos().toPoint() ).manhattanLength()<3 ){
				resizeMode=TopLeft;
			}else if ((r.toRect().bottomLeft()-=e->scenePos().toPoint() ).manhattanLength()<3 ){
				resizeMode=BottomLeft;	
			}else if ((r.toRect().topRight()-=e->scenePos().toPoint() ).manhattanLength()<3 ){
				resizeMode=TopRight;	
			}else if ((r.toRect().bottomRight()-=e->scenePos().toPoint() ).manhattanLength()<3 ){
				resizeMode=BottomRight;	
			}else if ( qAbs(r.toRect().left()-e->scenePos().toPoint().x() ) <3){
				resizeMode=Left;	
			}else if ( qAbs(r.toRect().right()-e->scenePos().toPoint().x() ) <3){
				resizeMode=Right;	
			}else if ( qAbs(r.toRect().top()-e->scenePos().toPoint().y() ) <3){
				resizeMode=Up;	
			}else if ( qAbs(r.toRect().bottom()-e->scenePos().toPoint().y() ) <3){
				resizeMode=Down;	
			}
			if (resizeMode!=NoResize)
				selected=gi;
		}
		break;
	}
	switch (resizeMode){
		case TopLeft:
		case BottomRight:
			setCursor(QCursor(Qt::SizeFDiagCursor));
			break;
		case BottomLeft:
		case TopRight:
			setCursor(QCursor(Qt::SizeBDiagCursor));
			break;
		case Left:
		case Right:
			setCursor(Qt::SizeHorCursor);
			break;
		case Up:
		case Down:
			setCursor(Qt::SizeVerCursor);
			break;
		default:
			setCursor(QCursor(Qt::ArrowCursor));
			QGraphicsScene::mouseMoveEvent(e);
	}

	
}

void GraphicsSceneRectMove::mousePressEvent(QGraphicsSceneMouseEvent* e){
	button=e->button();
	if (!selected || selected->type()!=3)
		QGraphicsScene::mousePressEvent(e);
}
void GraphicsSceneRectMove::mouseReleaseEvent(QGraphicsSceneMouseEvent* e){
	button=0;
	if (!selected || selected->type()!=3)
		QGraphicsScene::mouseReleaseEvent(e);
}

void GraphicsSceneRectMove::setCursor(QCursor c){
	QList<QGraphicsView*> l=views();
	foreach(QGraphicsView* v, l){
		v->setCursor(c);
	}
}
