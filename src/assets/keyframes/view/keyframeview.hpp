/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
    void setDuration(int dur, int inPoint);
    const QString getAssetId();
    /** @brief Copy a keyframe parameter to selected keyframes. */
    void copyCurrentValue(const QModelIndex &ix, const QString &paramName);

public slots:
    /** @brief moves the current position*/
    void slotSetPosition(int pos, bool isInRange);
    /** @brief remove the keyframe at given position
       If pos is negative, we remove keyframe at current position
     */
    void slotRemoveKeyframe(const QVector<int> &positions);
    /** @brief Add a keyframe with given parameter value at given pos.
       If pos is negative, then keyframe is added at current position
    */
    bool slotAddKeyframe(int pos = -1);
    /** @brief Duplicate selected keyframe at cursor position
    */
    void slotDuplicateKeyframe();
    /** @brief If there is a keyframe at current position, it is removed.
       Otherwise, we add a new one with given value.
    */
    void slotAddRemove();
    void slotGoToNext();
    void slotGoToPrev();
    void slotModelChanged();
    void slotModelDisplayChanged();
    void slotEditType(int type, const QPersistentModelIndex &index);
    /** @brief Emit initial info for monitor. */
    void initKeyframePos();
    /** @brief Move selected keyframe to cursor position. */
    void slotCenterKeyframe();

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
    int m_currentKeyframeOriginal;
    QVector <int>m_selectedKeyframes;
    int m_hoverKeyframe;
    int m_lineHeight;
    int m_zoomHeight;
    int m_offset;
    double m_scale;
    double m_zoomFactor;
    double m_zoomStart;
    bool m_moveKeyframeMode;
    bool m_keyframeZonePress;
    int m_clickPoint;
    int m_clickEnd;
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
