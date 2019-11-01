/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
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

#ifndef CURVEPARAMWIDGET_H
#define CURVEPARAMWIDGET_H

#include "../abstractparamwidget.hpp"
#include "bezier/beziersplineeditor.h"
#include "cubic/kis_curve_widget.h"
#include "ui_bezierspline_ui.h"
#include "widgets/dragvalue.h"

template <typename CurveWidget_t> class ValueLabel;

/** @brief Class representing a curve and some additional controls
 */
template <typename CurveWidget_t> class CurveParamWidget : public AbstractParamWidget
{
public:
    ~CurveParamWidget() override = default;
    CurveParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

    enum class CurveModes { Red = 0, Green = 1, Blue = 2, Luma = 3, Alpha = 4, RGB = 5, Hue = 6, Saturation = 7 };
    /** @brief sets the mode of the curve. This affects the background that is displayed.
     *  The list of available modes depends on the CurveWidget that we have
     */
    void setMode(CurveModes mode);

    /** @brief Stringify the content of the curve
     */
    QString toString() const;

    using Point_t = typename CurveWidget_t::Point_t;
    /** @brief returns the list of points on the curve
     */
    QList<Point_t> getPoints() { return m_edit->getPoints(); }

    /** @brief Set the maximal number of points on the curve. This function is only available for KisCurveWidget.
     */
    void setMaxPoints(int max);

    /** @brief Helper function to convert a mode to the corresponding ColorsRGB value.
        This avoids using potentially non consistent intermediate cast to int
    */
    static ColorTools::ColorsRGB modeToColorsRGB(CurveModes mode);

protected:
    void deleteIrrelevantItems();
    void setupLayoutPoint();
    void setupLayoutHandles();
    void slotGridChange();
    void slotShowPixmap(bool show);
    void slotUpdatePointEntries(const BPoint &p, bool extremal);
    void slotUpdatePointEntries(const QPointF &p, bool extremal);
    void slotUpdatePointP(double /*value*/, bool final);
    void slotUpdatePointH1(double /*value*/, bool final);
    void slotUpdatePointH2(double /*value*/, bool final);
    void slotSetHandlesLinked(bool linked);
    void slotShowAllHandles(bool show);
    void resizeEvent(QResizeEvent *event) override;

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

private:
    Ui::BezierSpline_UI m_ui;
    DragValue *m_pX;
    DragValue *m_pY;
    DragValue *m_h1X;
    DragValue *m_h1Y;
    DragValue *m_h2X;
    DragValue *m_h2Y;
    CurveWidget_t *m_edit;
    CurveModes m_mode;
    bool m_showPixmap;

    ValueLabel<CurveWidget_t> *m_leftParam, *m_bottomParam;
};

#include "curveparamwidget.ipp"

#endif
