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
#include "abstractparamwidget.h"
#include <mlt++/Mlt.h>

#include <QWidget>

class QDomElement;
class Monitor;
class KeyframeHelper;
class TimecodeDisplay;
class QGraphicsRectItem;
class DragValue;
struct EffectMetaInfo;

class GeometryWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the UI and connects it.
    * @param monitor Project monitor
    * @param timecode Timecode needed by timecode display widget
    * @param clipPos Position of the clip in timeline
    * @param parent (optional) Parent widget */
    explicit GeometryWidget(EffectMetaInfo *info, int clipPos, bool showRotation, bool useOffset, QWidget *parent = nullptr);
    virtual ~GeometryWidget();
    /** @brief Gets the geometry as a serialized string. */
    QString getValue() const;
    QString getExtraValue(const QString &name) const;
    /** @brief Updates the timecode display according to settings (frame number or hh:mm:ss:ff) */
    void updateTimecodeFormat();
    /** @brief Sets the size of the original clip. */
    void setFrameSize(const QPoint &size);
    void addParameter(const QDomElement &elem);
    void importKeyframes(const QString &data, int maximum);
    QString offsetAnimation(int offset, bool useOffset);
    /** @brief Effect param @tag was changed, reload keyframes */
    void reload(const QString &tag, const QString &data);
    /** @brief connect with monitor scene */
    void connectMonitor(bool activate);

public slots:
    /** @brief Sets up the rect and the geometry object.
    * @param elem DomElement representing this effect parameter
    * @param minframe In point of the clip
    * @param maxframe Out point of the clip */
    void setupParam(const QDomElement &elem, int minframe, int maxframe);
    /** @brief Updates position of the local timeline to @param relTimelinePos.  */
    void slotSyncPosition(int relTimelinePos);
    void slotResetKeyframes();
    void slotResetNextKeyframes();
    void slotResetPreviousKeyframes();
    void slotUpdateRange(int inPoint, int outPoint);
    /** @brief Send geometry info to the monitor. */
    void slotInitScene(int pos);
    void slotSeekToKeyframe(int index);

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
    QGraphicsRectItem *m_previous;
    KeyframeHelper *m_timeline;
    /** Stores the different settings in the MLT geometry format. */
    Mlt::Geometry *m_geometry;
    QStringList m_extraGeometryNames;
    QStringList m_extraFactors;
    QList<Mlt::Geometry *>m_extraGeometries;
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
    /** @brief Action switching between profile and source size. */
    QAction *m_originalSize;
    /** @brief Action locking image ratio. */
    QAction *m_lockRatio;
    /** @brief True if this is a fixed parameter (no kexframes allowed). */
    bool m_fixedGeom;
    /** @brief True if there is only one keyframe in this geometry. */
    bool m_singleKeyframe;
    /** @brief Should we use an offset when displaying keyframes (when effect is not synced with producer). */
    bool m_useOffset;
    /** @brief Update monitor rect with current width / height values. */
    void updateMonitorGeometry();
    /** @brief Calculate the path for rectangle center moves. */
    QVariantList calculateCenters();
    /** @brief check if we have only one keyframe for this geometry. */
    void checkSingleKeyframe();

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
    void slotUpdateGeometryRect(const QRect r);
    /** @brief Updates the spinBoxes according to the rect. */
    void slotUpdateProperties(QRect rect = QRect());

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
    /** @brief Adjust size to source clip original resolution*/
    void slotAdjustToSource();
    /** @brief Adjust size to project's frame size */
    void slotAdjustToFrameSize();
    void slotFitToWidth();
    void slotFitToHeight();
    /** @brief Show / hide previous keyframe in monitor scene. */
    void slotShowPreviousKeyFrame(bool show);
    /** @brief Show / hide keyframe path in monitor scene. */
    void slotShowPath(bool show);
    /** @brief Edit center points for the geometry keyframes. */
    void slotUpdateCenters(const QVariantList &centers);
    /** @brief Un/Lock aspect ratio for size in effect parameter. */
    void slotLockRatio();

signals:
    void seekToPos(int);
    void importClipKeyframes();
};

#endif
