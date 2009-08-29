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
#include "definitions.h"
#include "gentime.h"

class CustomTrackScene;
class QGraphicsSceneMouseEvent;

class AbstractClipItem : public QObject , public QGraphicsRectItem
{
    Q_OBJECT
public:
    AbstractClipItem(const ItemInfo info, const QRectF& rect, double fps);
    void updateSelectedKeyFrame();
    void updateKeyFramePos(const GenTime pos, const double value);
    void addKeyFrame(const GenTime pos, const double value);
    bool hasKeyFrames() const;
    int selectedKeyFramePos() const;
    double selectedKeyFrameValue() const;
    double keyFrameFactor() const;
    ItemInfo info() const;
    CustomTrackScene* projectScene();
    void updateRectGeometry();
    void updateItem();
    void setItemLocked(bool locked);
    bool isItemLocked() const;

    virtual  OPERATIONTYPE operationMode(QPointF pos) = 0;
    virtual GenTime startPos() const ;
    virtual void setTrack(int track);
    virtual GenTime endPos() const ;
    virtual int track() const ;
    virtual GenTime cropStart() const ;
    virtual GenTime cropDuration() const ;
    virtual void resizeStart(int posx, double speed = 1.0);
    virtual void resizeEnd(int posx, double speed = 1.0);
    virtual double fps() const;
    virtual GenTime maxDuration() const;
    virtual void setCropStart(GenTime pos);

protected:
    int m_track;
    int m_editedKeyframe;
    int m_selectedKeyframe;
    GenTime m_cropStart;
    GenTime m_cropDuration;
    GenTime m_startPos;
    GenTime m_maxDuration;
    QMap <int, int> m_keyframes;
    double m_keyframeFactor;
    double m_keyframeDefault;
    double m_fps;
    //QRect visibleRect();
    void drawKeyFrames(QPainter *painter, QRectF exposedRect);
    int mouseOverKeyFrames(QPointF pos);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent * event);
};

#endif
