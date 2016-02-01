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

#include "definitions.h"
#include "gentime.h"

#include "mlt++/MltProperties.h"
#include "mlt++/MltAnimation.h"

#include <QGraphicsRectItem>
#include <QGraphicsWidget>

class CustomTrackScene;
class QGraphicsSceneMouseEvent;


class AbstractClipItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ rect WRITE setRect)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:

    enum KEYFRAMETYPE {
        NoKeyframe = 0,
        SimpleKeyframe,
        NormalKeyframe,
        GeometryKeyframe,
        AnimatedKeyframe
    };

    AbstractClipItem(const ItemInfo &info, const QRectF& rect, double fps);
    virtual ~ AbstractClipItem();
    void updateSelectedKeyFrame();

    /** @brief Move the selected keyframe (does not influence the effect, only the display in timeline).
    * @param pos new Position
    * @param value new Value */
    void updateKeyFramePos(int frame, const double y);
    int checkForSingleKeyframe();
    int addKeyFrame(const GenTime &pos, const double y);
    bool hasKeyFrames();
    int editedKeyFramePos() const;
    int selectedKeyFramePos() const;
    double editedKeyFrameValue();
    /** @brief Returns the number of keyframes the selected effect has. */
    int keyFrameNumber();
    ItemInfo info() const;
    CustomTrackScene* projectScene();
    void updateRectGeometry();
    void updateItem(int track);
    void setItemLocked(bool locked);
    bool isItemLocked() const;
    void closeAnimation();

    virtual OperationType operationMode(const QPointF &pos) = 0;
    virtual GenTime startPos() const ;
    virtual GenTime endPos() const ;
    virtual int track() const ;
    virtual GenTime cropStart() const ;
    virtual GenTime cropDuration() const ;
    /** @brief Return the current item's height */
    static int itemHeight();
    /** @brief Return the current item's vertical offset
     *         For example transitions are drawn at 1/3 of track height */
    static int itemOffset();

    /** @brief Resizes the clip from the start.
    * @param posx Absolute position of new in point
    * @param hasSizeLimit (optional) Whether the clip has a maximum size */
    virtual void resizeStart(int posx, bool hasSizeLimit = true, bool emitChange = true);

    /** @brief Resizes the clip from the end.
    * @param posx Absolute position of new out point */
    virtual void resizeEnd(int posx, bool emitChange = true);
    virtual double fps() const;
    virtual void updateFps(double fps);
    virtual GenTime maxDuration() const;
    virtual void setCropStart(const GenTime &pos);

    /** @brief Set this clip as the main selected clip (or not). */
    void setMainSelectedClip(bool selected);
    /** @brief Is this clip selected as the main clip. */
    bool isMainSelectedClip();
    
protected:
    ItemInfo m_info;
    /** The position of the current keyframe when it has moved */
    int m_editedKeyframe;
    /** The position of the current keyframe before it was moved */
    int m_selectedKeyframe;
    /*    GenTime m_cropStart;
        GenTime m_cropDuration;
        GenTime m_startPos;*/
    GenTime m_maxDuration;
    KEYFRAMETYPE m_keyframeType;
    Mlt::Properties m_keyProperties;
    Mlt::Animation m_keyAnim;
    double m_keyframeDefault;
    double m_keyframeMin;
    double m_keyframeMax;
    double m_keyframeFactor;
    int m_handleSize;
    int m_visibleParam;
    double m_fps;
    /** @brief True if this is the last clip the user selected */
    bool m_isMainSelectedClip;
    /** @brief Draw the keyframes of a clip
      * @param painter The painter device for the clip
      */
    double keyframeUnmap(double y);
    double keyframeMap(double value);
    QPointF keyframeMap(int frame, double value);
    QPointF keyframePoint(int index);
    void drawKeyFrames(QPainter *painter, const QTransform &transformation);
    int mouseOverKeyFrames(QPointF pos, double maxOffset);
    void mousePressEvent(QGraphicsSceneMouseEvent * event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent * event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent * event);
      int trackForPos(int position);
      int posForTrack(int track);
};

#endif
