/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2024 aisuneko icecat <iceneko@protonmail.ch>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "../../../../bpoint.h"
#include "../abstractcurvewidget.h"
#include "../bezier/cubicbezierspline.h"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "utils/colortools.h"
#include <QMutex>
#include <QWidget>

/** @class KeyframeCurveEditor
    @brief \@todo Describe class KeyframeCurveEditor
    @todo Describe class KeyframeCurveEditor
 */
class KeyframeCurveEditor : public AbstractCurveWidget<CubicBezierSpline>
{
    Q_OBJECT
Q_SIGNALS:
    void atKeyframe(bool isKeyframe, bool singleKeyframe);
    void seekToPos(int pos);
    void modified();
    void activateEffect();

public:
    using Point_t = BPoint;
    explicit KeyframeCurveEditor(std::shared_ptr<KeyframeModelList> model, int duration, double min, double max, double factor,
                                 const QPersistentModelIndex &paramindex, int rectindex = -1, QWidget *parent = nullptr);
    ~KeyframeCurveEditor() override;

    QList<BPoint> getPoints() const override;

    bool isHandleDisabled(BPoint point);

    void setDuration(int duration);

public Q_SLOTS:
    void slotModelChanged();
    void slotModelDisplayChanged();
    void slotSetPosition(int pos, bool isInRange);
    void slotOnFocus();
    void slotLoseFocus();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintBackground(QPainter *p);

private:
    std::shared_ptr<KeyframeModelList> m_model;
    CubicBezierSpline m_curve;
    /** @brief Index of the parameter that this instance of KeyframeCurveEditor would handle */
    QPersistentModelIndex m_paramindex;
    int m_duration;
    int m_position;
    int m_clickKeyframe;
    QVariant m_clickValue;
    /** @brief Record the index of the specific part of data the widget controls, if the parameter is an AnimatedRect */
    int m_rectindex;

    QVector<int> m_selectedKeyframes;
    QMutex m_curveMutex;
    int m_hoverKeyframe;
    int m_lineHeight;
    bool m_moveKeyframeMode;
    bool m_keyframeZonePress;
    int m_clickPoint;
    int m_clickEnd;
    int m_size;
    double m_minVal, m_maxVal;
    double m_minLimit, m_maxLimit;
    double m_interval;
    double m_factor;
    double m_paddingfactor;
    BPoint::PointType m_currentPointType{BPoint::PointType::P};
    double m_grabOffsetX{0};
    double m_grabOffsetY{0};
    /** selected point before it was modified by dragging (at the time of the mouse press) */
    BPoint m_grabPOriginal;

    /** @brief Finds the point nearest to @param p and returns it's index.
     * @param sel Is filled with the type of the closest point (h1, p, h2)
     *
     * If no point is near enough -1 is returned. */
    int nearestPointInRange(const QPointF &p, int wWidth, int wHeight, BPoint::PointType *sel);
    void loadSplineFromModel();
    void updateInterval();
    double getPadding();
    double valueFromCanvasPos(double ypos);
    void updateKeyframeData(double val);
    int seekPosOnCanvas(double xpos);
    const QPointF getPointFromModel(int framePos, int offset);
};
