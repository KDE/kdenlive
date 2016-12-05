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

#ifndef ABSTRACTCLIPITEM_H
#define ABSTRACTCLIPITEM_H

#include "keyframeview.h"
#include "definitions.h"
#include "gentime.h"

#include "mlt++/MltProperties.h"
#include "mlt++/MltAnimation.h"

#include <QGraphicsRectItem>
#include <QGraphicsWidget>
#include <QTimer>

class CustomTrackScene;
class QGraphicsSceneMouseEvent;

class AbstractClipItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ rect WRITE setRect)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:

    AbstractClipItem(const ItemInfo &info, const QRectF &rect, double fps);
    virtual ~ AbstractClipItem();
    ItemInfo info() const;
    CustomTrackScene *projectScene();
    void updateRectGeometry();
    void updateItem(int track);
    void setItemLocked(bool locked);
    bool isItemLocked() const;
    void closeAnimation();

    virtual OperationType operationMode(const QPointF &pos, Qt::KeyboardModifiers modifiers) = 0;
    virtual void updateKeyframes(const QDomElement &effect) = 0;
    virtual GenTime startPos() const;
    virtual GenTime endPos() const;
    virtual int track() const;
    virtual GenTime cropStart() const;
    virtual GenTime cropDuration() const;
    /** @brief Return the current item's height */
    static int itemHeight();
    /** @brief Return the current item's vertical offset
     *         For example transitions are drawn at 1/3 of track height */
    static int itemOffset();

    /** @brief Resizes the clip from the start.
    * @param posx Absolute position of new in point
    * @param hasSizeLimit (optional) Whether the clip has a maximum size */
    virtual void resizeStart(int posx, bool hasSizeLimit = true, bool emitChange = true);
    void updateKeyFramePos(int frame, const double y);
    int selectedKeyFramePos() const;
    int originalKeyFramePos() const;
    void prepareKeyframeMove();
    int keyframesCount();
    double editedKeyFrameValue();
    double getKeyFrameClipHeight(const double y);
    QAction *parseKeyframeActions(const QList<QAction *> &list);
    void editKeyframeType(const QDomElement &effect, int type);
    void attachKeyframeToEnd(const QDomElement &effect, bool attach);
    bool isAttachedToEnd() const;

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

    void insertKeyframe(ProfileInfo profile, QDomElement effect, int pos, double val, bool defaultValue = false);
    void movedKeyframe(QDomElement effect, int newpos, int oldpos = -1, double value = -1);
    void removeKeyframe(QDomElement effect, int frame);

private slots:
    void doUpdate(const QRectF &r);
    void slotSelectItem();

protected:
    ItemInfo m_info;
    GenTime m_maxDuration;
    int m_visibleParam;
    int m_selectedEffect;
    QTimer m_selectionTimer;
    double m_fps;
    /** @brief True if this is the last clip the user selected */
    bool m_isMainSelectedClip;
    KeyframeView m_keyframeView;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    int trackForPos(int position);
    int posForTrack(int track);
    bool resizeGeometries(QDomElement effect, int width, int height, int previousDuration, int start, int duration, int cropstart);
    QString resizeAnimations(QDomElement effect, int previousDuration, int start, int duration, int cropstart);
    bool switchKeyframes(QDomElement param, int in, int oldin, int out, int oldout);

signals:
    void selectItem(AbstractClipItem *);
};

#endif
