
#include <KDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QCursor>
#include <QList>

#include "graphicsscenerectmove.h"

GraphicsSceneRectMove::GraphicsSceneRectMove(QObject *parent): QGraphicsScene(parent), m_selectedItem(NULL), resizeMode(NoResize) {
    //grabMouse();
    zoom = 1.0;
}

//virtual
void GraphicsSceneRectMove::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) {
    QPointF p = e->scenePos();
    p += QPoint(-2, -2);
    resizeMode = NoResize;
    m_selectedItem = NULL;
    QGraphicsItem* g = items(QRectF(p , QSizeF(4, 4)).toRect()).at(0);
    if (g) {
        if (g->type() == 8) {
            QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(g);
            t->setTextInteractionFlags(Qt::TextEditorInteraction);
        }
    }
    QGraphicsScene::mouseDoubleClickEvent(e);
}

void GraphicsSceneRectMove::mousePressEvent(QGraphicsSceneMouseEvent* e) {
    QPointF p = e->scenePos();
    p += QPoint(-2, -2);
    resizeMode = NoResize;
    QList <QGraphicsItem *> list = items(QRectF(p , QSizeF(4, 4)).toRect());
    QGraphicsItem *item = NULL;
    bool hasSelected = false;
    foreach(QGraphicsItem* g, list) {
        // check is there is a selected item in list
        if (g->zValue() > -1000 && g->isSelected()) {
            hasSelected = true;
            item = g;
            break;
        }
    }
    if (item == NULL) {
        if (m_selectedItem && m_selectedItem->type() == 8) {
            // disable text editing
            QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(m_selectedItem);
            t->setTextInteractionFlags(Qt::NoTextInteraction);
        }
        m_selectedItem = NULL;
        foreach(QGraphicsItem* g, list) {
            if (g->zValue() > -1000) {
                item = g;
                break;
            }
        }
    }
    if (item != NULL) {
        m_clickPoint = e->scenePos();
        m_selectedItem = item;
        if (item->type() == 8) {
            QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(item);
            if (t->textInteractionFlags() == Qt::TextEditorInteraction) {
                QGraphicsScene::mousePressEvent(e);
                return;
            }
            t->setTextInteractionFlags(Qt::NoTextInteraction);
        } else if (item->type() == 3) {
            QGraphicsRectItem *gi = (QGraphicsRectItem*)item;
            QRectF r = gi->rect();
            r.translate(gi->scenePos());
            if ((r.toRect().topLeft() - e->scenePos().toPoint()).manhattanLength() < 6) {
                resizeMode = TopLeft;
            } else if ((r.toRect().bottomLeft() - e->scenePos().toPoint()).manhattanLength() < 6) {
                resizeMode = BottomLeft;
            } else if ((r.toRect().topRight() - e->scenePos().toPoint()).manhattanLength() < 6) {
                resizeMode = TopRight;
            } else if ((r.toRect().bottomRight() - e->scenePos().toPoint()).manhattanLength() < 6) {
                resizeMode = BottomRight;
            } else if (qAbs(r.toRect().left() - e->scenePos().toPoint().x()) < 3) {
                resizeMode = Left;
            } else if (qAbs(r.toRect().right() - e->scenePos().toPoint().x()) < 3) {
                resizeMode = Right;
            } else if (qAbs(r.toRect().top() - e->scenePos().toPoint().y()) < 3) {
                resizeMode = Up;
            } else if (qAbs(r.toRect().bottom() - e->scenePos().toPoint().y()) < 3) {
                resizeMode = Down;
            }
        }
    }
    if (!hasSelected) QGraphicsScene::mousePressEvent(e);

    kDebug() << "//////  MOUSE CLICK, RESIZE MODE: " << resizeMode;

}

//virtual
void GraphicsSceneRectMove::mouseReleaseEvent(QGraphicsSceneMouseEvent* e) {
    //m_selectedItem = NULL;
}


//virtual
void GraphicsSceneRectMove::mouseMoveEvent(QGraphicsSceneMouseEvent* e) {

    if (m_selectedItem && e->buttons() & Qt::LeftButton) {
        if (m_selectedItem->type() == 3) {
            QGraphicsRectItem *gi = (QGraphicsRectItem*)m_selectedItem;
            QRectF newrect = gi->rect();
            QPointF newpoint = e->scenePos();
            newpoint -= m_selectedItem->scenePos();
            switch (resizeMode) {
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
            default:
                QPointF diff = e->scenePos() - m_clickPoint;
                m_clickPoint = e->scenePos();
                gi->moveBy(diff.x(), diff.y());
                break;
            }
            gi->setRect(newrect);
            //gi->setPos(m_selectedItem->scenePos());
            /*if (resizeMode == NoResize) {
                QGraphicsScene::mouseMoveEvent(e);
                return;
            }*/
        } else if (m_selectedItem->type() == 8) {
            QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(m_selectedItem);
            if (t->textInteractionFlags() & Qt::TextEditorInteraction) {
                QGraphicsScene::mouseMoveEvent(e);
                return;
            }
            QPointF diff = e->scenePos() - m_clickPoint;
            m_clickPoint = e->scenePos();
            m_selectedItem->moveBy(diff.x(), diff.y());
        }
    } else {

        QPointF p = e->scenePos();
        p += QPoint(-2, -2);
        resizeMode = NoResize;

        foreach(QGraphicsItem* g, items(QRectF(p , QSizeF(4, 4)).toRect())) {

            if (g->type() == 3) {

                QGraphicsRectItem *gi = (QGraphicsRectItem*)g;
                QRectF r = gi->rect();
                r.translate(gi->scenePos());

                if ((r.toRect().topLeft() - e->scenePos().toPoint()).manhattanLength() < 6) {
                    setCursor(QCursor(Qt::SizeFDiagCursor));
                } else if ((r.toRect().bottomLeft() - e->scenePos().toPoint()).manhattanLength() < 6) {
                    setCursor(QCursor(Qt::SizeBDiagCursor));
                } else if ((r.toRect().topRight() - e->scenePos().toPoint()).manhattanLength() < 6) {
                    setCursor(QCursor(Qt::SizeBDiagCursor));
                } else if ((r.toRect().bottomRight() - e->scenePos().toPoint()).manhattanLength() < 6) {
                    setCursor(QCursor(Qt::SizeFDiagCursor));
                } else if (qAbs(r.toRect().left() - e->scenePos().toPoint().x()) < 3) {
                    setCursor(Qt::SizeHorCursor);
                } else if (qAbs(r.toRect().right() - e->scenePos().toPoint().x()) < 3) {
                    setCursor(Qt::SizeHorCursor);
                } else if (qAbs(r.toRect().top() - e->scenePos().toPoint().y()) < 3) {
                    setCursor(Qt::SizeVerCursor);
                } else if (qAbs(r.toRect().bottom() - e->scenePos().toPoint().y()) < 3) {
                    setCursor(Qt::SizeVerCursor);
                } else setCursor(QCursor(Qt::ArrowCursor));
            }
            break;
        }
        QGraphicsScene::mouseMoveEvent(e);
    }
}

void GraphicsSceneRectMove::wheelEvent(QGraphicsSceneWheelEvent * wheelEvent) {
    QList<QGraphicsView*> viewlist = views();
    kDebug() << wheelEvent->delta() << " " << zoom;
    double scale = 1.0;
    if (viewlist.size() > 0) {
        if (wheelEvent->delta() < 0 && zoom < 20.0) {
            scale = 1.1;

        } else if (wheelEvent->delta() > 0 && zoom > .05) {
            scale = 1 / 1.1;
        }
        setScale(scale);
        //viewlist[0]->resetTransform();
        //viewlist[0]->scale(zoom, zoom);
    }
}

void GraphicsSceneRectMove::setScale(double s) {
    QList<QGraphicsView*> viewlist = views();
    if (viewlist.size() > 0) {
        viewlist[0]->scale(s, s);
        zoom = zoom * s;
    }
}

void GraphicsSceneRectMove::setCursor(QCursor c) {
    QList<QGraphicsView*> l = views();
    foreach(QGraphicsView* v, l) {
        v->setCursor(c);
    }
}
