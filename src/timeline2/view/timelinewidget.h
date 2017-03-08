/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include "mltcontroller/bincontroller.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include <QQuickWidget>

class BinController;
class ThumbnailProvider;
class KActionCollection;

class TimelineWidget : public QQuickWidget
{
    Q_OBJECT
    /* @brief holds a list of currently selected clips (list of clipId's)
     */
    Q_PROPERTY(QList<int> selection READ selection WRITE setSelection NOTIFY selectionChanged)
    /* @brief holds the timeline zoom factor
     */
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    /* @brief holds the current project duration
     */
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    /* @brief holds the current timeline position
     */
    Q_PROPERTY(int position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(bool snap READ snap NOTIFY snapChanged)
    Q_PROPERTY(bool ripple READ ripple NOTIFY rippleChanged)
    Q_PROPERTY(bool scrub READ scrub NOTIFY scrubChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails NOTIFY showThumbnailsChanged)
    Q_PROPERTY(bool showAudioThumbnails READ showAudioThumbnails NOTIFY showAudioThumbnailsChanged)

public:
    TimelineWidget(KActionCollection *actionCollection, BinController *binController, std::weak_ptr<DocUndoStack> undoStack, QWidget *parent = Q_NULLPTR);
    /* @brief Sets the list of currently selected clips
       @param selection is the list of id's
       @param trackIndex is current clip's track
       @param isMultitrack is true if we want to select the whole tractor (currently unused)
     */
    void setSelection(QList<int> selection = QList<int>(), int trackIndex = -1, bool isMultitrack = false);
    /* @brief Get the list of currenly selected clip id's
     */
    QList<int> selection() const;
    Q_INVOKABLE bool isMultitrackSelected() const { return m_selection.isMultitrackSelected; }
    Q_INVOKABLE int selectedTrack() const { return m_selection.selectedTrack; }
    /* @brief Add a clip id to current selection
     */
    Q_INVOKABLE void addSelection(int newSelection);
    /* @brief returns current timeline's zoom factor
     */
    Q_INVOKABLE double scaleFactor() const;
    /* @brief set current timeline's zoom factor
     */
    Q_INVOKABLE void setScaleFactor(double scale);
    /* @brief requests a clip move
       @param cid is the clip's id
       @param tid is the destination track
       @param position is the requested position on track
       @param logUndo is true if the operation should be stored in the undo stack
     */
    Q_INVOKABLE bool moveClip(int cid, int tid, int position, bool logUndo = true);
    /* @brief Ask if a clip move should be allowed (enough blank space on track)
       @param cid is the clip's id
       @param tid is the destination track
       @param position is the requested position on track
     */
    Q_INVOKABLE bool allowMoveClip(int cid, int tid, int position);
        /* @brief Ask if a clip move should be allowed (enough blank space on track), without using clip id
       @param tid is the destination track
       @param position is the requested position on track
       @param duration is the clip's duration
     */
    Q_INVOKABLE bool allowMove(int tid, int position, int duration);
    /* @brief Request best appropriate move (accounting snap / other clips)
       @param cid is the clip's id
       @param tid is the destination track
       @param position is the requested position on track
     */
    Q_INVOKABLE int suggestClipMove(int cid, int tid, int position);
    /* @brief Request a clip resize based on a length offset
       @param cid is the clip's id
       @param delta is the offset compared to original length
       @param right is true if resizing the end of the clip (false when resizing start)
       @param logUndo is true if the operation should be stored in the undo stack
     */
    Q_INVOKABLE bool trimClip(int cid, int delta, bool right, bool logUndo = true);
    /* @brief Request a clip resize
       @param cid is the clip's id
       @param duration is the new clip's duration
       @param right is true if resizing the end of the clip (false when resizing start)
       @param logUndo is true if the operation should be stored in the undo stack
     */
    Q_INVOKABLE bool resizeClip(int cid, int duration, bool right, bool logUndo = true);
    /* @brief Returns the project's duration (tractor)
     */
    Q_INVOKABLE int duration() const;
    /* @brief Returns the current cursor position (frame currently displayed by MLT)
     */
    Q_INVOKABLE int position() const { return m_position; }
    /* @brief Request a seek operation
       @param position is the desired new timeline position
     */
    Q_INVOKABLE void setPosition(int position);
    Q_INVOKABLE bool snap();
    Q_INVOKABLE bool ripple();
    Q_INVOKABLE bool scrub();
    Q_INVOKABLE QString timecode(int frames);
    /* @brief Request inserting a new clip in timeline (dragged from bin or monitor)
       @param tid is the destination track
       @param position is the timeline position
       @param xml is the data describing the dropped clip
     */
    Q_INVOKABLE void insertClip(int tid, int position, QString xml);
    void deleteSelectedClips();

    Q_INVOKABLE int requestBestSnapPos(int pos, int duration);

    Q_INVOKABLE void triggerAction(const QString &name);

    /* @brief Returns the current tractor's producer, useful fo control seeking, playing, etc
     */
    Mlt::Producer *producer();
    /* @brief Load a new project from a tractor
     */
    void buildFromMelt(Mlt::Tractor tractor);
    /* @brief Define the undo stack for this timeline
     */
    void setUndoStack(std::weak_ptr<DocUndoStack> undo_stack);
    /* @brief Do we want to display video thumbnails
     */
    bool showThumbnails() const;
    bool showAudioThumbnails() const;
    /* @brief Do we want to display audio thumbnails
     */
    Q_INVOKABLE bool showWaveforms() const;
    /* @brief Insert a timeline track
     */
    Q_INVOKABLE void addTrack(int tid);
    /* @brief Remove a timeline track
     */
    Q_INVOKABLE void deleteTrack(int tid);
    Q_INVOKABLE void groupSelection();
    Q_INVOKABLE void unGroupSelection(int cid);

    void gotoNextSnap();
    void gotoPreviousSnap();
protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

public slots:
    void selectMultitrack();
    void onSeeked(int position);
    void slotChangeZoom(int value, bool zoomOnMouse);
    void seek(int position);

private:
    std::shared_ptr<TimelineItemModel> m_model;
    KActionCollection *m_actionCollection;
    BinController *m_binController;
    struct Selection {
        QList<int> selectedClips;
        int selectedTrack;
        bool isMultitrackSelected;
    };
    int m_position;
    double m_scale;
    Selection m_selection;
    Selection m_savedSelection;
    ThumbnailProvider *m_thumbnailer;
    static const int comboScale[];
    static int m_duration;
    void emitSelectedFromSelection();
    void checkDuration();
    static void tractorChanged(mlt_multitrack mtk, void *self);

signals:
    void selectionChanged();
    void selected(Mlt::Producer* producer);
    void trackHeightChanged();
    void scaleFactorChanged();
    void durationChanged();
    void positionChanged();
    void showThumbnailsChanged();
    void showAudioThumbnailsChanged();
    void snapChanged();
    void rippleChanged();
    void scrubChanged();
    void seeked(int position);
    void focusProjectMonitor();
    /* @brief Requests a zoom in for timeline
       @param zoomOnMouse is set to true if we want to center zoom on mouse, otherwise we center on timeline cursor position
     */
    void zoomIn(bool zoomOnMouse);
    /* @brief Requests a zoom out for timeline
       @param zoomOnMouse is set to true if we want to center zoom on mouse, otherwise we center on timeline cursor position
     */
    void zoomOut(bool zoomOnMouse);
};

#endif


