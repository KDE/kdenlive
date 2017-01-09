/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
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


#ifndef CURVEPARAMWIDGET_H
#define CURVEPARAMWIDGET_H

#include "ui_bezierspline_ui.h"
#include "../abstractparamwidget.h"
#include "bezier/beziersplineeditor.h"
#include "cubic/kis_curve_widget.h"



/** @brief Class representing a curve and some additional controls
 */
template<typename CurveWidget_t>
class CurveParamWidget : public AbstractParamWidget
{
public:
    virtual ~CurveParamWidget(){};
    CurveParamWidget(const QString &spline, QWidget *parent);

    enum class CurveModes { Red = 0,
            Green = 1,
            Blue = 2,
            Luma = 3,
            Alpha = 4,
            RGB = 5,
            Hue = 6,
            Saturation = 7 };
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
    QList<Point_t> getPoints(){return m_edit->getPoints();}

    /** @brief Set the maximal number of points on the curve. This function is only available for KisCurveWidget.
     */
    void setMaxPoints(int max);

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
    /** @brief Helper function to convert a mode to the corresponding ColorsRGB value.
        This avoids using potentially non consistent intermediate cast to int
    */
    static ColorTools::ColorsRGB modeToColorsRGB(CurveModes mode);
public:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool);
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

};

#include "curveparamwidget.ipp"


#endif
