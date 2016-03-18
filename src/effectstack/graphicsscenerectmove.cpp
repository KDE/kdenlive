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
#include "titler/titledocument.h"
#include "titler/gradientwidget.h"

#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QCursor>
#include <QTextCursor>
#include <QTextDocument>
#include <QList>
#include <QKeyEvent>
#include <QApplication>
#include <QTextBlock>

MyTextItem::MyTextItem(const QString &txt, QGraphicsItem *parent) :
    QGraphicsTextItem(txt, parent)
    , m_alignment(Qt::AlignLeft)
    , m_useShadow(false)
{
    setCacheMode(QGraphicsItem::ItemCoordinateCache);
    document()->setDocumentMargin(0);
    updateGeometry();
    connect(document(), SIGNAL(contentsChange(int, int, int)),
            this, SLOT(updateGeometry(int, int, int)));
}

Qt::Alignment MyTextItem::alignment() const
{
    return m_alignment;
}

void MyTextItem::updateShadow(bool enabled, int blur, int xoffset, int yoffset, QColor color)
{
    m_shadowOffset = QPoint(xoffset, yoffset);
    m_shadowBlur = blur;
    m_shadowColor = color;
    m_useShadow = enabled;
    updateShadow();
    update();
}

QStringList MyTextItem::shadowInfo() const
{
    QStringList info;
    info << QString::number(m_useShadow) << m_shadowColor.name(QColor::HexArgb) << QString::number( m_shadowBlur) << QString::number(m_shadowOffset.x()) << QString::number(m_shadowOffset.y());
    return info;
}

void MyTextItem::loadShadow(QStringList info)
{
    if (info.count() < 5) return;
    updateShadow((info.at(0).toInt() == true), info.at(2).toInt(), info.at(3).toInt(),info.at(4).toInt(), QColor(info.at(1)));
}

void MyTextItem::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
    QTextBlockFormat format;
    format.setAlignment(alignment);
    QTextCursor cursor = textCursor();      // save cursor position
    int position = textCursor().position();
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(format);
    cursor.clearSelection();
    cursor.setPosition(position);           // restore cursor position
    setTextCursor(cursor);
}

void MyTextItem::updateGeometry(int, int, int)
{
    updateGeometry();
    // update gradient if necessary
    QString gradientData = data(TitleDocument::Gradient).toString();
    if (!gradientData.isEmpty()) {
        QTextCursor cursor = textCursor(); 
        QTextCharFormat cformat;
        QRectF rect = boundingRect();
        int position = textCursor().position();
        QLinearGradient gr = GradientWidget::gradientFromString(gradientData, rect.width(), rect.height());
        cursor.select(QTextCursor::Document);
        cformat.setForeground(QBrush(gr));
        cursor.mergeCharFormat(cformat);
        cursor.clearSelection();
        cursor.setPosition(position);           // restore cursor position
        setTextCursor(cursor);
    }
    if (m_useShadow) {
        updateShadow();
    }
}

void MyTextItem::updateShadow()
{
    QString text = toPlainText();
    if (text.isEmpty()) {
        m_shadow = QImage();
        return;
    }
    QFontMetrics metrics(font());
    //ADJUST TO CURRENT SETTING
    int lineSpacing = 0;
    lineSpacing += metrics.lineSpacing();
    QPainterPath path;

    // Calculate line width
    QStringList lines = text.split('\n');
    double linePos = metrics.ascent();
    QRectF bounding = boundingRect();
    foreach(const QString &line, lines)
    {
        QPainterPath linePath;
        linePath.addText(0, linePos, font(), line);
        linePos += lineSpacing;
        if ( m_alignment == Qt::AlignHCenter ) {
            double offset = (bounding.width() - metrics.width(line)) / 2;
            linePath.translate(offset, 0);
        } else if ( m_alignment == Qt::AlignRight ) {
            double offset = (bounding.width() - metrics.width(line));
            linePath.translate(offset, 0);
        }
        path.addPath(linePath);
    }
    // Calculate position of text in parent item
    QRectF pathRect = QRectF(0, 0, bounding.width(), linePos - lineSpacing + metrics.descent() );
    QPointF offset = bounding.center() - pathRect.center() + m_shadowOffset;
    path.translate(offset);
    QRectF fullSize = bounding.united(path.boundingRect());
    QImage shadow(fullSize.width(), fullSize.height(), QImage::Format_ARGB32_Premultiplied);
    shadow.fill(Qt::transparent);
    QPainter painter(&shadow);
    painter.fillPath(path, QBrush(m_shadowColor));
    painter.end();
    if (m_shadowBlur > 0) {
        blurShadow(shadow, m_shadowBlur, true);
    } else {
        m_shadow = shadow;
    }
}

void MyTextItem::blurShadow(const QImage &image, int radius, bool alphaOnly)
{
    int tab[] = { 14, 10, 8, 6, 5, 5, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2 };
    int alpha = (radius < 1)  ? 16 : (radius > 17) ? 1 : tab[radius-1];

    m_shadow = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int r1 = 0;
    int r2 = image.height() - 1;
    int c1 = 0;
    int c2 = image.width() - 1;

    int bpl = m_shadow.bytesPerLine();
    int rgba[4];
    unsigned char* p;

    int i1 = 0;
    int i2 = 3;

    if (alphaOnly)
        i1 = i2 = (QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3);

    for (int col = c1; col <= c2; col++) {
        p = m_shadow.scanLine(r1) + col * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p += bpl;
        for (int j = r1; j < r2; j++, p += bpl)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int row = r1; row <= r2; row++) {
        p = m_shadow.scanLine(row) + c1 * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p += 4;
        for (int j = c1; j < c2; j++, p += 4)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int col = c1; col <= c2; col++) {
        p = m_shadow.scanLine(r2) + col * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p -= bpl;
        for (int j = r1; j < r2; j++, p -= bpl)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int row = r1; row <= r2; row++) {
        p = m_shadow.scanLine(row) + c2 * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p -= 4;
        for (int j = c1; j < c2; j++, p -= 4)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }
}

void MyTextItem::paint( QPainter *painter, const QStyleOptionGraphicsItem * option, QWidget* w)
{
    if (m_useShadow && !m_shadow.isNull()) {
        painter->drawImage(0, 0, m_shadow);
    }
    QGraphicsTextItem::paint(painter, option, w);
}
 
void MyTextItem::updateGeometry()
{
    QPointF topRightPrev = boundingRect().topRight();
    setTextWidth(-1);
    setTextWidth(boundingRect().width());
    setAlignment(m_alignment);
    QPointF topRight = boundingRect().topRight();
 
    if (m_alignment & Qt::AlignRight)
    {
        setPos(pos() + (topRightPrev - topRight));
    }
}

QRectF MyTextItem::boundingRect() const
{
    QRectF base = QGraphicsTextItem::boundingRect();
    QTextCursor cur(document());
    cur.select(QTextCursor::Document);
    QTextBlockFormat format = cur.blockFormat();
    int lines = document()->lineCount();
    int lineHeight = format.lineHeight();
    int lineHeight2 = QFontMetrics(font()).lineSpacing();
    if (lines > 1) {
        base.setHeight(lines * lineHeight2 + lineHeight * (lines - 1));
    }
    base.setRight(base.right() + m_shadowOffset.x());
    base.setBottom(base.bottom() + m_shadowOffset.y());
    return base;
}

MyRectItem::MyRectItem(QGraphicsItem *parent) :
    QGraphicsRectItem(parent)
{
    setCacheMode(QGraphicsItem::ItemCoordinateCache);
}

void MyRectItem::setRect(const QRectF & rectangle)
{
    QGraphicsRectItem::setRect(rectangle);
    if (m_rect != rectangle && !data(TitleDocument::Gradient).isNull()) {
        m_rect = rectangle;
        QLinearGradient gr = GradientWidget::gradientFromString(data(TitleDocument::Gradient).toString(), m_rect.width(), m_rect.height());
        setBrush(QBrush(gr));
    }
}

GraphicsSceneRectMove::GraphicsSceneRectMove(QObject *parent) :
    QGraphicsScene(parent),
    m_selectedItem(NULL),
    m_resizeMode(NoResize),
    m_tool(TITLE_RECTANGLE)
{
    //grabMouse();
    m_zoom = 1.0;
    setBackgroundBrush(QBrush(Qt::transparent));
    m_fontSize = 0;
}

void GraphicsSceneRectMove::setSelectedItem(QGraphicsItem *item)
{
    clearSelection();
    m_selectedItem = item;
    item->setSelected(true);
    update();
}

TITLETOOL GraphicsSceneRectMove::tool() const
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

void GraphicsSceneRectMove::keyPressEvent(QKeyEvent * keyEvent)
{
    if (m_selectedItem == NULL || !(m_selectedItem->flags() & QGraphicsItem::ItemIsMovable)) {
        QGraphicsScene::keyPressEvent(keyEvent);
        return;
    }
    if (m_selectedItem->type() == QGraphicsTextItem::Type) {
        MyTextItem *t = static_cast<MyTextItem *>(m_selectedItem);
        if (t->textInteractionFlags() & Qt::TextEditorInteraction) {
            QGraphicsScene::keyPressEvent(keyEvent);
            return;
        }
    }
    int diff = 1;
    if (keyEvent->modifiers() & Qt::ControlModifier) diff = 10;
    switch (keyEvent->key()) {
    case Qt::Key_Left:
        foreach (QGraphicsItem *qgi, selectedItems()) { qgi->moveBy(-diff,0); }
        emit itemMoved();
        break;
    case Qt::Key_Right:
        foreach (QGraphicsItem *qgi, selectedItems()) { qgi->moveBy( diff,0); }
        emit itemMoved();
        break;
    case Qt::Key_Up:
        foreach (QGraphicsItem *qgi, selectedItems()) { qgi->moveBy(0,-diff); }
        emit itemMoved();
        break;
    case Qt::Key_Down:
        foreach (QGraphicsItem *qgi, selectedItems()) { qgi->moveBy(0, diff); }
        emit itemMoved();
        break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        foreach (QGraphicsItem *qgi, selectedItems()) {
            if (qgi->data(-1).toInt() == -1) continue;
            removeItem(qgi);
            delete qgi;
        }
        m_selectedItem = NULL;
        emit selectionChanged();
        break;
    default:
        QGraphicsScene::keyPressEvent(keyEvent);
    }
    emit actionFinished();
}

void GraphicsSceneRectMove::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    QPointF p = e->scenePos();
    p += QPoint(-2, -2);
    m_resizeMode = NoResize;
    m_selectedItem = NULL;

    // http://www.kdenlive.org/mantis/view.php?id=1035
    QList<QGraphicsItem*> i = items(QRectF(p , QSizeF(4, 4)).toRect());
    if (i.isEmpty()) return;

    QGraphicsItem* g = i.first();
    if (g->type() == QGraphicsTextItem::Type) {
        m_selectedItem = g;
        MyTextItem *t = static_cast<MyTextItem *>(g);
        t->setTextInteractionFlags(Qt::TextEditorInteraction);
    } else emit doubleClickEvent();
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

    if (m_tool == TITLE_SELECT) {
        foreach(QGraphicsItem *g, list) {
            //qDebug() << " - - CHECKING ITEM Z:" << g->zValue() << ", TYPE: " << g->type();
            // check is there is a selected item in list
            if (g->zValue() > -1000 && g->isSelected()) {
                item = g;
                break;
            }
        }
        if (item == NULL  || !(item->flags() & QGraphicsItem::ItemIsSelectable)) {
            if (m_selectedItem && m_selectedItem->type() == QGraphicsTextItem::Type) {
                // disable text editing
                MyTextItem *t = static_cast<MyTextItem *>(m_selectedItem);
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
        if (item != NULL && item->flags() & QGraphicsItem::ItemIsMovable) {
            m_sceneClickPoint = e->scenePos();
            m_selectedItem = item;
            //qDebug() << "/////////  ITEM TYPE: " << item->type();
            if (item->type() == QGraphicsTextItem::Type) {
                MyTextItem *t = static_cast<MyTextItem *>(item);
                if (t->textInteractionFlags() == Qt::TextEditorInteraction) {
                    QGraphicsScene::mousePressEvent(e);
                    return;
                }
                t->setTextInteractionFlags(Qt::NoTextInteraction);
                setCursor(Qt::ClosedHandCursor);
            } else if (item->type() == QGraphicsRectItem::Type || item->type() == QGraphicsSvgItem::Type || item->type() == QGraphicsPixmapItem::Type) {
                QRectF r1;
                if (m_selectedItem->type() == QGraphicsRectItem::Type)
                    r1 = ((QGraphicsRectItem*)m_selectedItem)->rect().normalized();
                else
                    r1 = m_selectedItem->boundingRect().normalized();

                QList<QGraphicsView*> viewlist = views();
                QGraphicsView *view = NULL;
                if (viewlist.size() > 0) view = viewlist[0];
                if (view == NULL) return;
                // Item mapped coordinates
                QPolygon r = m_selectedItem->deviceTransform(view->viewportTransform()).map(r1).toPolygon();
                QPainterPath top(r.point(0));
                top.lineTo(r.point(1));
                QPainterPath bottom(r.point(2));
                bottom.lineTo(r.point(3));
                QPainterPath left(r.point(0));
                left.lineTo(r.point(3));
                QPainterPath right(r.point(1));
                right.lineTo(r.point(2));


                // The area interested by the mouse pointer
                QPoint viewPos = view->mapFromScene(e->scenePos());
                QPainterPath mouseArea;
                mouseArea.addRect(viewPos.x() - 4, viewPos.y() - 4, 8, 8);

                // Check for collisions between the mouse and the borders
                if (mouseArea.contains(r.point(0))) m_resizeMode = TopLeft;
                else if (mouseArea.contains(r.point(2))) m_resizeMode = BottomRight;
                else if (mouseArea.contains(r.point(1))) m_resizeMode = TopRight;
                else if (mouseArea.contains(r.point(3))) m_resizeMode = BottomLeft;
                else if (top.intersects(mouseArea)) m_resizeMode = Up;
                else if (bottom.intersects(mouseArea)) m_resizeMode = Down;
                else if (right.intersects(mouseArea)) m_resizeMode = Right;
                else if (left.intersects(mouseArea)) m_resizeMode = Left;

                else
                    setCursor(Qt::ClosedHandCursor);
            }
        }
        QGraphicsScene::mousePressEvent(e);
    } else if (m_tool == TITLE_RECTANGLE) {
        m_sceneClickPoint = e->scenePos();
        m_selectedItem = NULL;
    } else if (m_tool == TITLE_TEXT) {
        MyTextItem *textItem = new MyTextItem(QString(), NULL);
        addItem(textItem);
        emit newText(textItem);
        textItem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
        textItem->setPos(e->scenePos() - QPointF(0, (int)(m_fontSize / 2)));
        m_selectedItem = textItem;
        QGraphicsScene::mousePressEvent(e);
    }

    //qDebug() << "//////  MOUSE CLICK, RESIZE MODE: " << m_resizeMode;

}

void GraphicsSceneRectMove::clearTextSelection()
{
    if (m_selectedItem && m_selectedItem->type() == QGraphicsTextItem::Type) {
        // disable text editing
        MyTextItem *t = static_cast<MyTextItem *>(m_selectedItem);
        t->textCursor().setPosition(0);
        QTextBlock cur = t->textCursor().block();
        t->setTextCursor(QTextCursor(cur));
        t->setTextInteractionFlags(Qt::NoTextInteraction);
    }
    m_selectedItem = NULL;
    clearSelection();
}

void GraphicsSceneRectMove::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->buttons() != Qt::NoButton && (e->screenPos() - m_clickPoint).manhattanLength() < QApplication::startDragDistance()) {
        e->accept();
        return;
    }
    if (m_selectedItem && e->buttons() & Qt::LeftButton) {
        if (m_selectedItem->type() == QGraphicsRectItem::Type || m_selectedItem->type() == QGraphicsSvgItem::Type || m_selectedItem->type() == QGraphicsPixmapItem::Type) {
            QRectF newrect;
            if (m_selectedItem->type() == QGraphicsRectItem::Type)
                newrect = ((QGraphicsRectItem*)m_selectedItem)->rect();
            else
                newrect = m_selectedItem->boundingRect();
            QPointF newpoint = e->scenePos();
            /*
             * The vertices of the rectangle (check for matrix
             * transformation); to be replaced by QTransform::map()?
             */
            QPointF itemOrigin = m_selectedItem->scenePos();
            QTransform transform = m_selectedItem->transform();
            QPointF topLeft(transform.m11() * newrect.toRect().left() + transform.m21() * newrect.toRect().top() + transform.m31() + itemOrigin.x(), transform.m22() * newrect.toRect().top() + transform.m12() * newrect.toRect().left() + transform.m32() + itemOrigin.y());
            QPointF bottomLeft(transform.m11() * newrect.toRect().left() + transform.m21() * newrect.toRect().bottom() + transform.m31() + itemOrigin.x(), transform.m22() * newrect.toRect().bottom() + transform.m12() * newrect.toRect().left() + transform.m32() + itemOrigin.y());
            QPointF topRight(transform.m11() * newrect.toRect().right() + transform.m21() * newrect.toRect().top() + transform.m31() + itemOrigin.x(), transform.m22() * newrect.toRect().top() + transform.m12() * newrect.toRect().right() + transform.m32() + itemOrigin.y());
            QPointF bottomRight(transform.m11() * newrect.toRect().right() + transform.m21() * newrect.toRect().bottom() + transform.m31() + itemOrigin.x(), transform.m22() * newrect.toRect().bottom() + transform.m12() * newrect.toRect().right() + transform.m32() + itemOrigin.y());
            // Convert the mouse coordinates applying inverted transformation
            QPointF newPointRelative = newpoint - itemOrigin;
            QPointF resizePoint(transform.inverted().m11() * newPointRelative.x() + transform.inverted().m21() * newPointRelative.y() + transform.inverted().m31(), transform.inverted().m22() * newPointRelative.y() + transform.inverted().m12() * newPointRelative.x() + transform.inverted().m32());
            /*
             * Will check if the mouse is on the right of the limit lines with a
             * determinant (it must be less than zero because the Y axis is
             * inverted)
             */
            int determinantH, determinantV;
            // Check whether to resize or to just move the item(s)
            switch (m_resizeMode) {
            case TopLeft:
                determinantV = (bottomRight.x() - newpoint.x()) * (topRight.y() - newpoint.y()) - (bottomRight.y() - newpoint.y()) * (topRight.x() - newpoint.x());
                determinantH = (bottomLeft.x() - newpoint.x()) * (bottomRight.y() - newpoint.y()) - (bottomLeft.y() - newpoint.y()) * (bottomRight.x() - newpoint.x());
                if (determinantV < 0) {
                    if (determinantH < 0) {
                        // resizePoint is not working for some reason
                        newrect.setBottomRight(QPointF(newrect.width() - (transform.inverted().m11() * resizePoint.x() + transform.inverted().m21() * resizePoint.y() + transform.inverted().m31()), newrect.bottom() - (transform.inverted().m22() * resizePoint.y() + transform.inverted().m12() * resizePoint.x() + transform.inverted().m32())));
                        m_selectedItem->setPos(resizePoint + itemOrigin);
                    } else
                        m_resizeMode = BottomLeft;
                } else {
                    if (determinantH < 0)
                        m_resizeMode = TopRight;
                    else
                        m_resizeMode = BottomRight;
                }
                break;
            case BottomLeft:
                determinantV = (bottomRight.x() - newpoint.x()) * (topRight.y() - newpoint.y()) - (bottomRight.y() - newpoint.y()) * (topRight.x() - newpoint.x());
                determinantH = (topRight.x() - newpoint.x()) * (topLeft.y() - newpoint.y()) - (topRight.y() - newpoint.y()) * (topLeft.x() - newpoint.x());
                if (determinantV < 0) {
                    if (determinantH < 0) {
                        newrect.setBottomRight(QPointF(newrect.width() - resizePoint.x(), resizePoint.y()));
                        m_selectedItem->setPos(QPointF(transform.m11() * resizePoint.x() + transform.m21() *(newrect.bottom() - resizePoint.y()) + transform.m31() + itemOrigin.x(), transform.m22() *(newrect.bottom() - resizePoint.y()) + transform.m12() * resizePoint.x() + transform.m32() + itemOrigin.y()));
                    } else
                        m_resizeMode = TopLeft;
                } else {
                    if (determinantH < 0)
                        m_resizeMode = BottomRight;
                    else
                        m_resizeMode = TopRight;
                }
                break;
            case TopRight:
                determinantV = (topLeft.x() - newpoint.x()) * (bottomLeft.y() - newpoint.y()) - (topLeft.y() - newpoint.y()) * (bottomLeft.x() - newpoint.x());
                determinantH = (bottomLeft.x() - newpoint.x()) * (bottomRight.y() - newpoint.y()) - (bottomLeft.y() - newpoint.y()) * (bottomRight.x() - newpoint.x());
                if (determinantV < 0) {
                    if (determinantH < 0) {
                        newrect.setBottomRight(QPointF(resizePoint.x(), newrect.bottom() - resizePoint.y()));
                        m_selectedItem->setPos(QPointF(transform.m11() *(newrect.width() - resizePoint.x()) + transform.m21() * resizePoint.y() + transform.m31() + itemOrigin.x(), transform.m22() * resizePoint.y() + transform.m12() *(newrect.width() - resizePoint.x()) + transform.m32() + itemOrigin.y()));
                    } else
                        m_resizeMode = BottomRight;
                } else {
                    if (determinantH < 0)
                        m_resizeMode = TopLeft;
                    else
                        m_resizeMode = BottomLeft;
                }
                break;
            case BottomRight:
                determinantV = (topLeft.x() - newpoint.x()) * (bottomLeft.y() - newpoint.y()) - (topLeft.y() - newpoint.y()) * (bottomLeft.x() - newpoint.x());
                determinantH = (topRight.x() - newpoint.x()) * (topLeft.y() - newpoint.y()) - (topRight.y() - newpoint.y()) * (topLeft.x() - newpoint.x());
                if (determinantV < 0) {
                    if (determinantH < 0)
                        newrect.setBottomRight(resizePoint);
                    else
                        m_resizeMode = TopRight;
                } else {
                    if (determinantH < 0)
                        m_resizeMode = BottomLeft;
                    else
                        m_resizeMode = TopLeft;
                }
                break;
            case Left:
                determinantV = (bottomRight.x() - newpoint.x()) * (topRight.y() - newpoint.y()) - (bottomRight.y() - newpoint.y()) * (topRight.x() - newpoint.x());
                if (determinantV < 0) {
                    newrect.setRight(newrect.width() - resizePoint.x());
                    m_selectedItem->setPos(QPointF(transform.m11() * resizePoint.x() + transform.m31() + itemOrigin.x(), transform.m12() * resizePoint.x() + transform.m32() + itemOrigin.y()));
                } else
                    m_resizeMode = Right;
                break;
            case Right:
                determinantV = (topLeft.x() - newpoint.x()) * (bottomLeft.y() - newpoint.y()) - (topLeft.y() - newpoint.y()) * (bottomLeft.x() - newpoint.x());
                if (determinantV < 0)
                    newrect.setRight(resizePoint.x());
                else
                    m_resizeMode = Left;
                break;
            case Up:
                determinantH = (bottomLeft.x() - newpoint.x()) * (bottomRight.y() - newpoint.y()) - (bottomLeft.y() - newpoint.y()) * (bottomRight.x() - newpoint.x());
                if (determinantH < 0) {
                    newrect.setBottom(newrect.bottom() - resizePoint.y());
                    m_selectedItem->setPos(QPointF(transform.m21() * resizePoint.y() + transform.m31() + itemOrigin.x(), transform.m22() * resizePoint.y() + transform.m32() + itemOrigin.y()));
                } else
                    m_resizeMode = Down;
                break;
            case Down:
                determinantH = (topRight.x() - newpoint.x()) * (topLeft.y() - newpoint.y()) - (topRight.y() - newpoint.y()) * (topLeft.x() - newpoint.x());
                if (determinantH < 0)
                    newrect.setBottom(resizePoint.y());
                else
                    m_resizeMode = Up;
                break;
            default:
                QPointF diff = e->scenePos() - m_sceneClickPoint;
                m_sceneClickPoint = e->scenePos();
                foreach (QGraphicsItem *qgi, selectedItems()) { qgi->moveBy(diff.x(), diff.y()); }
                break;
            }
            if (m_selectedItem->type() == QGraphicsRectItem::Type && m_resizeMode != NoResize) {
                MyRectItem *gi = (MyRectItem*)m_selectedItem;
                // Resize using aspect ratio
                if (!m_selectedItem->data(0).isNull()) {
                    // we want to keep aspect ratio
                    double hRatio = (double) newrect.width() / m_selectedItem->data(0).toInt();
                    double vRatio = (double) newrect.height() / m_selectedItem->data(1).toInt();
                    if (hRatio < vRatio) newrect.setHeight(m_selectedItem->data(1).toInt() * hRatio);
                    else newrect.setWidth(m_selectedItem->data(0).toInt() * vRatio);
                }

                gi->setRect(newrect);
            }
            /*else {
            qreal s;
            if (resizeMode == Left || resizeMode == Right ) s = m_selectedItem->boundingRect().width() / newrect.width();
            else s = m_selectedItem->boundingRect().height() / newrect.height();
            m_selectedItem->scale( 1 / s, 1 / s );
            //qDebug()<<"/// SCALING SVG, RESIZE MODE: "<<resizeMode<<", RECT:"<<m_selectedItem->boundingRect();
            }*/
            //gi->setPos(m_selectedItem->scenePos());
            /*if (resizeMode == NoResize) {
                QGraphicsScene::mouseMoveEvent(e);
                return;
            }*/
        } else if (m_selectedItem->type() == QGraphicsTextItem::Type) {
            MyTextItem *t = static_cast<MyTextItem *>(m_selectedItem);
            if (t->textInteractionFlags() & Qt::TextEditorInteraction) {
                QGraphicsScene::mouseMoveEvent(e);
                return;
            }
            QPointF diff = e->scenePos() - m_sceneClickPoint;
            m_sceneClickPoint = e->scenePos();
            foreach (QGraphicsItem *qgi, selectedItems()) { qgi->moveBy(diff.x(), diff.y()); }
        }
        emit itemMoved();
    } else if (m_tool == TITLE_SELECT) {
        QList<QGraphicsView*> viewlist = views();
        QGraphicsView *view = NULL;
        if (viewlist.size() > 0) view = viewlist[0];

        QPointF p = e->scenePos();
        p += QPoint(-2, -2);
        m_resizeMode = NoResize;
        bool itemFound = false;
        QList<QGraphicsItem *> list = items(QRectF(p , QSizeF(4, 4)).toRect());
        foreach(const QGraphicsItem* g, list) {
            if ((g->type() == QGraphicsSvgItem::Type || g->type() == QGraphicsPixmapItem::Type) && g->zValue() > -1000) {
                // image or svg item
                setCursor(Qt::OpenHandCursor);
                break;
            } else if (g->type() == QGraphicsRectItem::Type && g->zValue() > -1000) {
                if (view == NULL) continue;
                QRectF r1 = ((const QGraphicsRectItem*)g)->rect().normalized();
                itemFound = true;

                // Item mapped coordinates
                QPolygon r = g->deviceTransform(view->viewportTransform()).map(r1).toPolygon();
                QPainterPath top(r.point(0));
                top.lineTo(r.point(1));
                QPainterPath bottom(r.point(2));
                bottom.lineTo(r.point(3));
                QPainterPath left(r.point(0));
                left.lineTo(r.point(3));
                QPainterPath right(r.point(1));
                right.lineTo(r.point(2));

                // The area interested by the mouse pointer
                QPoint viewPos = view->mapFromScene(e->scenePos());
                QPainterPath mouseArea;
                mouseArea.addRect(viewPos.x() - 4, viewPos.y() - 4, 8, 8);

                // Check for collisions between the mouse and the borders
                if (mouseArea.contains(r.point(0)) || mouseArea.contains(r.point(2))) setCursor(Qt::SizeFDiagCursor);
                else if (mouseArea.contains(r.point(1)) || mouseArea.contains(r.point(3))) setCursor(Qt::SizeBDiagCursor);
                else if (top.intersects(mouseArea) || bottom.intersects(mouseArea)) setCursor(Qt::SizeVerCursor);
                else if (right.intersects(mouseArea) || left.intersects(mouseArea)) setCursor(Qt::SizeHorCursor);
                else
                    setCursor(Qt::OpenHandCursor);
                break;
            }
            if (!itemFound) setCursor(Qt::ArrowCursor);
        }
        QGraphicsScene::mouseMoveEvent(e);
    } else if (m_tool == TITLE_RECTANGLE && e->buttons() & Qt::LeftButton) {
        if (m_selectedItem == NULL) {
            // create new rect item
            QRectF r(0, 0, e->scenePos().x() - m_sceneClickPoint.x(), e->scenePos().y() - m_sceneClickPoint.y());
            r = r.normalized();
            MyRectItem *rect = new MyRectItem();
            rect->setRect(QRectF(0, 0, r.width(), r.height()));
            addItem(rect);
            m_selectedItem = rect;
            m_selectedItem->setPos(m_sceneClickPoint);
            emit newRect(rect);
            m_selectedItem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
            m_resizeMode = BottomRight;
            QGraphicsScene::mouseMoveEvent(e);
        }
    }
}

void GraphicsSceneRectMove::wheelEvent(QGraphicsSceneWheelEvent * wheelEvent)
{
    if (wheelEvent->modifiers() == Qt::ControlModifier) {
        QList<QGraphicsView*> viewlist = views();
        ////qDebug() << wheelEvent->delta() << ' ' << zoom;
        if (viewlist.size() > 0) {
            if (wheelEvent->delta() > 0) emit sceneZoom(true);
            else emit sceneZoom(false);
        }
    } else wheelEvent->setAccepted(false);
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
    ////qDebug()<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setZoom(double s)
{
    QList<QGraphicsView*> viewlist = views();
    if (viewlist.size() > 0) {
        viewlist[0]->resetTransform();
        viewlist[0]->scale(s, s);
        m_zoom = s;
    }

    ////qDebug()<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setCursor(QCursor c)
{
    const QList<QGraphicsView*> l = views();
    foreach(QGraphicsView* v, l) {
        v->setCursor(c);
    }
}

void GraphicsSceneRectMove::slotUpdateFontSize(int s)
{
    m_fontSize = s;
}


