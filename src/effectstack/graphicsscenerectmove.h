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
    explicit MyQGraphicsEffect(QObject *parent = Q_NULLPTR);
    void setOffset(int xOffset, int yOffset, int blur);
    void setShadow(QImage image);
protected:
    void draw(QPainter *painter);
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
    MyTextItem(const QString &, QGraphicsItem *parent = Q_NULLPTR);
    void setAlignment(Qt::Alignment alignment);
    /** @brief returns an extended bounding containing shadow */
    QRectF boundingRect() const;
    /** @brief returns the normal bounding rect around text */
    QRectF baseBoundingRect() const;
    Qt::Alignment alignment() const;
    void updateShadow(bool enabled, int blur, int xoffset, int yoffset, QColor color);
    QStringList shadowInfo() const;
    void loadShadow(QStringList info);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *evt);

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    Qt::Alignment m_alignment;
    QPoint m_shadowOffset;
    int m_shadowBlur;
    QColor m_shadowColor;
    MyQGraphicsEffect *m_shadowEffect;
    void updateShadow();
    void blurShadow(QImage &image, int radius);

public slots:
    void updateGeometry(int, int, int);
    void updateGeometry();
};

class MyRectItem: public QGraphicsRectItem
{
public:
    explicit MyRectItem(QGraphicsItem *parent = 0);
    void setRect(const QRectF & rectangle);
protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);
private:
    QRectF m_rect;
};

class MyPixmapItem: public QGraphicsPixmapItem
{
public:
    MyPixmapItem(const QPixmap &pixmap, QGraphicsItem *parent = 0);
protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);
};

class MySvgItem: public QGraphicsSvgItem
{
public:
    MySvgItem(const QString &fileName = QString(), QGraphicsItem *parent = 0);
protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);
};

class GraphicsSceneRectMove: public QGraphicsScene
{
    Q_OBJECT
public:
    explicit GraphicsSceneRectMove(QObject* parent = 0);
    void setSelectedItem(QGraphicsItem *item);
    void setScale(double s);
    void setZoom(double s);
    void setTool(TITLETOOL tool);
    TITLETOOL tool() const;
    void clearTextSelection();
    int gridSize() const;
    void addNewItem(QGraphicsItem *item);

public slots:
    void slotUpdateFontSize(int s);
    void slotUseGrid(bool enableGrid);

protected:
    virtual void keyPressEvent(QKeyEvent * keyEvent);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e);
    /** @brief Resizes and moves items */
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    virtual void wheelEvent(QGraphicsSceneWheelEvent * wheelEvent);
    void drawForeground(QPainter *painter, const QRectF &rect);

private:
    void setCursor(QCursor);
    double m_zoom;
    QGraphicsItem* m_selectedItem;
    resizeModes m_resizeMode;
    resizeModes m_possibleAction;
    QPointF m_sceneClickPoint;
    TITLETOOL m_tool;
    QPointF m_clickPoint;
    int m_fontSize;
    int m_gridSize;
    bool m_createdText;
    bool m_moveStarted;

signals:
    void itemMoved();
    void sceneZoom(bool);
    void newRect(QGraphicsRectItem *);
    void newText(MyTextItem *);
    void actionFinished();
    void doubleClickEvent();
};

#endif
