/***************************************************************************
 *   copyright (C) 2008 by Marco Gittler (g.marco@freenet.de)                                 *
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "graphicsscenerectmove.h"

#include <KDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QCursor>
#include <QTextCursor>
#include <QList>
#include <QKeyEvent>
#include <QApplication>
#include <QTextBlock>


GraphicsSceneRectMove::GraphicsSceneRectMove(QObject *parent) :
        QGraphicsScene(parent),
        m_selectedItem(NULL),
        m_resizeMode(NoResize),
        m_tool(TITLE_RECTANGLE)
{
    //grabMouse();
    m_zoom = 1.0;
    setBackgroundBrush(QBrush(Qt::transparent));
}

void GraphicsSceneRectMove::setSelectedItem(QGraphicsItem *item)
{
    clearSelection();
    m_selectedItem = item;
    item->setSelected(true);
    update();
}

TITLETOOL GraphicsSceneRectMove::tool()
{
    return m_tool;
}

void GraphicsSceneRectMove::setTool(TITLETOOL tool)
{
    m_tool = tool;
    switch (m_tool) {
    case TITLE_RECTANGLE:
        setCursor(Qt::CrossCursor);
        break;
    case TITLE_TEXT:
        setCursor(Qt::IBeamCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
    }
}

//virtual
void GraphicsSceneRectMove::keyPressEvent(QKeyEvent * keyEvent)
{
    if (m_selectedItem == NULL) {
        QGraphicsScene::keyPressEvent(keyEvent);
        return;
    }
    int diff = 1;
    if (m_selectedItem->type() == 8) {
        QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(m_selectedItem);
        if (t->textInteractionFlags() & Qt::TextEditorInteraction) {
            QGraphicsScene::keyPressEvent(keyEvent);
            return;
        }
    }
    if (keyEvent->modifiers() & Qt::ControlModifier) diff = 10;
    switch (keyEvent->key()) {
    case Qt::Key_Left:
        m_selectedItem->setPos(m_selectedItem->pos() - QPointF(diff, 0));
        emit itemMoved();
        break;
    case Qt::Key_Right:
        m_selectedItem->setPos(m_selectedItem->pos() + QPointF(diff, 0));
        emit itemMoved();
        break;
    case Qt::Key_Up:
        m_selectedItem->setPos(m_selectedItem->pos() - QPointF(0, diff));
        emit itemMoved();
        break;
    case Qt::Key_Down:
        m_selectedItem->setPos(m_selectedItem->pos() + QPointF(0, diff));
        emit itemMoved();
        break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        delete m_selectedItem;
        m_selectedItem = NULL;
        emit selectionChanged();
        break;
    default:
        QGraphicsScene::keyPressEvent(keyEvent);
    }
    emit actionFinished();
}

//virtual
void GraphicsSceneRectMove::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    QPointF p = e->scenePos();
    p += QPoint(-2, -2);
    m_resizeMode = NoResize;
    m_selectedItem = NULL;
    QGraphicsItem* g = items(QRectF(p , QSizeF(4, 4)).toRect()).at(0);
    if (g) {
        if (g->type() == 8) {
            QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(g);
            m_selectedItem = g;
            t->setTextInteractionFlags(Qt::TextEditorInteraction);
        }
    }
    QGraphicsScene::mouseDoubleClickEvent(e);
}

void GraphicsSceneRectMove::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if (m_tool == TITLE_RECTANGLE && m_selectedItem) setSelectedItem(m_selectedItem);
    QGraphicsScene::mouseReleaseEvent(e);
    emit actionFinished();
}

void GraphicsSceneRectMove::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    m_clickPoint = e->screenPos();
    QPointF p = e->scenePos();
    p += QPoint(-2, -2);
    m_resizeMode = NoResize;
    const QList <QGraphicsItem *> list = items(QRectF(p , QSizeF(4, 4)).toRect());
    QGraphicsItem *item = NULL;
    bool hasSelected = false;

    if (m_tool == TITLE_SELECT) {
        foreach(QGraphicsItem *g, list) {
            kDebug() << " - - CHECKING ITEM Z:" << g->zValue() << ", TYPE: " << g->type();
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
                t->textCursor().setPosition(0);
                QTextBlock cur = t->textCursor().block();
                t->setTextCursor(QTextCursor(cur));
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
            m_sceneClickPoint = e->scenePos();
            m_selectedItem = item;
            kDebug() << "/////////  ITEM TYPE: " << item->type();
            if (item->type() == 8) {
                QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(item);
                if (t->textInteractionFlags() == Qt::TextEditorInteraction) {
                    QGraphicsScene::mousePressEvent(e);
                    return;
                }
                t->setTextInteractionFlags(Qt::NoTextInteraction);
                setCursor(Qt::ClosedHandCursor);
            } else if (item->type() == 3 || item->type() == 13 || item->type() == 7) {
                QRectF r;
                if (m_selectedItem->type() == 3) {
                    r = ((QGraphicsRectItem*)m_selectedItem)->rect();
                } else r = m_selectedItem->boundingRect();

                r.translate(item->scenePos());
                if ((r.toRect().topLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    m_resizeMode = TopLeft;
                } else if ((r.toRect().bottomLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    m_resizeMode = BottomLeft;
                } else if ((r.toRect().topRight() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    m_resizeMode = TopRight;
                } else if ((r.toRect().bottomRight() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    m_resizeMode = BottomRight;
                } else if (qAbs(r.toRect().left() - e->scenePos().toPoint().x()) < 3 / m_zoom) {
                    m_resizeMode = Left;
                } else if (qAbs(r.toRect().right() - e->scenePos().toPoint().x()) < 3 / m_zoom) {
                    m_resizeMode = Right;
                } else if (qAbs(r.toRect().top() - e->scenePos().toPoint().y()) < 3 / m_zoom) {
                    m_resizeMode = Up;
                } else if (qAbs(r.toRect().bottom() - e->scenePos().toPoint().y()) < 3 / m_zoom) {
                    m_resizeMode = Down;
                } else setCursor(Qt::ClosedHandCursor);
            }
        }
        QGraphicsScene::mousePressEvent(e);
    } else if (m_tool == TITLE_RECTANGLE) {
        m_sceneClickPoint = e->scenePos();
        m_selectedItem = NULL;
    } else if (m_tool == TITLE_TEXT) {
        m_selectedItem = addText(QString());
        emit newText((QGraphicsTextItem *) m_selectedItem);
        m_selectedItem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        ((QGraphicsTextItem *)m_selectedItem)->setTextInteractionFlags(Qt::TextEditorInteraction);
        m_selectedItem->setPos(e->scenePos());
        QGraphicsScene::mousePressEvent(e);
    }

    kDebug() << "//////  MOUSE CLICK, RESIZE MODE: " << m_resizeMode;

}

void GraphicsSceneRectMove::clearTextSelection()
{
    if (m_selectedItem && m_selectedItem->type() == 8) {
        // disable text editing
        QGraphicsTextItem *t = static_cast<QGraphicsTextItem *>(m_selectedItem);
        t->textCursor().setPosition(0);
        QTextBlock cur = t->textCursor().block();
        t->setTextCursor(QTextCursor(cur));
        t->setTextInteractionFlags(Qt::NoTextInteraction);
    }
    m_selectedItem = NULL;
    clearSelection();
}

//virtual
void GraphicsSceneRectMove::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if ((e->screenPos() - m_clickPoint).manhattanLength() < QApplication::startDragDistance()) {
        e->accept();
        return;
    }
    if (m_selectedItem && e->buttons() & Qt::LeftButton) {
        if (m_selectedItem->type() == 3 || m_selectedItem->type() == 13 || m_selectedItem->type() == 7) {
            QRectF newrect;
            if (m_selectedItem->type() == 3) {
                newrect = ((QGraphicsRectItem*)m_selectedItem)->rect();
            } else newrect = m_selectedItem->boundingRect();

            QPointF newpoint = e->scenePos();
            //newpoint -= m_selectedItem->scenePos();
            switch (m_resizeMode) {
            case TopLeft:
                if (newpoint.x() < newrect.right() + m_selectedItem->pos().x() && newpoint.y() < newrect.bottom() + m_selectedItem->pos().y()) {
                    newrect.setBottomRight(newrect.bottomRight() + m_selectedItem->pos() - newpoint);
                    m_selectedItem->setPos(newpoint);
                }
                break;
            case BottomLeft:
                if (newpoint.x() < newrect.right() + m_selectedItem->pos().x() && newpoint.y() > m_selectedItem->pos().y()) {
                    newrect.setBottomRight(QPointF(newrect.bottomRight().x() + m_selectedItem->pos().x() - newpoint.x(), newpoint.y() - m_selectedItem->pos().y()));
                    m_selectedItem->setPos(QPointF(newpoint.x(), m_selectedItem->pos().y()));
                }
                break;
            case TopRight:
                if (newpoint.x() > m_selectedItem->pos().x() && newpoint.y() < newrect.bottom() + m_selectedItem->pos().y()) {
                    newrect.setBottomRight(QPointF(newpoint.x() - m_selectedItem->pos().x(), newrect.bottom() + m_selectedItem->pos().y() - newpoint.y()));
                    m_selectedItem->setPos(QPointF(m_selectedItem->pos().x(), newpoint.y()));
                }
                break;
            case BottomRight:
                if (newpoint.x() > m_selectedItem->pos().x() && newpoint.y() > m_selectedItem->pos().y()) {
                    newrect.setBottomRight(newpoint - m_selectedItem->pos());
                }
                break;
            case Left:
                if (newpoint.x() < newrect.right() + m_selectedItem->pos().x()) {
                    newrect.setRight(m_selectedItem->pos().x() + newrect.width() - newpoint.x());
                    m_selectedItem->setPos(QPointF(newpoint.x(), m_selectedItem->pos().y()));
                }
                break;
            case Right:
                if (newpoint.x() > m_selectedItem->pos().x()) {
                    newrect.setRight(newpoint.x() - m_selectedItem->pos().x());
                }
                break;
            case Up:
                if (newpoint.y() < newrect.bottom() + m_selectedItem->pos().y()) {
                    newrect.setBottom(m_selectedItem->pos().y() + newrect.bottom() - newpoint.y());
                    m_selectedItem->setPos(QPointF(m_selectedItem->pos().x(), newpoint.y()));
                }
                break;
            case Down:
                if (newpoint.y() > m_selectedItem->pos().y()) {
                    newrect.setBottom(newpoint.y() - m_selectedItem->pos().y());
                }
                break;
            default:
                QPointF diff = e->scenePos() - m_sceneClickPoint;
                m_sceneClickPoint = e->scenePos();
                m_selectedItem->moveBy(diff.x(), diff.y());
                break;
            }
            if (m_selectedItem->type() == 3 && m_resizeMode != NoResize) {
                QGraphicsRectItem *gi = (QGraphicsRectItem*)m_selectedItem;
                gi->setRect(newrect);
            }
            /*else {
            qreal s;
            if (resizeMode == Left || resizeMode == Right ) s = m_selectedItem->boundingRect().width() / newrect.width();
            else s = m_selectedItem->boundingRect().height() / newrect.height();
            m_selectedItem->scale( 1 / s, 1 / s );
            kDebug()<<"/// SCALING SVG, RESIZE MODE: "<<resizeMode<<", RECT:"<<m_selectedItem->boundingRect();
            }*/
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
            QPointF diff = e->scenePos() - m_sceneClickPoint;
            m_sceneClickPoint = e->scenePos();
            m_selectedItem->moveBy(diff.x(), diff.y());
        }
        emit itemMoved();
    } else if (m_tool == TITLE_SELECT) {
        QPointF p = e->scenePos();
        p += QPoint(-2, -2);
        m_resizeMode = NoResize;
        bool itemFound = false;
        foreach(const QGraphicsItem* g, items(QRectF(p , QSizeF(4, 4)).toRect())) {
            if ((g->type() == 13 || g->type() == 7) && g->zValue() > -1000) {
                // image or svg item
                setCursor(Qt::OpenHandCursor);
                break;
            } else if (g->type() == 3 && g->zValue() > -1000) {
                QRectF r = ((const QGraphicsRectItem*)g)->rect();
                r.translate(g->scenePos());
                itemFound = true;
                if ((r.toRect().topLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    setCursor(QCursor(Qt::SizeFDiagCursor));
                } else if ((r.toRect().bottomLeft() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    setCursor(QCursor(Qt::SizeBDiagCursor));
                } else if ((r.toRect().topRight() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    setCursor(QCursor(Qt::SizeBDiagCursor));
                } else if ((r.toRect().bottomRight() - e->scenePos().toPoint()).manhattanLength() < 6 / m_zoom) {
                    setCursor(QCursor(Qt::SizeFDiagCursor));
                } else if (qAbs(r.toRect().left() - e->scenePos().toPoint().x()) < 3 / m_zoom) {
                    setCursor(Qt::SizeHorCursor);
                } else if (qAbs(r.toRect().right() - e->scenePos().toPoint().x()) < 3 / m_zoom) {
                    setCursor(Qt::SizeHorCursor);
                } else if (qAbs(r.toRect().top() - e->scenePos().toPoint().y()) < 3 / m_zoom) {
                    setCursor(Qt::SizeVerCursor);
                } else if (qAbs(r.toRect().bottom() - e->scenePos().toPoint().y()) < 3 / m_zoom) {
                    setCursor(Qt::SizeVerCursor);
                } else setCursor(Qt::OpenHandCursor);
                break;
            }
            if (!itemFound) setCursor(Qt::ArrowCursor);
        }
        QGraphicsScene::mouseMoveEvent(e);
    } else if (m_tool == TITLE_RECTANGLE && e->buttons() & Qt::LeftButton) {
        if (m_selectedItem == NULL && (m_clickPoint - e->screenPos()).manhattanLength() >= QApplication::startDragDistance()) {
            // create new rect item
            m_selectedItem = addRect(0, 0, e->scenePos().x() - m_sceneClickPoint.x(), e->scenePos().y() - m_sceneClickPoint.y());
            emit newRect((QGraphicsRectItem *) m_selectedItem);
            m_selectedItem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
            m_selectedItem->setPos(m_sceneClickPoint);
            m_resizeMode = BottomRight;
            QGraphicsScene::mouseMoveEvent(e);
        }
    }
}

void GraphicsSceneRectMove::wheelEvent(QGraphicsSceneWheelEvent * wheelEvent)
{
    QList<QGraphicsView*> viewlist = views();
    //kDebug() << wheelEvent->delta() << " " << zoom;
    if (viewlist.size() > 0) {
        if (wheelEvent->delta() < 0) emit sceneZoom(true);
        else emit sceneZoom(false);
    }
}

void GraphicsSceneRectMove::setScale(double s)
{
    if (m_zoom < 1.0 / 7.0 && s < 1.0) return;
    else if (m_zoom > 10.0 / 7.9 && s > 1.0) return;
    QList<QGraphicsView*> viewlist = views();
    if (viewlist.size() > 0) {
        viewlist[0]->scale(s, s);
        m_zoom = m_zoom * s;
    }
    //kDebug()<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setZoom(double s)
{
    QList<QGraphicsView*> viewlist = views();
    if (viewlist.size() > 0) {
        viewlist[0]->resetTransform();
        viewlist[0]->scale(s, s);
        m_zoom = s;
    }

    //kDebug()<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setCursor(QCursor c)
{
    const QList<QGraphicsView*> l = views();
    foreach(QGraphicsView* v, l) {
        v->setCursor(c);
    }
}
