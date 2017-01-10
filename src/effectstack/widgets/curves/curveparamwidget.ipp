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

#include <klocalizedstring.h>
#include "effectstack/dragvalue.h"
#include "utils/KoIconUtils.h"
#include "colortools.h"
#include "bezier/beziersplineeditor.h"
#include "cubic/kis_curve_widget.h"


/*@brief this label is a pixmap corresponding to a legend of the axis*/
template<typename CurveWidget_t>
class ValueLabel : public QLabel
{
public:
    /**@brief Creates the widget
       @param isVert This parameter is true if the widget is vertical
       @param mode This is the original mode
       @param parent Parent of the widget
    */
    ValueLabel(bool isVert, typename CurveParamWidget<CurveWidget_t>::CurveModes mode, QWidget *parent) : QLabel(parent), m_mode(mode), m_isVert(isVert)  {
        if (m_isVert) {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            setMaximumSize(10, 500);
            setFixedWidth(10);
        } else {
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            setFixedHeight(10);
        }
        setScaledContents(true);
    }

public slots:
    void setMode(typename CurveParamWidget<CurveWidget_t>::CurveModes m)
    {
        m_mode = m;
        createPixmap();
    }
private:
    using CurveModes = typename CurveParamWidget<CurveWidget_t>::CurveModes;
    void createPixmap()
    {
        QTransform t;
        QSize s = size();
        if (!m_isVert) {
            t.rotate(90);
            s.setHeight(size().width());
            s.setWidth(size().height());
        }
        if (m_mode == CurveModes::Hue) {
            setPixmap(QPixmap::fromImage(ColorTools::hsvCurvePlane(s, QColor::fromHsv(200, 200, 200), ColorTools::COM_H, ColorTools::COM_H)).transformed(t));
        } else if (m_mode == CurveModes::Saturation) {
            setPixmap(QPixmap());
        } else {
            auto color = CurveParamWidget<CurveWidget_t>::modeToColorsRGB(m_mode);
            setPixmap(QPixmap::fromImage(ColorTools::rgbCurveLine(s, color, palette().background().color().rgb())).transformed(t));
        }
    }

    typename CurveParamWidget<CurveWidget_t>::CurveModes m_mode;
    bool m_isVert;
};

template<>
void CurveParamWidget<KisCurveWidget>::slotUpdatePointP(double, bool final)
{
    m_edit->updateCurrentPoint(QPointF(m_pX->value(), m_pY->value()), final);
}

template<>
void CurveParamWidget<BezierSplineEditor>::slotUpdatePointP(double, bool final)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setP(QPointF(m_pX->value(), m_pY->value()));
    m_edit->updateCurrentPoint(p, final);
}

template<>
void CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1(double /*value*/, bool final)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setH1(QPointF(m_h1X->value(), m_h1Y->value()));
    m_edit->updateCurrentPoint(p, final);
}
template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotUpdatePointH1(double /*value*/, bool /*final*/)
{
}


template<>
void CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2(double /*value*/, bool final)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setH2(QPointF(m_h2X->value(), m_h2Y->value()));
    m_edit->updateCurrentPoint(p, final);
}
template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotUpdatePointH2(double /*value*/, bool /*final*/)
{
}


template<>
void CurveParamWidget<BezierSplineEditor>::slotSetHandlesLinked(bool linked)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setHandlesLinked(linked);
    m_edit->updateCurrentPoint(p);
}
template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotSetHandlesLinked(bool /*linked*/)
{
}

template<>
void CurveParamWidget<BezierSplineEditor>::slotShowAllHandles(bool show)
{
    m_edit->setShowAllHandles(show);
    KdenliveSettings::setBezier_showallhandles(show);
}
template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotShowAllHandles(bool /*show*/)
{
}
template<typename CurveWidget_t>
CurveParamWidget<CurveWidget_t>::CurveParamWidget(const QString &spline, QWidget *parent) :
    AbstractParamWidget(parent),
    m_mode(CurveModes::Luma),
    m_showPixmap(KdenliveSettings::bezier_showpixmap())
{
    // construct curve editor
    m_edit = new CurveWidget_t(this);
    connect(m_edit, &CurveWidget_t::modified, this, &AbstractParamWidget::valueChanged);
    using Point_t = typename CurveWidget_t::Point_t;
    connect(m_edit, static_cast<void (CurveWidget_t::*)(const Point_t&, bool)>
            (&CurveWidget_t::currentPoint),
            this, static_cast<void (CurveParamWidget<CurveWidget_t>::*)(const Point_t&, bool)>
            (&CurveParamWidget<CurveWidget_t>::slotUpdatePointEntries));

    //construct and fill layout
    QVBoxLayout *layout = new QVBoxLayout(this);

    //grid layout containing the curve and the optional param values
    QGridLayout *curve_layout = new QGridLayout();
    curve_layout->addWidget(m_edit, 0, 1);

    m_leftParam = new ValueLabel<CurveWidget_t>(true, m_mode, this);
    m_leftParam->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    m_leftParam->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    curve_layout->addWidget(m_leftParam, 0, 0);

    m_bottomParam = new ValueLabel<CurveWidget_t>(false, m_mode, this);
    m_bottomParam->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    m_bottomParam->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    curve_layout->addWidget(m_bottomParam, 1, 1);

    //horizontal layout to make sure that everything is centered
    QHBoxLayout *horiz_layout = new QHBoxLayout;
    horiz_layout->addLayout(curve_layout);


    layout->addLayout(horiz_layout);
    QWidget *widget = new QWidget(this);
    m_ui.setupUi(widget);
    layout->addWidget(widget);

    //set up icons and initial button states
    m_ui.buttonLinkHandles->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-link")));
    m_ui.buttonDeletePoint->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-remove")));
    m_ui.buttonZoomIn->setIcon(KoIconUtils::themedIcon(QStringLiteral("zoom-in")));
    m_ui.buttonZoomOut->setIcon(KoIconUtils::themedIcon(QStringLiteral("zoom-out")));
    m_ui.buttonGridChange->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-grid")));
    m_ui.buttonShowPixmap->setIcon(QIcon(QPixmap::fromImage(ColorTools::rgbCurvePlane(QSize(16, 16), ColorTools::ColorsRGB::Luma, 0.8))));
    m_ui.buttonResetSpline->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-refresh")));
    m_ui.buttonShowAllHandles->setIcon(KoIconUtils::themedIcon(QStringLiteral("draw-bezier-curves")));
    m_ui.widgetPoint->setEnabled(false);
    m_edit->setGridLines(KdenliveSettings::bezier_gridlines());
    m_ui.buttonShowPixmap->setChecked(KdenliveSettings::bezier_showpixmap());
    m_ui.buttonShowAllHandles->setChecked(KdenliveSettings::bezier_showallhandles());
    slotShowAllHandles(KdenliveSettings::bezier_showallhandles());

    //connect buttons to their slots
    connect(m_ui.buttonLinkHandles, &QAbstractButton::toggled, this, &CurveParamWidget<CurveWidget_t>::slotSetHandlesLinked);
    connect(m_ui.buttonDeletePoint, &QAbstractButton::clicked, m_edit, &CurveWidget_t::slotDeleteCurrentPoint);
    connect(m_ui.buttonZoomIn, &QAbstractButton::clicked, m_edit, &CurveWidget_t::slotZoomIn);
    connect(m_ui.buttonZoomOut, &QAbstractButton::clicked, m_edit, &CurveWidget_t::slotZoomOut);
    connect(m_ui.buttonGridChange, &QAbstractButton::clicked, this, &CurveParamWidget<CurveWidget_t>::slotGridChange);
    connect(m_ui.buttonShowPixmap, &QAbstractButton::toggled, this, &CurveParamWidget<CurveWidget_t>::slotShowPixmap);
    connect(m_ui.buttonResetSpline, &QAbstractButton::clicked, m_edit, &CurveWidget_t::reset);
    connect(m_ui.buttonShowAllHandles, &QAbstractButton::toggled, this, &CurveParamWidget<CurveWidget_t>::slotShowAllHandles);

    setupLayoutPoint();
    setupLayoutHandles();
    m_edit->setFromString(spline);

    deleteIrrelevantItems();
    emit valueChanged();
}

template<>
void CurveParamWidget<KisCurveWidget>::deleteIrrelevantItems()
{
    m_ui.gridLayout->removeWidget(m_ui.buttonShowAllHandles);
    delete m_ui.buttonShowAllHandles;
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::deleteIrrelevantItems()
{
    //Nothing to do in general
}


template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::setupLayoutPoint()
{
    m_pX = new DragValue(i18n("In"), 0, 3, 0, 1, -1, QString(), false, this);
    m_pX->setStep(0.001);
    m_pY = new DragValue(i18n("Out"), 0, 3, 0, 1, -1, QString(), false, this);
    m_pY->setStep(0.001);
    m_ui.layoutP->addWidget(m_pX);
    m_ui.layoutP->addWidget(m_pY);
    connect(m_pX, &DragValue::valueChanged, this,
            &CurveParamWidget<CurveWidget_t>::slotUpdatePointP);
    connect(m_pY, &DragValue::valueChanged, this,
            &CurveParamWidget<CurveWidget_t>::slotUpdatePointP);
}

template<>
void CurveParamWidget<BezierSplineEditor>::setupLayoutHandles()
{
    m_h1X = new DragValue(i18n("X"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h1X->setStep(0.001);
    m_h1Y = new DragValue(i18n("Y"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h1Y->setStep(0.001);
    m_h2X = new DragValue(i18n("X"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h2X->setStep(0.001);
    m_h2Y = new DragValue(i18n("Y"), 0, 3, -2, 2, -1, QString(), false, this);
    m_h2Y->setStep(0.001);
    m_ui.layoutH1->addWidget(new QLabel(i18n("Handle 1:")));
    m_ui.layoutH1->addWidget(m_h1X);
    m_ui.layoutH1->addWidget(m_h1Y);
    m_ui.layoutH2->addWidget(new QLabel(i18n("Handle 2:")));
    m_ui.layoutH2->addWidget(m_h2X);
    m_ui.layoutH2->addWidget(m_h2Y);
    connect(m_h1X, &DragValue::valueChanged,
            this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1);
    connect(m_h1Y, &DragValue::valueChanged,
            this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1);
    connect(m_h2X, &DragValue::valueChanged,
            this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2);
    connect(m_h2Y, &DragValue::valueChanged,
            this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2);
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::setupLayoutHandles()
{
    //Nothing to do in general
}


template<typename CurveWidget_t>
QString CurveParamWidget<CurveWidget_t>::toString() const
{
    return m_edit->toString();
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::setMode(CurveModes mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        if (m_showPixmap) {
            slotShowPixmap(true);
        }
        m_leftParam->setMode(mode);
        m_bottomParam->setMode(mode);
    }
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotGridChange()
{
    m_edit->setGridLines((m_edit->gridLines() + 1) % 9);
    KdenliveSettings::setBezier_gridlines(m_edit->gridLines());
}

template<typename CurveWidget_t>
ColorTools::ColorsRGB CurveParamWidget<CurveWidget_t>::modeToColorsRGB(CurveModes mode)
{
    switch(mode){
    case CurveModes::Red:
        return ColorTools::ColorsRGB::R;
    case CurveModes::Green:
        return ColorTools::ColorsRGB::G;
    case CurveModes::Blue:
        return ColorTools::ColorsRGB::B;
    case CurveModes::Luma:
        return ColorTools::ColorsRGB::Luma;
    case CurveModes::Alpha:
        return ColorTools::ColorsRGB::A;
    case CurveModes::RGB:
    case CurveModes::Hue:
    case CurveModes::Saturation:
    default:
        return ColorTools::ColorsRGB::RGB;
    }
    return ColorTools::ColorsRGB::RGB;
}
template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotShowPixmap(bool show)
{
    m_showPixmap = show;
    KdenliveSettings::setBezier_showpixmap(show);
    if (show) {
        if (m_mode == CurveModes::Hue) {
            m_edit->setPixmap(QPixmap::fromImage(ColorTools::hsvCurvePlane(m_edit->size(), QColor::fromHsv(200, 200, 200), ColorTools::COM_H, ColorTools::COM_H)));
        } else if (m_mode == CurveModes::Saturation) {
            m_edit->setPixmap(QPixmap());
        } else {
            auto color = modeToColorsRGB(m_mode);
            m_edit->setPixmap(QPixmap::fromImage(ColorTools::rgbCurvePlane(m_edit->size(), color, 1, palette().background().color().rgb())));
        }
    } else {
        m_edit->setPixmap(QPixmap());
    }
}

template<>
void CurveParamWidget<BezierSplineEditor>::slotUpdatePointEntries(const BPoint &p, bool extremal)
{
    blockSignals(true);
    if (p == BPoint()) {
        m_ui.widgetPoint->setEnabled(false);
    } else {
        m_ui.widgetPoint->setEnabled(true);
        //disable irrelevant buttons if the point is extremal
        m_pX->setEnabled(!extremal);
        m_ui.buttonDeletePoint->setEnabled(!extremal);
        m_ui.buttonLinkHandles->setEnabled(!extremal);
        if(extremal && p.p.x() + 1e-4 >= 1.00){ //last point
            m_h2X->setEnabled(false);
            m_h2Y->setEnabled(false);
        } else {
            m_h2X->setEnabled(true);
            m_h2Y->setEnabled(true);
        }
        if(extremal && p.p.x() <= 0.01){ //first point
            m_h1X->setEnabled(false);
            m_h1Y->setEnabled(false);
        } else {
            m_h1X->setEnabled(true);
            m_h1Y->setEnabled(true);
        }

        for(auto elem : {m_pX, m_pY, m_h1X, m_h1Y, m_h2X, m_h2Y}) {
            elem->blockSignals(true);
        }
        m_pX->setValue(p.p.x());
        m_pY->setValue(p.p.y());
        m_h1X->setValue(p.h1.x());
        m_h1Y->setValue(p.h1.y());
        m_h2X->setValue(p.h2.x());
        m_h2Y->setValue(p.h2.y());
        for(auto elem : {m_pX, m_pY, m_h1X, m_h1Y, m_h2X, m_h2Y}) {
            elem->blockSignals(false);
        }
        m_ui.buttonLinkHandles->setChecked(p.handlesLinked);
    }
    blockSignals(false);
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotUpdatePointEntries(const BPoint &p, bool extremal)
{
    //Wrong slot called in curve widget
    Q_ASSERT(false);
}

template<>
void CurveParamWidget<KisCurveWidget>::slotUpdatePointEntries(const QPointF &p, bool extremal)
{
    blockSignals(true);
    if (p == QPointF()) {
        m_ui.widgetPoint->setEnabled(false);
    } else {
        m_ui.widgetPoint->setEnabled(true);
        //disable irrelevant buttons if the point is extremal
        m_pX->setEnabled(!extremal);
        m_ui.buttonDeletePoint->setEnabled(!extremal);

        for(auto elem : {m_pX, m_pY}) {
            elem->blockSignals(true);
        }
        m_pX->setValue(p.x());
        m_pY->setValue(p.y());
        for(auto elem : {m_pX, m_pY}) {
            elem->blockSignals(false);
        }
    }
    blockSignals(false);
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotUpdatePointEntries(const QPointF &p, bool extremal)
{
    //Wrong slot called in curve widget
    Q_ASSERT(false);
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::slotShowComment(bool show)
{
    Q_UNUSED(show);
}

template<typename CurveWidget_t>
void CurveParamWidget<CurveWidget_t>::setMaxPoints(int max)
{
    m_edit->setMaxPoints(max);
}
