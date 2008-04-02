
#include <KDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QCursor>
#include <QList>
#include <QKeyEvent>

#include "graphicsscenerectmove.h"

GraphicsSceneRectMove::GraphicsSceneRectMove(QObject *parent): QGraphicsScene(parent), m_selectedItem(NULL), resizeMode(NoResize) {
    //grabMouse();
    zoom = 1.0;
}

void GraphicsSceneRectMove::setSelectedItem(QGraphicsItem *item) {
    m_selectedItem = item;
    item->setSelected(true);
}

//virtual
void GraphicsSceneRectMove::keyPressEvent(QKeyEvent * keyEvent) {
    if (m_selectedItem == NULL) {
        QGraphicsScene::keyPressEvent(keyEvent);
        return;
    }
    int diff = 1;
    if (keyEvent->modifiers() & Qt::ControlModifier) diff = 10;
    switch (keyEvent->key()) {
    case Qt::Key_Left:
        m_selectedItem->setPos(m_selectedItem->pos() - QPointF(diff, 0));
        break;
    case Qt::Key_Right:
        m_selectedItem->setPos(m_selectedItem->pos() + QPointF(diff, 0));
        break;
    case Qt::Key_Up:
        m_selectedItem->setPos(m_selectedItem->pos() - QPointF(0, diff));
        break;
    case Qt::Key_Down:
        m_selectedItem->setPos(m_selectedItem->pos() + QPointF(0, diff));
        break;
    default:
        QGraphicsScene::keyPressEvent(keyEvent);
    }
    emit itemMoved();
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
            if ((r.toRect().topLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                resizeMode = TopLeft;
            } else if ((r.toRect().bottomLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                resizeMode = BottomLeft;
            } else if ((r.toRect().topRight() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                resizeMode = TopRight;
            } else if ((r.toRect().bottomRight() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                resizeMode = BottomRight;
            } else if (qAbs(r.toRect().left() - e->scenePos().toPoint().x()) < 3 / zoom) {
                resizeMode = Left;
            } else if (qAbs(r.toRect().right() - e->scenePos().toPoint().x()) < 3 / zoom) {
                resizeMode = Right;
            } else if (qAbs(r.toRect().top() - e->scenePos().toPoint().y()) < 3 / zoom) {
                resizeMode = Up;
            } else if (qAbs(r.toRect().bottom() - e->scenePos().toPoint().y()) < 3 / zoom) {
                resizeMode = Down;
            }
        }
    }
    QGraphicsScene::mousePressEvent(e);

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
            //newpoint -= m_selectedItem->scenePos();
            switch (resizeMode) {
            case TopLeft:
                newrect.setBottomRight(newrect.bottomRight() + gi->pos() - newpoint);
                gi->setPos(newpoint);
                break;
            case BottomLeft:
                newrect.setBottomRight(QPointF(newrect.bottomRight().x() + gi->pos().x() - newpoint.x(), newpoint.y() - gi->pos().y()));
                gi->setPos(QPointF(newpoint.x(), gi->pos().y()));
                break;
            case TopRight:
                newrect.setBottomRight(QPointF(newpoint.x() - gi->pos().x(), newrect.bottom() + gi->pos().y() - newpoint.y()));
                gi->setPos(QPointF(gi->pos().x(), newpoint.y()));
                break;
            case BottomRight:
                newrect.setBottomRight(newpoint - gi->pos());
                break;
            case Left:
                newrect.setRight(gi->pos().x() + newrect.width() - newpoint.x());
                gi->setPos(QPointF(newpoint.x(), gi->pos().y()));
                break;
            case Right:
                newrect.setRight(newpoint.x() - gi->pos().x());
                break;
            case Up:
                newrect.setBottom(gi->pos().y() + newrect.bottom() - newpoint.y());
                gi->setPos(QPointF(gi->pos().x(), newpoint.y()));
                break;
            case Down:
                newrect.setBottom(newpoint.y() - gi->pos().y());
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
        emit itemMoved();
    } else {

        QPointF p = e->scenePos();
        p += QPoint(-2, -2);
        resizeMode = NoResize;
        bool itemFound = false;
        foreach(QGraphicsItem* g, items(QRectF(p , QSizeF(4, 4)).toRect())) {
            if (g->type() == 3) {
                QGraphicsRectItem *gi = (QGraphicsRectItem*)g;
                QRectF r = gi->rect();
                r.translate(gi->scenePos());
                itemFound = true;
                if ((r.toRect().topLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                    setCursor(QCursor(Qt::SizeFDiagCursor));
                } else if ((r.toRect().bottomLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                    setCursor(QCursor(Qt::SizeBDiagCursor));
                } else if ((r.toRect().topRight() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                    setCursor(QCursor(Qt::SizeBDiagCursor));
                } else if ((r.toRect().bottomRight() - e->scenePos().toPoint()).manhattanLength() < 6 / zoom) {
                    setCursor(QCursor(Qt::SizeFDiagCursor));
                } else if (qAbs(r.toRect().left() - e->scenePos().toPoint().x()) < 3 / zoom) {
                    setCursor(Qt::SizeHorCursor);
                } else if (qAbs(r.toRect().right() - e->scenePos().toPoint().x()) < 3 / zoom) {
                    setCursor(Qt::SizeHorCursor);
                } else if (qAbs(r.toRect().top() - e->scenePos().toPoint().y()) < 3 / zoom) {
                    setCursor(Qt::SizeVerCursor);
                } else if (qAbs(r.toRect().bottom() - e->scenePos().toPoint().y()) < 3 / zoom) {
                    setCursor(Qt::SizeVerCursor);
                } else setCursor(QCursor(Qt::ArrowCursor));
                break;
            }
            if (!itemFound) setCursor(QCursor(Qt::ArrowCursor));
        }
        QGraphicsScene::mouseMoveEvent(e);
    }
}

void GraphicsSceneRectMove::wheelEvent(QGraphicsSceneWheelEvent * wheelEvent) {
    QList<QGraphicsView*> viewlist = views();
    //kDebug() << wheelEvent->delta() << " " << zoom;
    double scale = 1.0;
    if (viewlist.size() > 0) {
        if (wheelEvent->delta() < 0) emit sceneZoom(true);
        else emit sceneZoom(false);
    }
}

void GraphicsSceneRectMove::setScale(double s) {
    if (zoom < 1.0 / 7.0 && s < 1.0) return;
    else if (zoom > 10.0 / 7.9 && s > 1.0) return;
    QList<QGraphicsView*> viewlist = views();
    if (viewlist.size() > 0) {
        viewlist[0]->scale(s, s);
        zoom = zoom * s;
    }
    //kDebug()<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setZoom(double s) {
    QList<QGraphicsView*> viewlist = views();
    if (viewlist.size() > 0) {
        viewlist[0]->resetTransform();
        viewlist[0]->scale(s, s);
        zoom = s;
    }

    //kDebug()<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setCursor(QCursor c) {
    QList<QGraphicsView*> l = views();
    foreach(QGraphicsView* v, l) {
        v->setCursor(c);
    }
}
