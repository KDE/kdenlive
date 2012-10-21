/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#ifndef GEOMETRYWIDGET_H
#define GEOMETRYWIDGET_H

#include "ui_geometrywidget_ui.h"
#include "timecode.h"
#include <mlt++/Mlt.h>

#include <QWidget>

class QDomElement;
class Monitor;
class MonitorScene;
class KeyframeHelper;
class TimecodeDisplay;
class OnMonitorRectItem;
class QGraphicsRectItem;
class DragValue;

class GeometryWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the UI and connects it.
    * @param monitor Project monitor
    * @param timecode Timecode needed by timecode display widget
    * @param clipPos Position of the clip in timeline
    * @param isEffect true if used in an effect, false if used in a transition
    * @param showRotation Should we show or hide the rotation sliders
    * @param parent (optional) Parent widget */
    GeometryWidget(Monitor *monitor, Timecode timecode, int clipPos, bool isEffect, bool showRotation, QWidget* parent = 0);
    virtual ~GeometryWidget();
    /** @brief Gets the geometry as a serialized string. */
    QString getValue() const;
    QString getExtraValue(const QString &name) const;
    /** @brief Updates the timecode display according to settings (frame number or hh:mm:ss:ff) */
    void updateTimecodeFormat();
    /** @brief Sets the size of the original clip. */
    void setFrameSize(QPoint size);
    void addParameter(const QDomElement elem);
    void importKeyframes(const QString &data);

public slots:
    /** @brief Sets up the rect and the geometry object.
    * @param elem DomElement representing this effect parameter
    * @param minframe In point of the clip
    * @param maxframe Out point of the clip */
    void setupParam(const QDomElement elem, int minframe, int maxframe);
    /** @brief Updates position of the local timeline to @param relTimelinePos.  */
    void slotSyncPosition(int relTimelinePos);

private:
    Ui::GeometryWidget_UI m_ui;
    Monitor *m_monitor;
    TimecodeDisplay *m_timePos;
    /** Position of the clip in timeline. */
    int m_clipPos;
    /** In point of the clip (crop from start). */
    int m_inPoint;
    /** Out point of the clip (crop from end). */
    int m_outPoint;
    bool m_isEffect;
    MonitorScene *m_scene;
    OnMonitorRectItem *m_rect;
    QGraphicsRectItem *m_previous;
    KeyframeHelper *m_timeline;
    /** Stores the different settings in the MLT geometry format. */
    Mlt::Geometry *m_geometry;
    QStringList m_extraGeometryNames;
    QStringList m_extraFactors;
    QList <Mlt::Geometry *>m_extraGeometries;
    bool m_showScene;
    DragValue *m_spinX;
    DragValue *m_spinY;
    DragValue *m_spinWidth;
    DragValue *m_spinHeight;
    DragValue *m_spinSize;
    DragValue *m_opacity;
    DragValue *m_rotateX;
    DragValue *m_rotateY;
    DragValue *m_rotateZ;
    QPoint m_frameSize;
    bool m_showRotation;
    /** @brief Update monitor rect with current width / height values. */
    void updateMonitorGeometry();

private slots:
    /** @brief Updates controls according to position.
    * @param pos (optional) Position to update to
    * @param seek (optional, default = true) Whether to seek timleine & project monitor to pos
    * If pos = -1 (default) the value of m_timePos is used. */
    void slotPositionChanged(int pos = -1, bool seek = true);
    /** @brief Seeking requested from timeline. */
    void slotRequestSeek(int pos);
    /** @brief Updates settings after a keyframe was moved to @param pos. */
    void slotKeyframeMoved(int pos);
    /** @brief Adds a keyframe.
    * @param pos (optional) Position where the keyframe should be added
    * If pos = -1 (default) the value of m_timePos is used. */
    void slotAddKeyframe(int pos = -1);
    /** @brief Deletes a keyframe.
    * @param pos (optional) Position of the keyframe which should be deleted
    * If pos = -1 (default) the value of m_timePos is used. */
    void slotDeleteKeyframe(int pos = -1);
    /** @brief Goes to the next keyframe or to the end if none is available. */
    void slotNextKeyframe();
    /** @brief Goes to the previous keyframe or to the beginning if none is available. */
    void slotPreviousKeyframe();
    /** @brief Adds or deletes a keyframe depending on whether there is already a keyframe at the current position. */
    void slotAddDeleteKeyframe();

    /** @brief Updates the Mlt::Geometry object. */
    void slotUpdateGeometry();
    /** @brief Updates the spinBoxes according to the rect. */
    void slotUpdateProperties();

    /** @brief Sets the rect's x position to @param value. */
    void slotSetX(double value);
    /** @brief Sets the rect's y position to @param value. */
    void slotSetY(double value);
    /** @brief Sets the rect's width to @param value. */
    void slotSetWidth(double value);
    /** @brief Sets the rect's height to @param value. */
    void slotSetHeight(double value);

    /** @brief Resizes the rect by @param value (in perecent) compared to the frame size. */
    void slotResize(double value);

    /** @brief Sets the opacity to @param value. */
    void slotSetOpacity(double value);

    /** @brief Moves the rect to the left frame border (x position = 0). */
    void slotMoveLeft();
    /** @brief Centers the rect horizontally. */
    void slotCenterH();
    /** @brief Moves the rect to the right frame border (x position = frame width - rect width). */
    void slotMoveRight();
    /** @brief Moves the rect to the top frame border (y position = 0). */
    void slotMoveTop();
    /** @brief Centers the rect vertically. */
    void slotCenterV();
    /** @brief Moves the rect to the bottom frame border (y position = frame height - rect height). */
    void slotMoveBottom();

    /** @brief Enables/Disables syncing with the timeline according to @param sync. */
    void slotSetSynchronize(bool sync);
    void slotAdjustToFrameSize();
    void slotFitToWidth();
    void slotFitToHeight();
    /** @brief Show / hide previous keyframe in monitor scene. */
    void slotShowPreviousKeyFrame(bool show);

signals:
    void parameterChanged();
    void seekToPos(int);
    void importClipKeyframes();
};

#endif
