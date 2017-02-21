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

#ifndef CORNERSWIDGET_H
#define CORNERSWIDGET_H

#include "keyframeedit.h"

class QDomElement;
class Monitor;

class CornersWidget : public KeyframeEdit
{
    Q_OBJECT
public:
    /** @brief Sets up the UI and connects it.
     * @param monitor Project monitor
     * @param clipPos Position of the clip in timeline
     * @param isEffect true if used in an effect, false if used in a transition
     * @param factor Factor by which the parameters differ from the range 0-1
     * @param parent (optional) Parent widget */
    explicit CornersWidget(Monitor *monitor, const QDomElement &e, int minFrame, int maxFrame, int pos, const Timecode &tc, int activeKeyframe, QWidget *parent = nullptr);
    virtual ~CornersWidget();

    void addParameter(const QDomElement &e, int activeKeyframe = -1) Q_DECL_OVERRIDE;

public slots:
    /** @brief Updates the on-monitor item.  */
    void slotSyncPosition(int relTimelinePos);
    void slotPrepareKeyframe();
    /** @brief Get info about frame size, source clip dar. */
    void setFrameSize(const QPoint &size, double stretch);

private:
    Monitor *m_monitor;
    int m_pos;

    /** @brief Returns the corner positions set in the row of @param keyframe. */
    QVariantList getPoints(QTableWidgetItem *keyframe);

private slots:

    /** @brief Updates the on-monitor item according to the current timeline position. */
    void slotUpdateItem();
    /** @brief Updates the keyframe editor according to the on-monitor item. */
    void slotUpdateGeometry(const QVariantList &points);

    /** @brief Inserts a keyframe at the current (playback) position (m_pos). */
    void slotInsertKeyframe();

    /** @brief Shows/Hides the lines connecting the corners in the on-monitor item according to @param show. */
    void slotShowLines(bool show = true);

    /** @brief Shows/Hides additional controls on the monitor according to @param show. */
    void slotShowControls(bool show = true);
};

#endif
