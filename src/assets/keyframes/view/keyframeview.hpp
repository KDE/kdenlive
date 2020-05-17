/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#ifndef KEYFRAMEVIEW2_H
#define KEYFRAMEVIEW2_H

#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include <QWidget>
#include <memory>

class KeyframeModelList;

class KeyframeView : public QWidget
{
    Q_OBJECT

public:
    explicit KeyframeView(std::shared_ptr<KeyframeModelList> model, int duration, QWidget *parent = nullptr);
    void setDuration(int dur);

public slots:
    /* @brief moves the current position*/
    void slotSetPosition(int pos, bool isInRange);
    /* @brief remove the keyframe at given position
       If pos is negative, we remove keyframe at current position
     */
    void slotRemoveKeyframe(int pos);
    /* @brief Add a keyframe with given parameter value at given pos.
       If pos is negative, then keyframe is added at current position
    */
    void slotAddKeyframe(int pos = -1);
    /* @brief If there is a keyframe at current position, it is removed.
       Otherwise, we add a new one with given value.
    */
    void slotAddRemove();
    void slotGoToNext();
    void slotGoToPrev();
    void slotModelChanged();
    void slotEditType(int type, const QPersistentModelIndex &index);
    /* @brief Emit initial info for monitor. */
    void initKeyframePos();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    std::shared_ptr<KeyframeModelList> m_model;
    int m_duration;
    int m_position;
    int m_currentKeyframe;
    int m_currentKeyframeOriginal;
    int m_hoverKeyframe;
    int m_lineHeight;
    int m_zoomHeight;
    int m_offset;
    double m_scale;
    double m_zoomFactor;
    double m_zoomStart;
    /** @brief The zoom factor (start, end - between 0 and 1) */
    QPointF m_zoomHandle;
    QPointF m_lastZoomHandle;
    /** @brief Mouse is the zoom left handle */
    bool m_hoverZoomIn;
    /** @brief Mouse is the zoom right handle */
    bool m_hoverZoomOut;
    /** @brief Mouse is over the zoom bar */
    bool m_hoverZoom;
    /** @brief the x click position offset for moving zoom */
    double m_clickOffset;
    int m_size;

    QColor m_colSelected;
    QColor m_colKeyframe;
    QColor m_colKeyframeBg;

signals:
    void seekToPos(int pos);
    void atKeyframe(bool isKeyframe, bool singleKeyframe);
    void modified();
    void activateEffect();
};

#endif
