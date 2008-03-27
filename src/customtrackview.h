/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef CUSTOMTRACKVIEW_H
#define CUSTOMTRACKVIEW_H

#include <QGraphicsView>
#include <QGraphicsItemAnimation>
#include <QTimeLine>
#include <QMenu>

#include <KUndoStack>

#include "kdenlivedoc.h"

class ClipItem;
class AbstractClipItem;
class Transition;

class CustomTrackView : public QGraphicsView {
    Q_OBJECT

public:
    CustomTrackView(KdenliveDoc *doc, QGraphicsScene * projectscene, QWidget *parent = 0);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    void addTrack(TrackInfo type);
    void removeTrack();
    int cursorPos();
    void checkAutoScroll();
    void moveClip(const ItemInfo start, const ItemInfo end);
    /** move transition, startPos = (old start, old end), endPos = (new start, new end) */
    void moveTransition(const ItemInfo start, const ItemInfo end);
    void resizeClip(const ItemInfo start, const ItemInfo end);
    void addClip(QDomElement xml, int clipId, ItemInfo info);
    void deleteClip(ItemInfo info);
    void setScale(double scaleFactor);
    void deleteClip(int clipId);
    void slotAddEffect(QDomElement effect, GenTime pos, int track);
    void addEffect(int track, GenTime pos, QDomElement effect);
    void deleteEffect(int track, GenTime pos, QDomElement effect);
    void updateEffect(int track, GenTime pos, QDomElement effect);
    void addTransition(ItemInfo transitionInfo, int endTrack, QDomElement params);
    void deleteTransition(ItemInfo transitionInfo, int endTrack, QDomElement params);
    void updateTransition(int track, GenTime pos,  QDomElement oldTransition, QDomElement transition);
    void moveTransition(GenTime oldpos, GenTime newpos);
    void activateMonitor();
    int duration() const;
    void deleteSelectedClips();
    void setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition);
    void checkTrackHeight();
    QList <TrackInfo> tracksList() const;

public slots:
    void setCursorPos(int pos, bool seek = true);
    void updateCursorPos();
    void slotDeleteEffect(ClipItem *clip, QDomElement effect);
    void slotChangeEffectState(ClipItem *clip, QDomElement effect, bool disable);
    void slotUpdateClipEffect(ClipItem *clip, QDomElement oldeffect, QDomElement effect);
    void slotRefreshEffects(ClipItem *clip);
    void setDuration(int duration);
    void slotAddTransition(ClipItem* clip, ItemInfo transitionInfo, int endTrack, QDomElement transition = QDomElement());
    void slotTransitionUpdated(QDomElement, QDomElement);
    void slotSwitchTrackAudio(int ix);
    void slotSwitchTrackVideo(int ix);

protected:
    virtual void drawBackground(QPainter * painter, const QRectF & rect);
    //virtual void drawForeground ( QPainter * painter, const QRectF & rect );
    virtual void dragEnterEvent(QDragEnterEvent * event);
    virtual void dragMoveEvent(QDragMoveEvent * event);
    virtual void dragLeaveEvent(QDragLeaveEvent * event);
    virtual void dropEvent(QDropEvent * event);
    virtual void wheelEvent(QWheelEvent * e);
    virtual QStringList mimeTypes() const;
    virtual Qt::DropActions supportedDropActions() const;
    virtual void resizeEvent(QResizeEvent * event);

private:
    uint m_tracksHeight;
    int m_projectDuration;
    int m_cursorPos;
    ClipItem *m_dropItem;
    KdenliveDoc *m_document;
    void addItem(DocClipBase *clip, QPoint pos);
    QGraphicsLineItem *m_cursorLine;
    ItemInfo m_dragItemInfo;
    OPERATIONTYPE m_operationMode;
    OPERATIONTYPE m_moveOpMode;
    AbstractClipItem *m_dragItem;
    KUndoStack *m_commandStack;
    QGraphicsItem *m_visualTip;
    QGraphicsItemAnimation *m_animation;
    QTimeLine *m_animationTimer;
    QColor m_tipColor;
    QPen m_tipPen;
    double m_scale;
    QPoint m_clickPoint;
    QList <GenTime> m_snapPoints;
    void updateSnapPoints(AbstractClipItem *selected);
    double getSnapPointForPos(double pos);
    ClipItem *getClipItemAt(int pos, int track);
    ClipItem *getClipItemAt(GenTime pos, int track);
    Transition *getTransitionItemAt(int pos, int track);
    Transition *getTransitionItemAt(GenTime pos, int track);
    void checkScrolling();
    /** Should we auto scroll while playing (keep in sync with KdenliveSettings::autoscroll() */
    bool m_autoScroll;
    void displayContextMenu(QPoint pos, ClipItem *clip = NULL);
    QMenu *m_timelineContextMenu;
    QMenu *m_timelineContextClipMenu;
    QMenu *m_timelineContextTransitionMenu;
    QList <TrackInfo> m_tracksList;
    /** Get the index of the video track that is just below current track */
    int getPreviousVideoTrack(int track);


signals:
    void cursorMoved(int, int);
    void zoomIn();
    void zoomOut();
    void mousePosition(int);
    void clipItemSelected(ClipItem*);
    void transitionItemSelected(Transition*);
    void activateDocumentMonitor();
    void trackHeightChanged();
};

#endif

