/***************************************************************************
 *   Copyright (C) 2008 by Marco Gittler (g.marco@freenet.de)              *
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

#ifndef ABSTRACTCLIPITEM
#define ABSTRACTCLIPITEM

#include <QGraphicsRectItem>
#include <QGraphicsWidget>

#if QT_VERSION >= 0x040600
#include <QPropertyAnimation>
#endif

#include "definitions.h"
#include "gentime.h"

class CustomTrackScene;
class QGraphicsSceneMouseEvent;

class AbstractClipItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
#if QT_VERSION >= 0x040600
    Q_PROPERTY(QRectF rect READ rect WRITE setRect)
#endif

public:
    AbstractClipItem(const ItemInfo info, const QRectF& rect, double fps);
    virtual ~ AbstractClipItem();
    void updateSelectedKeyFrame();

    /** @brief Move the selected keyframe (does not influence the effect, only the display in timeline).
    * @param pos new Position
    * @param value new Value */
    void updateKeyFramePos(const GenTime pos, const double value);
    int addKeyFrame(const GenTime pos, const double value);
    bool hasKeyFrames() const;
    int editedKeyFramePos() const;
    int selectedKeyFramePos() const;
    double selectedKeyFrameValue() const;
    double editedKeyFrameValue() const;
    double keyFrameFactor() const;
    ItemInfo info() const;
    CustomTrackScene* projectScene();
    void updateRectGeometry();
    void updateItem();
    void setItemLocked(bool locked);
    bool isItemLocked() const;
    void closeAnimation();

    virtual  OPERATIONTYPE operationMode(QPointF pos) = 0;
    virtual GenTime startPos() const ;
    virtual void setTrack(int track);
    virtual GenTime endPos() const ;
    virtual int defaultZValue() const ;
    virtual int track() const ;
    virtual GenTime cropStart() const ;
    virtual GenTime cropDuration() const ;

    /** @brief Resizes the clip from the start.
    * @param posx Absolute position of new in point
    * @param hasSizeLimit (optional) Whether the clip has a maximum size */
    virtual void resizeStart(int posx, bool hasSizeLimit = true);

    /** @brief Resizes the clip from the end.
    * @param posx Absolute position of new out point */
    virtual void resizeEnd(int posx);
    virtual double fps() const;
    virtual void updateFps(double fps);
    virtual GenTime maxDuration() const;
    virtual void setCropStart(GenTime pos);

protected:
    ItemInfo m_info;
//    int m_track;
    /** The position of the current keyframe when it has moved */
    int m_editedKeyframe;
    /** The position of the current keyframe before it was moved */
    int m_selectedKeyframe;
    /*    GenTime m_cropStart;
        GenTime m_cropDuration;
        GenTime m_startPos;*/
    GenTime m_maxDuration;
    QMap <int, int> m_keyframes;
    double m_keyframeFactor;
    double m_keyframeDefault;
    double m_fps;
    //QRect visibleRect();
    void drawKeyFrames(QPainter *painter, QRectF exposedRect);
    int mouseOverKeyFrames(QPointF pos, double maxOffset);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent * event);

private:
#if QT_VERSION >= 0x040600
    QPropertyAnimation *m_closeAnimation;
#endif
};

#endif
