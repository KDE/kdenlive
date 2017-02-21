/***************************************************************************
 *   copyright (C) 2008 by Marco Gittler (g.marco@freenet.de)                                  *
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

#ifndef GRAPHICSSCENERECTMOVE_H
#define GRAPHICSSCENERECTMOVE_H

#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsSvgItem>
#include <QGraphicsEffect>

enum resizeModes {NoResize = 0, TopLeft, BottomLeft, TopRight, BottomRight, Left, Right, Up, Down};
enum TITLETOOL { TITLE_SELECT = 0, TITLE_RECTANGLE = 1, TITLE_TEXT = 2, TITLE_IMAGE = 3 };

class MyQGraphicsEffect: public QGraphicsEffect
{
public:
    explicit MyQGraphicsEffect(QObject *parent = nullptr);
    void setOffset(int xOffset, int yOffset, int blur);
    void setShadow(const QImage &image);
protected:
    void draw(QPainter *painter) Q_DECL_OVERRIDE;
private:
    int m_xOffset;
    int m_yOffset;
    int m_blur;
    QImage m_shadow;
};

class MyTextItem: public QGraphicsTextItem
{
    Q_OBJECT
public:
    MyTextItem(const QString &, QGraphicsItem *parent = nullptr);
    void setAlignment(Qt::Alignment alignment);
    /** @brief returns an extended bounding containing shadow */
    QRectF boundingRect() const Q_DECL_OVERRIDE;
    /** @brief returns the normal bounding rect around text */
    QRectF baseBoundingRect() const;
    Qt::Alignment alignment() const;
    void updateShadow(bool enabled, int blur, int xoffset, int yoffset, QColor color);
    QStringList shadowInfo() const;
    void loadShadow(const QStringList &info);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *evt) Q_DECL_OVERRIDE;
    void setTextColor(const QColor &col);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) Q_DECL_OVERRIDE;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *w) Q_DECL_OVERRIDE;

private:
    Qt::Alignment m_alignment;
    QPoint m_shadowOffset;
    int m_shadowBlur;
    QColor m_shadowColor;
    QPainterPath m_path;
    MyQGraphicsEffect *m_shadowEffect;
    void updateShadow();
    void blurShadow(QImage &image, int radius);
    void refreshFormat();

public slots:
    void updateGeometry(int, int, int);
    void updateGeometry();
};

class MyRectItem: public QGraphicsRectItem
{
public:
    explicit MyRectItem(QGraphicsItem *parent = nullptr);
    void setRect(const QRectF &rectangle);
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) Q_DECL_OVERRIDE;
private:
    QRectF m_rect;
};

class MyPixmapItem: public QGraphicsPixmapItem
{
public:
    MyPixmapItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) Q_DECL_OVERRIDE;
};

class MySvgItem: public QGraphicsSvgItem
{
public:
    MySvgItem(const QString &fileName = QString(), QGraphicsItem *parent = nullptr);
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) Q_DECL_OVERRIDE;
};

class GraphicsSceneRectMove: public QGraphicsScene
{
    Q_OBJECT
public:
    explicit GraphicsSceneRectMove(QObject *parent = nullptr);
    void setSelectedItem(QGraphicsItem *item);
    void setScale(double s);
    void setZoom(double s);
    void setTool(TITLETOOL tool);
    TITLETOOL tool() const;
    /** @brief Get out of text edit mode. If reset is true, we also unselect all items */
    void clearTextSelection(bool reset = true);
    int gridSize() const;
    void addNewItem(QGraphicsItem *item);

public slots:
    void slotUpdateFontSize(int s);
    void slotUseGrid(bool enableGrid);

protected:
    void keyPressEvent(QKeyEvent *keyEvent) Q_DECL_OVERRIDE;
    void mousePressEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) Q_DECL_OVERRIDE;
    /** @brief Resizes and moves items */
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void wheelEvent(QGraphicsSceneWheelEvent *wheelEvent) Q_DECL_OVERRIDE;
    void drawForeground(QPainter *painter, const QRectF &rect) Q_DECL_OVERRIDE;

private:
    void setCursor(const QCursor &);
    double m_zoom;
    QGraphicsItem *m_selectedItem;
    resizeModes m_resizeMode;
    resizeModes m_possibleAction;
    QPointF m_sceneClickPoint;
    TITLETOOL m_tool;
    QPointF m_clickPoint;
    int m_fontSize;
    int m_gridSize;
    bool m_createdText;
    bool m_moveStarted;
    bool m_pan;

signals:
    void itemMoved();
    void sceneZoom(bool);
    void newRect(QGraphicsRectItem *);
    void newText(MyTextItem *);
    void actionFinished();
    void doubleClickEvent();
};

#endif
