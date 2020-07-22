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
#include "titler/gradientwidget.h"
#include "titler/titledocument.h"

#include "kdenlive_debug.h"
#include <QApplication>
#include <QCursor>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QList>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <utility>

MyQGraphicsEffect::MyQGraphicsEffect(QObject *parent)
    : QGraphicsEffect(parent)

{
}

void MyQGraphicsEffect::setShadow(const QImage &image)
{
    m_shadow = image;
}

void MyQGraphicsEffect::setOffset(int xOffset, int yOffset, int blur)
{
    m_xOffset = xOffset;
    m_yOffset = yOffset;
    m_blur = blur;
    updateBoundingRect();
}

void MyQGraphicsEffect::draw(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::transparent);
    painter->drawImage(-2 * m_blur + m_xOffset, -2 * m_blur + m_yOffset, m_shadow);
    drawSource(painter);
}

MyTextItem::MyTextItem(const QString &txt, QGraphicsItem *parent)
    : QGraphicsTextItem(txt, parent)
    , m_alignment(qApp->isLeftToRight() ? Qt::AlignRight : Qt::AlignLeft)
{
    setCacheMode(QGraphicsItem::ItemCoordinateCache);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    document()->setDocumentMargin(0);
    m_shadowEffect = new MyQGraphicsEffect(this);
    m_shadowEffect->setEnabled(false);
    setGraphicsEffect(m_shadowEffect);
    updateGeometry();
    connect(document(), SIGNAL(contentsChange(int,int,int)), this, SLOT(updateGeometry(int,int,int)));
}

Qt::Alignment MyTextItem::alignment() const
{
    return m_alignment;
}

void MyTextItem::updateShadow(bool enabled, int blur, int xoffset, int yoffset, QColor color)
{
    m_shadowOffset = QPoint(xoffset, yoffset);
    m_shadowBlur = blur;
    m_shadowColor = std::move(color);
    m_shadowEffect->setEnabled(enabled);
    m_shadowEffect->setOffset(xoffset, yoffset, blur);
    if (enabled) {
        updateShadow();
    }
    update();
}

void MyTextItem::setTextColor(const QColor &col)
{
    setDefaultTextColor(col);
    refreshFormat();
}

QStringList MyTextItem::shadowInfo() const
{
    QStringList info;
    info << QString::number(static_cast<int>(m_shadowEffect->isEnabled())) << m_shadowColor.name(QColor::HexArgb) << QString::number(m_shadowBlur)
         << QString::number(m_shadowOffset.x()) << QString::number(m_shadowOffset.y());
    return info;
}

void MyTextItem::loadShadow(const QStringList &info)
{
    if (info.count() < 5) {
        return;
    }
    updateShadow((static_cast<bool>(info.at(0).toInt())), info.at(2).toInt(), info.at(3).toInt(), info.at(4).toInt(), QColor(info.at(1)));
}

void MyTextItem::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
    QTextBlockFormat format;
    format.setAlignment(alignment);
    QTextCursor cursor = textCursor(); // save cursor position
    int position = textCursor().position();
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(format);
    cursor.clearSelection();
    cursor.setPosition(position); // restore cursor position
    setTextCursor(cursor);
}

void MyTextItem::refreshFormat()
{
    QString gradientData = data(TitleDocument::Gradient).toString();
    QTextCursor cursor = textCursor();
    QTextCharFormat cformat;
    cursor.select(QTextCursor::Document);
    int position = textCursor().position();

    // Formatting can be lost on paste, since our QTextCursor gets overwritten, so re-apply all formatting here
    QColor fgColor = defaultTextColor();
    cformat.setForeground(fgColor);
    cformat.setFont(font());

    if (!gradientData.isEmpty()) {
        QRectF rect = boundingRect();
        QLinearGradient gr = GradientWidget::gradientFromString(gradientData, rect.width(), rect.height());
        cformat.setForeground(QBrush(gr));
    }

    // Apply
    cursor.mergeCharFormat(cformat);
    // restore cursor position
    cursor.clearSelection();
    cursor.setPosition(position);
    setTextCursor(cursor);
}

void MyTextItem::updateGeometry(int, int, int)
{
    updateGeometry();
    // update gradient if necessary
    refreshFormat();

    QString text = toPlainText();
    m_path = QPainterPath();
    m_path.setFillRule(Qt::WindingFill);
    if (text.isEmpty()) {
        //
    } else {
        QFontMetrics metrics(font());
        double lineSpacing = data(TitleDocument::LineSpacing).toInt() + metrics.lineSpacing();

        // Calculate line width
        const QStringList lines = text.split(QLatin1Char('\n'));
        double linePos = metrics.ascent();
        QRectF bounding = boundingRect();
        /*if (lines.count() > 0) {
            lineSpacing = bounding.height() / lines.count();
            if (lineSpacing != data(TitleDocument::LineSpacing).toInt() + metrics.lineSpacing()) {
                linePos = 2 * lineSpacing - metrics.descent() - metrics.height();
            }
        }*/

        for (const QString &line : lines) {
            QPainterPath linePath;
            linePath.addText(0, linePos, font(), line);
            linePos += lineSpacing;
            if (m_alignment == Qt::AlignHCenter) {
                double offset = (bounding.width() - metrics.horizontalAdvance(line)) / 2;
                linePath.translate(offset, 0);
            } else if (m_alignment == Qt::AlignRight) {
                double offset = bounding.width() - metrics.horizontalAdvance(line);
                linePath.translate(offset, 0);
            }
            m_path.addPath(linePath);
        }
    }

    if (m_shadowEffect->isEnabled()) {
        updateShadow();
    }
    update();
}

void MyTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *w)
{
    if ((textInteractionFlags() & static_cast<int>((Qt::TextEditable) != 0)) != 0) {
        QGraphicsTextItem::paint(painter, option, w);
    } else {
        painter->setRenderHint(QPainter::Antialiasing);
        int outline = data(TitleDocument::OutlineWidth).toInt();
        QString gradientData = data(TitleDocument::Gradient).toString();
        QTextCursor cursor(document());
        cursor.select(QTextCursor::Document);
        QBrush paintBrush;
        if (gradientData.isEmpty()) {
            paintBrush = QBrush(cursor.charFormat().foreground().color());
        } else {
            QRectF rect = boundingRect();
            paintBrush = QBrush(GradientWidget::gradientFromString(gradientData, rect.width(), rect.height()));
        }
        painter->fillPath(m_path, paintBrush);
        if (outline > 0) {
            QVariant variant = data(TitleDocument::OutlineColor);
            QColor outlineColor = variant.value<QColor>();
            QPen pen(outlineColor);
            pen.setWidthF(outline);
            painter->strokePath(m_path, pen);
        }
        if (isSelected()) {
            QPen pen(Qt::red);
            pen.setStyle(Qt::DashLine);
            painter->setPen(pen);
            painter->drawRect(boundingRect());
        }
    }
}

void MyTextItem::updateShadow()
{
    QString text = toPlainText();
    if (text.isEmpty()) {
        m_shadowEffect->setShadow(QImage());
        return;
    }
    QRectF bounding = boundingRect();
    QPainterPath path = m_path;
    // Calculate position of text in parent item
    path.translate(QPointF(2 * m_shadowBlur, 2 * m_shadowBlur));
    QRectF fullSize = bounding.united(path.boundingRect());
    QImage shadow(fullSize.width() + qAbs(m_shadowOffset.x()) + 4 * m_shadowBlur, fullSize.height() + qAbs(m_shadowOffset.y()) + 4 * m_shadowBlur,
                  QImage::Format_ARGB32_Premultiplied);
    shadow.fill(Qt::transparent);
    QPainter painter(&shadow);
    painter.fillPath(path, QBrush(m_shadowColor));
    painter.end();
    if (m_shadowBlur > 0) {
        blurShadow(shadow, m_shadowBlur);
    }
    m_shadowEffect->setShadow(shadow);
}

void MyTextItem::blurShadow(QImage &result, int radius)
{
    int tab[] = {14, 10, 8, 6, 5, 5, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2};
    int alpha = (radius < 1) ? 16 : (radius > 17) ? 1 : tab[radius - 1];

    int r1 = 0;
    int r2 = result.height() - 1;
    int c1 = 0;
    int c2 = result.width() - 1;

    int bpl = result.bytesPerLine();
    int rgba[4];
    unsigned char *p;

    int i1 = 0;
    int i2 = 3;

    for (int col = c1; col <= c2; col++) {
        p = result.scanLine(r1) + col * 4;
        for (int i = i1; i <= i2; i++) {
            rgba[i] = p[i] << 4;
        }

        p += bpl;
        for (int j = r1; j < r2; j++, p += bpl)
            for (int i = i1; i <= i2; i++) {
                p[i] = (uchar)((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
            }
    }

    for (int row = r1; row <= r2; row++) {
        p = result.scanLine(row) + c1 * 4;
        for (int i = i1; i <= i2; i++) {
            rgba[i] = p[i] << 4;
        }

        p += 4;
        for (int j = c1; j < c2; j++, p += 4)
            for (int i = i1; i <= i2; i++) {
                p[i] = (uchar)((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
            }
    }

    for (int col = c1; col <= c2; col++) {
        p = result.scanLine(r2) + col * 4;
        for (int i = i1; i <= i2; i++) {
            rgba[i] = p[i] << 4;
        }

        p -= bpl;
        for (int j = r1; j < r2; j++, p -= bpl)
            for (int i = i1; i <= i2; i++) {
                p[i] = (uchar)((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
            }
    }

    for (int row = r1; row <= r2; row++) {
        p = result.scanLine(row) + c2 * 4;
        for (int i = i1; i <= i2; i++) {
            rgba[i] = p[i] << 4;
        }

        p -= 4;
        for (int j = c1; j < c2; j++, p -= 4)
            for (int i = i1; i <= i2; i++) {
                p[i] = (uchar)((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
            }
    }
}

void MyTextItem::updateGeometry()
{
    QPointF topRightPrev = boundingRect().topRight();
    setTextWidth(-1);
    setTextWidth(boundingRect().width());
    setAlignment(m_alignment);
    QPointF topRight = boundingRect().topRight();

    if ((m_alignment & static_cast<int>((Qt::AlignRight) != 0)) != 0) {
        setPos(pos() + (topRightPrev - topRight));
    }
}

QRectF MyTextItem::baseBoundingRect() const
{
    QRectF base = QGraphicsTextItem::boundingRect();
    QTextCursor cur(document());
    cur.select(QTextCursor::Document);
    QTextBlockFormat format = cur.blockFormat();
    int lineHeight = format.lineHeight();
    int lineHeight2 = QFontMetrics(font()).lineSpacing();
    int lines = document()->lineCount();
    if (lines > 1) {
        base.setHeight(lines * lineHeight2 + lineHeight * (lines - 1));
    }
    return base;
}

QRectF MyTextItem::boundingRect() const
{
    QRectF base = baseBoundingRect();
    if (m_shadowEffect->isEnabled() && m_shadowOffset.x() > 0) {
        base.setRight(base.right() + m_shadowOffset.x());
    }
    if (m_shadowEffect->isEnabled() && m_shadowOffset.y() > 0) {
        base.setBottom(base.bottom() + m_shadowOffset.y());
    }
    return base;
}

QVariant MyTextItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && (scene() != nullptr)) {
        QPoint newPos = value.toPoint();
        if (QApplication::mouseButtons() == Qt::LeftButton && (qobject_cast<GraphicsSceneRectMove *>(scene()) != nullptr)) {
            auto *customScene = qobject_cast<GraphicsSceneRectMove *>(scene());
            int gridSize = customScene->gridSize();
            int xV = (newPos.x() / gridSize) * gridSize;
            int yV = (newPos.y() / gridSize) * gridSize;
            newPos = QPoint(xV, yV);
        }
        return newPos;
    }
    if (change == QGraphicsItem::ItemSelectedHasChanged) {
        if (!value.toBool()) {
            // Make sure to deselect text when item loses focus
            QTextCursor cur(document());
            cur.clearSelection();
            setTextCursor(cur);
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void MyTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *evt)
{
    if (textInteractionFlags() == Qt::TextEditorInteraction) {
        // if editor mode is already on: pass double click events on to the editor:
        QGraphicsTextItem::mouseDoubleClickEvent(evt);
        return;
    }
    // if editor mode is off:
    // 1. turn editor mode on and set selected and focused:
    // SetTextInteraction(true);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFocus(Qt::MouseFocusReason);
    // 2. send a single click to this QGraphicsTextItem (this will set the cursor to the mouse position):
    // create a new mouse event with the same parameters as evt
    auto *click = new QGraphicsSceneMouseEvent(QEvent::GraphicsSceneMousePress);
    click->setButton(evt->button());
    click->setPos(evt->pos());
    QGraphicsTextItem::mousePressEvent(click);
    delete click; // don't forget to delete the event
}

MyRectItem::MyRectItem(QGraphicsItem *parent)
    : QGraphicsRectItem(parent)
{
    setCacheMode(QGraphicsItem::ItemCoordinateCache);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

void MyRectItem::setRect(const QRectF &rectangle)
{
    QGraphicsRectItem::setRect(rectangle);
    if (m_rect != rectangle && !data(TitleDocument::Gradient).isNull()) {
        m_rect = rectangle;
        QLinearGradient gr = GradientWidget::gradientFromString(data(TitleDocument::Gradient).toString(), m_rect.width(), m_rect.height());
        setBrush(QBrush(gr));
    }
}

QVariant MyRectItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && (scene() != nullptr)) {
        QPoint newPos = value.toPoint();
        if (QApplication::mouseButtons() == Qt::LeftButton && (qobject_cast<GraphicsSceneRectMove *>(scene()) != nullptr)) {
            auto *customScene = qobject_cast<GraphicsSceneRectMove *>(scene());
            int gridSize = customScene->gridSize();
            int xV = (newPos.x() / gridSize) * gridSize;
            int yV = (newPos.y() / gridSize) * gridSize;
            newPos = QPoint(xV, yV);
        }
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

MyPixmapItem::MyPixmapItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
{
    setCacheMode(QGraphicsItem::ItemCoordinateCache);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

QVariant MyPixmapItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && (scene() != nullptr)) {
        QPoint newPos = value.toPoint();
        if (QApplication::mouseButtons() == Qt::LeftButton && (qobject_cast<GraphicsSceneRectMove *>(scene()) != nullptr)) {
            auto *customScene = qobject_cast<GraphicsSceneRectMove *>(scene());
            int gridSize = customScene->gridSize();
            int xV = (newPos.x() / gridSize) * gridSize;
            int yV = (newPos.y() / gridSize) * gridSize;
            newPos = QPoint(xV, yV);
        }
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

MySvgItem::MySvgItem(const QString &fileName, QGraphicsItem *parent)
    : QGraphicsSvgItem(fileName, parent)
{
    setCacheMode(QGraphicsItem::ItemCoordinateCache);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

QVariant MySvgItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && (scene() != nullptr)) {
        QPoint newPos = value.toPoint();
        if (QApplication::mouseButtons() == Qt::LeftButton && (qobject_cast<GraphicsSceneRectMove *>(scene()) != nullptr)) {
            auto *customScene = qobject_cast<GraphicsSceneRectMove *>(scene());
            int gridSize = customScene->gridSize();
            int xV = (newPos.x() / gridSize) * gridSize;
            int yV = (newPos.y() / gridSize) * gridSize;
            newPos = QPoint(xV, yV);
        }
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}
GraphicsSceneRectMove::GraphicsSceneRectMove(QObject *parent)
    : QGraphicsScene(parent)

{
    // grabMouse();
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

void GraphicsSceneRectMove::keyPressEvent(QKeyEvent *keyEvent)
{
    if (m_selectedItem == nullptr || !(m_selectedItem->flags() & QGraphicsItem::ItemIsMovable)) {
        QGraphicsScene::keyPressEvent(keyEvent);
        return;
    }
    if (m_selectedItem->type() == QGraphicsTextItem::Type) {
        auto *t = static_cast<MyTextItem *>(m_selectedItem);
        if ((t->textInteractionFlags() & static_cast<int>((Qt::TextEditorInteraction) != 0)) != 0) {
            QGraphicsScene::keyPressEvent(keyEvent);
            return;
        }
    }
    int diff = m_gridSize;
    if ((keyEvent->modifiers() & Qt::ControlModifier) != 0u) {
        diff = m_gridSize * 5;
    }
    switch (keyEvent->key()) {
    case Qt::Key_Left:
        for (QGraphicsItem *qgi : selectedItems()) {
            qgi->moveBy(-diff, 0);
        }
        emit itemMoved();
        break;
    case Qt::Key_Right:
        for (QGraphicsItem *qgi : selectedItems()) {
            qgi->moveBy(diff, 0);
        }
        emit itemMoved();
        break;
    case Qt::Key_Up:
        for (QGraphicsItem *qgi : selectedItems()) {
            qgi->moveBy(0, -diff);
        }
        emit itemMoved();
        break;
    case Qt::Key_Down:
        for (QGraphicsItem *qgi : selectedItems()) {
            qgi->moveBy(0, diff);
        }
        emit itemMoved();
        break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        for (QGraphicsItem *qgi : selectedItems()) {
            if (qgi->data(-1).toInt() == -1) {
                continue;
            }
            removeItem(qgi);
            delete qgi;
        }
        m_selectedItem = nullptr;
        emit selectionChanged();
        break;
    default:
        QGraphicsScene::keyPressEvent(keyEvent);
    }
    emit actionFinished();
}

void GraphicsSceneRectMove::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e)
{
    QPointF p = e->scenePos();
    p += QPoint(-2, -2);
    m_resizeMode = NoResize;
    m_selectedItem = nullptr;

    // http://web.archive.org/web/20140728070013/http://www.kdenlive.org/mantis/view.php?id=1035
    QList<QGraphicsItem *> i = items(QRectF(p, QSizeF(4, 4)).toRect());
    if (i.isEmpty()) {
        return;
    }

    int ix = 1;
    QGraphicsItem *g = i.constFirst();
    while (!(g->flags() & QGraphicsItem::ItemIsSelectable) && ix < i.count()) {
        g = i.at(ix);
        ix++;
    }
    if ((g != nullptr) && g->type() == QGraphicsTextItem::Type && (((g->flags() & static_cast<int>((QGraphicsItem::ItemIsSelectable) != 0))) != 0)) {
        m_selectedItem = g;
    } else {
        emit doubleClickEvent();
    }
    QGraphicsScene::mouseDoubleClickEvent(e);
}

void GraphicsSceneRectMove::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    m_pan = false;
    if (m_tool == TITLE_RECTANGLE && (m_selectedItem != nullptr)) {
        setSelectedItem(m_selectedItem);
    }
    if (m_createdText) {
        m_selectedItem->setSelected(true);
        auto *newText = static_cast<MyTextItem *>(m_selectedItem);
        QTextCursor cur(newText->document());
        cur.select(QTextCursor::Document);
        newText->setTextCursor(cur);
        m_createdText = false;
    }
    if ((e->modifiers() & Qt::ShiftModifier) != 0u) {
        e->accept();
    } else {
        QGraphicsScene::mouseReleaseEvent(e);
    }
    QList<QGraphicsView *> viewlist = views();
    if (!viewlist.isEmpty()) {
        viewlist.constFirst()->setDragMode(QGraphicsView::RubberBandDrag);
    }
    emit actionFinished();
}

void GraphicsSceneRectMove::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if ((e->buttons() & Qt::MiddleButton) != 0u) {
        clearTextSelection();
        QList<QGraphicsView *> viewlist = views();
        if (!viewlist.isEmpty()) {
            viewlist.constFirst()->setDragMode(QGraphicsView::ScrollHandDrag);
            m_pan = true;
            e->accept();
            QGraphicsScene::mousePressEvent(e);
            return;
        }
    }
    int xPos = ((int)e->scenePos().x() / m_gridSize) * m_gridSize;
    int yPos = ((int)e->scenePos().y() / m_gridSize) * m_gridSize;
    m_moveStarted = false;
    m_clickPoint = e->scenePos();
    m_resizeMode = m_possibleAction;
    const QList<QGraphicsItem *> list = items(e->scenePos());
    QGraphicsItem *item = nullptr;
    if (m_tool == TITLE_SELECT) {
        QList<QGraphicsView *> viewlist = views();
        if ((e->modifiers() & Qt::ControlModifier) != 0u) {
            clearTextSelection();
            if (!viewlist.isEmpty()) {
                viewlist.constFirst()->setDragMode(QGraphicsView::ScrollHandDrag);
                e->ignore();
                // QGraphicsScene::mousePressEvent(e);
                return;
            }
        } else {
            if (!viewlist.isEmpty()) {
                viewlist.constFirst()->setRubberBandSelectionMode(Qt::IntersectsItemShape);
            }
        }
        bool alreadySelected = false;
        for (QGraphicsItem *g : list) {
            // qDebug() << " - - CHECKING ITEM Z:" << g->zValue() << ", TYPE: " << g->type();
            // check is there is a selected item in list
            if (!(g->flags() & QGraphicsItem::ItemIsSelectable)) {
                continue;
            }
            if (g->zValue() > -1000 /* && g->isSelected()*/) {
                alreadySelected = g->isSelected();
                if (!alreadySelected) {
                    g->setSelected(true);
                }
                item = g;
                break;
            }
        }
        if (item == nullptr || (e->modifiers() != Qt::ShiftModifier && !alreadySelected)) {
            clearTextSelection();
        } else if ((e->modifiers() & Qt::ShiftModifier) != 0u) {
            clearTextSelection(false);
        }
        if ((item != nullptr) && ((item->flags() & QGraphicsItem::ItemIsMovable) != 0)) {
            m_sceneClickPoint = e->scenePos();
            m_selectedItem = item;
            // qCDebug(KDENLIVE_LOG) << "/////////  ITEM TYPE: " << item->type();
            if (item->type() == QGraphicsTextItem::Type) {
                auto *t = static_cast<MyTextItem *>(item);
                if (t->textInteractionFlags() == Qt::TextEditorInteraction) {
                    QGraphicsScene::mousePressEvent(e);
                    return;
                }
                t->setTextInteractionFlags(Qt::NoTextInteraction);
                t->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
                setCursor(Qt::ClosedHandCursor);
            } else if (item->type() == QGraphicsRectItem::Type || item->type() == QGraphicsSvgItem::Type || item->type() == QGraphicsPixmapItem::Type) {
                QRectF r1;
                if (m_selectedItem->type() == QGraphicsRectItem::Type) {
                    r1 = ((QGraphicsRectItem *)m_selectedItem)->rect().normalized();
                } else {
                    r1 = m_selectedItem->boundingRect().normalized();
                }

                r1.translate(m_selectedItem->scenePos());
                switch (m_resizeMode) {
                case BottomRight:
                case Right:
                case Down:
                    m_clickPoint = r1.topLeft();
                    e->accept();
                    break;
                case TopLeft:
                case Left:
                case Up:
                    m_clickPoint = r1.bottomRight();
                    e->accept();
                    break;
                case TopRight:
                    m_clickPoint = r1.bottomLeft();
                    e->accept();
                    break;
                case BottomLeft:
                    m_clickPoint = r1.topRight();
                    e->accept();
                    break;
                default:
                    break;
                }
            }
        }
        QGraphicsScene::mousePressEvent(e);
    } else if (m_tool == TITLE_RECTANGLE) {
        clearTextSelection();
        m_sceneClickPoint = QPointF(xPos, yPos);
        m_selectedItem = nullptr;
        e->ignore();
    } else if (m_tool == TITLE_TEXT) {
        clearTextSelection();
        MyTextItem *textItem = new MyTextItem(i18n("Text"), nullptr);
        yPos = (((int)e->scenePos().y() - (int)(m_fontSize / 2)) / m_gridSize) * m_gridSize;
        textItem->setPos(xPos, yPos);
        addItem(textItem);
        textItem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
        textItem->setFocus(Qt::MouseFocusReason);
        emit newText(textItem);
        m_selectedItem = textItem;
        m_selectedItem->setSelected(true);
        m_createdText = true;
    }
    // qCDebug(KDENLIVE_LOG) << "//////  MOUSE CLICK, RESIZE MODE: " << m_resizeMode;
}

void GraphicsSceneRectMove::clearTextSelection(bool reset)
{
    if ((m_selectedItem != nullptr) && m_selectedItem->type() == QGraphicsTextItem::Type) {
        // disable text editing
        auto *t = static_cast<MyTextItem *>(m_selectedItem);
        t->textCursor().setPosition(0);
        QTextBlock cur = t->textCursor().block();
        t->setTextCursor(QTextCursor(cur));
        t->setTextInteractionFlags(Qt::NoTextInteraction);
    }
    if (reset) {
        m_selectedItem = nullptr;
        clearSelection();
    }
}

void GraphicsSceneRectMove::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    QList<QGraphicsView *> viewlist = views();
    if (viewlist.isEmpty()) {
        e->ignore();
        return;
    }
    QGraphicsView *view = viewlist.constFirst();
    if (m_pan) {
        QPoint diff = e->lastScreenPos() - e->screenPos();
        view->horizontalScrollBar()->setValue(view->horizontalScrollBar()->value() + diff.x());
        view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() + diff.y());
        e->accept();
        QGraphicsScene::mouseMoveEvent(e);
        return;
    }
    if (e->buttons() != Qt::NoButton && !m_moveStarted) {
        if ((view->mapFromScene(e->scenePos()) - view->mapFromScene(m_clickPoint)).manhattanLength() < QApplication::startDragDistance()) {
            e->ignore();
            return;
        }
        m_moveStarted = true;
    }
    if ((m_selectedItem != nullptr) && ((e->buttons() & Qt::LeftButton) != 0u)) {
        if (m_selectedItem->type() == QGraphicsRectItem::Type || m_selectedItem->type() == QGraphicsSvgItem::Type ||
            m_selectedItem->type() == QGraphicsPixmapItem::Type) {
            QRectF newrect;
            if (m_selectedItem->type() == QGraphicsRectItem::Type) {
                newrect = ((QGraphicsRectItem *)m_selectedItem)->rect();
            } else {
                newrect = m_selectedItem->boundingRect();
            }
            int xPos = ((int)e->scenePos().x() / m_gridSize) * m_gridSize;
            int yPos = ((int)e->scenePos().y() / m_gridSize) * m_gridSize;
            QPointF newpoint(xPos, yPos);
            switch (m_resizeMode) {
            case BottomRight:
            case BottomLeft:
            case TopRight:
            case TopLeft:
                newrect = QRectF(m_clickPoint, newpoint).normalized();
                break;
            case Up:
                newrect = QRectF(m_clickPoint, QPointF(m_clickPoint.x() - newrect.width(), newpoint.y())).normalized();
                break;
            case Down:
                newrect = QRectF(m_clickPoint, QPointF(newrect.width() + m_clickPoint.x(), newpoint.y())).normalized();
                break;
            case Right:
                newrect = QRectF(m_clickPoint, QPointF(newpoint.x(), m_clickPoint.y() + newrect.height())).normalized();
                break;
            case Left:
                newrect = QRectF(m_clickPoint, QPointF(newpoint.x(), m_clickPoint.y() - newrect.height())).normalized();
                break;
            default:
                break;
            }

            if (m_selectedItem->type() == QGraphicsRectItem::Type && m_resizeMode != NoResize) {
                auto *gi = static_cast<MyRectItem *>(m_selectedItem);
                // Resize using aspect ratio
                if (!m_selectedItem->data(0).isNull()) {
                    // we want to keep aspect ratio
                    double hRatio = (double)newrect.width() / m_selectedItem->data(0).toInt();
                    double vRatio = (double)newrect.height() / m_selectedItem->data(1).toInt();
                    if (hRatio < vRatio) {
                        newrect.setHeight(m_selectedItem->data(1).toInt() * hRatio);
                    } else {
                        newrect.setWidth(m_selectedItem->data(0).toInt() * vRatio);
                    }
                }
                gi->setPos(newrect.topLeft());
                gi->setRect(QRectF(QPointF(), newrect.bottomRight() - newrect.topLeft()));
                return;
            }
            QGraphicsScene::mouseMoveEvent(e);
        } else if (m_selectedItem->type() == QGraphicsTextItem::Type) {
            auto *t = static_cast<MyTextItem *>(m_selectedItem);
            if ((t->textInteractionFlags() & static_cast<int>((Qt::TextEditorInteraction) != 0)) != 0) {
                QGraphicsScene::mouseMoveEvent(e);
                return;
            }
            QGraphicsScene::mouseMoveEvent(e);
            m_sceneClickPoint = e->scenePos();
        }
        emit itemMoved();
    } else if (m_tool == TITLE_SELECT) {
        QPointF p = e->scenePos();
        p += QPoint(-2, -2);
        m_resizeMode = NoResize;
        bool itemFound = false;
        QList<QGraphicsItem *> list = items(QRectF(p, QSizeF(4, 4)).toRect());
        for (const QGraphicsItem *g : qAsConst(list)) {
            if (!(g->flags() & QGraphicsItem::ItemIsSelectable)) {
                continue;
            }
            if ((g->type() == QGraphicsSvgItem::Type || g->type() == QGraphicsPixmapItem::Type) && g->zValue() > -1000) {
                // image or svg item
                setCursor(Qt::OpenHandCursor);
                itemFound = true;
                break;
            } else if (g->type() == QGraphicsRectItem::Type && g->zValue() > -1000) {
                if (view == nullptr) {
                    continue;
                }
                QRectF r1 = ((const QGraphicsRectItem *)g)->rect().normalized();
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
                QFontMetrics metrics(font());
                int box = metrics.lineSpacing() / 2;
                mouseArea.addRect(viewPos.x() - box, viewPos.y() - box, 2 * box, 2 * box);

                // Check for collisions between the mouse and the borders
                if (mouseArea.contains(r.point(0))) {
                    m_possibleAction = TopLeft;
                    setCursor(Qt::SizeFDiagCursor);
                } else if (mouseArea.contains(r.point(2))) {
                    m_possibleAction = BottomRight;
                    setCursor(Qt::SizeFDiagCursor);
                } else if (mouseArea.contains(r.point(1))) {
                    m_possibleAction = TopRight;
                    setCursor(Qt::SizeBDiagCursor);
                } else if (mouseArea.contains(r.point(3))) {
                    m_possibleAction = BottomLeft;
                    setCursor(Qt::SizeBDiagCursor);
                } else if (top.intersects(mouseArea)) {
                    m_possibleAction = Up;
                    setCursor(Qt::SizeVerCursor);
                } else if (bottom.intersects(mouseArea)) {
                    m_possibleAction = Down;
                    setCursor(Qt::SizeVerCursor);
                } else if (right.intersects(mouseArea)) {
                    m_possibleAction = Right;
                    setCursor(Qt::SizeHorCursor);
                } else if (left.intersects(mouseArea)) {
                    m_possibleAction = Left;
                    setCursor(Qt::SizeHorCursor);
                } else {
                    setCursor(Qt::OpenHandCursor);
                    m_possibleAction = NoResize;
                }
            }
            break;
        }
        if (!itemFound) {
            m_possibleAction = NoResize;
            setCursor(Qt::ArrowCursor);
        }
        QGraphicsScene::mouseMoveEvent(e);
    } else if (m_tool == TITLE_RECTANGLE && ((e->buttons() & Qt::LeftButton) != 0u)) {
        if (m_selectedItem == nullptr) {
            // create new rect item
            QRectF r(0, 0, e->scenePos().x() - m_sceneClickPoint.x(), e->scenePos().y() - m_sceneClickPoint.y());
            r = r.normalized();
            auto *rect = new MyRectItem();
            rect->setRect(QRectF(0, 0, r.width(), r.height()));
            addItem(rect);
            m_selectedItem = rect;
            m_selectedItem->setPos(m_sceneClickPoint);
            m_selectedItem->setSelected(true);
            emit newRect(rect);
            m_selectedItem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
            m_resizeMode = BottomRight;
            QGraphicsScene::mouseMoveEvent(e);
        }
    }
}

void GraphicsSceneRectMove::wheelEvent(QGraphicsSceneWheelEvent *wheelEvent)
{
    if (wheelEvent->modifiers() == Qt::ControlModifier) {
        QList<QGraphicsView *> viewlist = views();
        ////qCDebug(KDENLIVE_LOG) << wheelEvent->delta() << ' ' << zoom;
        if (!viewlist.isEmpty()) {
            if (wheelEvent->delta() > 0) {
                emit sceneZoom(true);
            } else {
                emit sceneZoom(false);
            }
        }
    } else {
        wheelEvent->setAccepted(false);
    }
}

void GraphicsSceneRectMove::setScale(double s)
{
    if (m_zoom < 1.0 / 7.0 && s < 1.0) {
        return;
    }
    if (m_zoom > 10.0 / 7.9 && s > 1.0) {
        return;
    }
    QList<QGraphicsView *> viewlist = views();
    if (!viewlist.isEmpty()) {
        viewlist[0]->scale(s, s);
        m_zoom = m_zoom * s;
    }
    ////qCDebug(KDENLIVE_LOG)<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setZoom(double s)
{
    QList<QGraphicsView *> viewlist = views();
    if (!viewlist.isEmpty()) {
        viewlist[0]->resetTransform();
        viewlist[0]->scale(s, s);
        m_zoom = s;
    }

    ////qCDebug(KDENLIVE_LOG)<<"//////////  ZOOM: "<<zoom;
}

void GraphicsSceneRectMove::setCursor(const QCursor &c)
{
    const QList<QGraphicsView *> l = views();
    for (QGraphicsView *v : l) {
        v->setCursor(c);
    }
}

void GraphicsSceneRectMove::slotUpdateFontSize(int s)
{
    m_fontSize = s;
}

void GraphicsSceneRectMove::drawForeground(QPainter *painter, const QRectF &rect)
{
    // draw the grid if needed
    if (m_gridSize <= 1) {
        return;
    }

    QPen pen(QColor(255, 0, 0, 100));
    painter->setPen(pen);

    qreal left = int(rect.left()) - (int(rect.left()) % m_gridSize);
    qreal top = int(rect.top()) - (int(rect.top()) % m_gridSize);
    QVector<QPointF> points;
    for (qreal x = left; x < rect.right(); x += m_gridSize) {
        for (qreal y = top; y < rect.bottom(); y += m_gridSize) {
            points.append(QPointF(x, y));
        }
    }
    painter->drawPoints(points.data(), points.size());
}

int GraphicsSceneRectMove::gridSize() const
{
    return m_gridSize;
}

void GraphicsSceneRectMove::slotUseGrid(bool enableGrid)
{
    m_gridSize = enableGrid ? 20 : 1;
}

void GraphicsSceneRectMove::addNewItem(QGraphicsItem *item)
{
    clearSelection();
    addItem(item);
    item->setSelected(true);
    m_selectedItem = item;
}
