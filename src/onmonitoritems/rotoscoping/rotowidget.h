/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef ROTOWIDGET_H
#define ROTOWIDGET_H

#include "definitions.h"
#include "effectstack/widgets/curves/bezier/bpoint.h"
#include "timecode.h"

#include <QWidget>

class Monitor;
class SplineItem;
class SimpleKeyframeWidget;
namespace Mlt
{
class Filter;
}

/** @brief Adjusts keyframes after resizing a clip. */
bool adjustRotoDuration(QByteArray *data, int in, int out);

class RotoWidget : public QWidget
{
    Q_OBJECT

public:
    RotoWidget(const QByteArray &data, Monitor *monitor, const ItemInfo &info, const Timecode &t, QWidget *parent = nullptr);
    ~RotoWidget();

    /** @brief Returns the spline(s) in the JSON format used by filter_rotoscoping (MLT). */
    QByteArray getSpline();

    /** @brief Replaces current data with \param spline (JSON). */
    void setSpline(const QByteArray &spline, bool notify = true);

    /** @brief Passed on to the keyframe timeline. Switches between frames and hh:mm:ss:ff timecode. */
    void updateTimecodeFormat();

public slots:
    /** @brief Updates the on-monitor item.  */
    void slotSyncPosition(int relTimelinePos);

signals:
    void valueChanged();
    void seekToPos(int pos);

private:
    SimpleKeyframeWidget *m_keyframeWidget;
    Monitor *m_monitor;
    //MonitorScene *m_scene;
    QVariant m_data;
    SplineItem *m_item;
    int m_in;
    int m_out;
    Mlt::Filter *m_filter;

    /** @brief Returns the list of cubic Bézier points that form the spline at position @param keyframe.
     * The points are brought from the range [0, 1] into project resolution space.
     * This function does not do any interpolation and therfore will only return a list when a keyframe at the given postion exists.
     * Set @param keyframe to -1 if only one keyframe currently exists. */
    QList<BPoint> getPoints(int keyframe);

    /** @brief Adds tracking_finished as listener for "tracking-finished" event in MLT rotoscoping filter. */
    void setupTrackingListen(const ItemInfo &info);

    /** @brief Passes list of keyframe positions to keyframe timeline widget. */
    void keyframeTimelineFullUpdate();

private slots:

    /** @brief Updates/Creates the spline at @param pos based on the on-monitor items. */
    void slotUpdateData(int pos, const QList<BPoint> &spline);
    /** @brief Updates/Creates the spline at the current timeline position based on the on-monitor items. */
    void slotUpdateDataPoints(const QVariantList &points, int pos = -1);

    /** @brief Updates the on-monitor items to fit the spline at position @param pos. */
    void slotPositionChanged(int pos, bool seek = true);

    void slotAddKeyframe(int pos = -1);
    void slotRemoveKeyframe(int pos = -1);
    void slotMoveKeyframe(int oldPos, int newPos);
};

#endif
