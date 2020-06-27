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

#include "bezier/beziersplineeditor.h"
#include "colortools.h"
#include "cubic/kis_curve_widget.h"
#include "kdenlivesettings.h"
#include "widgets/dragvalue.h"
#include <klocalizedstring.h>

/*@brief this label is a pixmap corresponding to a legend of the axis*/
template <typename CurveWidget_t> class ValueLabel : public QLabel
{
public:
    /**@brief Creates the widget
       @param isVert This parameter is true if the widget is vertical
       @param mode This is the original mode
       @param parent Parent of the widget
    */
    ValueLabel(bool isVert, typename CurveParamWidget<CurveWidget_t>::CurveModes mode, QWidget *parent)
        : QLabel(parent)
        , m_mode(mode)
        , m_isVert(isVert)
    {
        if (m_isVert) {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
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

template <> void CurveParamWidget<KisCurveWidget>::slotUpdatePointP(double, bool final)
{
    m_edit->updateCurrentPoint(QPointF(m_pX->value(), m_pY->value()), final);
}

template <> void CurveParamWidget<BezierSplineEditor>::slotUpdatePointP(double, bool final)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setP(QPointF(m_pX->value(), m_pY->value()));
    m_edit->updateCurrentPoint(p, final);
}

template <> void CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1(double /*value*/, bool final)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setH1(QPointF(m_h1X->value(), m_h1Y->value()));
    m_edit->updateCurrentPoint(p, final);
}
template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotUpdatePointH1(double /*value*/, bool /*final*/) {}

template <> void CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2(double /*value*/, bool final)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setH2(QPointF(m_h2X->value(), m_h2Y->value()));
    m_edit->updateCurrentPoint(p, final);
}
template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotUpdatePointH2(double /*value*/, bool /*final*/) {}

template <> void CurveParamWidget<BezierSplineEditor>::slotSetHandlesLinked(bool linked)
{
    BPoint p = m_edit->getCurrentPoint();
    p.setHandlesLinked(linked);
    m_edit->updateCurrentPoint(p);
}
template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotSetHandlesLinked(bool /*linked*/) {}

template <> void CurveParamWidget<BezierSplineEditor>::slotShowAllHandles(bool show)
{
    m_edit->setShowAllHandles(show);
    KdenliveSettings::setBezier_showallhandles(show);
}
template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotShowAllHandles(bool /*show*/) {}
template <typename CurveWidget_t>

CurveParamWidget<CurveWidget_t>::CurveParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_mode(CurveModes::Luma)
    , m_showPixmap(KdenliveSettings::bezier_showpixmap())
{
    // construct curve editor
    m_edit = new CurveWidget_t(this);
    connect(m_edit, static_cast<void (CurveWidget_t::*)(const Point_t &, bool)>(&CurveWidget_t::currentPoint), this,
            static_cast<void (CurveParamWidget<CurveWidget_t>::*)(const Point_t &, bool)>(&CurveParamWidget<CurveWidget_t>::slotUpdatePointEntries));

    // construct and fill layout
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    layout->addWidget(m_edit);
    m_edit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    m_leftParam = new ValueLabel<CurveWidget_t>(true, m_mode, this);
    m_leftParam->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    m_leftParam->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_bottomParam = new ValueLabel<CurveWidget_t>(false, m_mode, this);
    m_bottomParam->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    m_bottomParam->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    // horizontal layout to make sure that everything is centered
    auto *horiz_layout = new QHBoxLayout;
    horiz_layout->addWidget(m_leftParam);
    horiz_layout->addWidget(m_bottomParam);

    layout->addLayout(horiz_layout);
    auto *widget = new QWidget(this);
    widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_ui.setupUi(widget);
    layout->addWidget(widget);

    // set up icons and initial button states
    m_ui.buttonLinkHandles->setIcon(QIcon::fromTheme(QStringLiteral("edit-link")));
    m_ui.buttonDeletePoint->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    m_ui.buttonZoomIn->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    m_ui.buttonZoomOut->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    m_ui.buttonGridChange->setIcon(QIcon::fromTheme(QStringLiteral("view-grid")));
    m_ui.buttonShowPixmap->setIcon(QIcon(QPixmap::fromImage(ColorTools::rgbCurvePlane(QSize(16, 16), ColorTools::ColorsRGB::Luma, (float)0.8))));
    m_ui.buttonResetSpline->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    m_ui.buttonShowAllHandles->setIcon(QIcon::fromTheme(QStringLiteral("draw-bezier-curves")));
    m_ui.widgetPoint->setEnabled(false);
    m_edit->setGridLines(KdenliveSettings::bezier_gridlines());
    m_ui.buttonShowPixmap->setChecked(KdenliveSettings::bezier_showpixmap());
    m_ui.buttonShowAllHandles->setChecked(KdenliveSettings::bezier_showallhandles());
    slotShowAllHandles(KdenliveSettings::bezier_showallhandles());

    // connect buttons to their slots
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
    slotRefresh();

    deleteIrrelevantItems();
    // emit the signal of the base class when appropriate
    connect(m_edit, &CurveWidget_t::modified, [this]() { emit valueChanged(m_index, m_edit->toString(), true); });
}

template <> void CurveParamWidget<KisCurveWidget>::deleteIrrelevantItems()
{
    m_ui.gridLayout->removeItem(m_ui.horizontalSpacer_3);
    delete m_ui.horizontalSpacer_3;
    delete m_ui.layoutH1;
    delete m_ui.layoutH2;
    delete m_ui.buttonLinkHandles;
    delete m_ui.handlesLayout;
    m_ui.gridLayout->removeWidget(m_ui.buttonShowAllHandles);
    delete m_ui.buttonShowAllHandles;

}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::deleteIrrelevantItems()
{
    // Nothing to do in general
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::setupLayoutPoint()
{
    m_pX = new DragValue(i18n("In"), 0, 3, 0, 1, -1, QString(), false, false, this);
    m_pX->setStep(0.001);
    m_pY = new DragValue(i18n("Out"), 0, 3, 0, 1, -1, QString(), false, false, this);
    m_pY->setStep(0.001);
    m_ui.layoutP->addWidget(m_pX);
    m_ui.layoutP->addWidget(m_pY);
    connect(m_pX, &DragValue::valueChanged, this, &CurveParamWidget<CurveWidget_t>::slotUpdatePointP);
    connect(m_pY, &DragValue::valueChanged, this, &CurveParamWidget<CurveWidget_t>::slotUpdatePointP);
}

template <> void CurveParamWidget<BezierSplineEditor>::setupLayoutHandles()
{
    m_h1X = new DragValue(i18n("X"), 0, 3, -2, 2, -1, QString(), false, false, this);
    m_h1X->setStep(0.001);
    m_h1Y = new DragValue(i18n("Y"), 0, 3, -2, 2, -1, QString(), false, false, this);
    m_h1Y->setStep(0.001);
    m_h2X = new DragValue(i18n("X"), 0, 3, -2, 2, -1, QString(), false, false, this);
    m_h2X->setStep(0.001);
    m_h2Y = new DragValue(i18n("Y"), 0, 3, -2, 2, -1, QString(), false, false, this);
    m_h2Y->setStep(0.001);
    m_ui.layoutH1->addWidget(new QLabel(i18n("Handle 1:")));
    m_ui.layoutH1->addWidget(m_h1X);
    m_ui.layoutH1->addWidget(m_h1Y);
    m_ui.layoutH2->addWidget(new QLabel(i18n("Handle 2:")));
    m_ui.layoutH2->addWidget(m_h2X);
    m_ui.layoutH2->addWidget(m_h2Y);
    connect(m_h1X, &DragValue::valueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1);
    connect(m_h1Y, &DragValue::valueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1);
    connect(m_h2X, &DragValue::valueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2);
    connect(m_h2Y, &DragValue::valueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2);
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::setupLayoutHandles()
{
    // Nothing to do in general
}

template <typename CurveWidget_t> QString CurveParamWidget<CurveWidget_t>::toString() const
{
    return m_edit->toString();
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::setMode(CurveModes mode)
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

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotGridChange()
{
    m_edit->setGridLines((m_edit->gridLines() + 1) % 9);
    KdenliveSettings::setBezier_gridlines(m_edit->gridLines());
}

template <typename CurveWidget_t> ColorTools::ColorsRGB CurveParamWidget<CurveWidget_t>::modeToColorsRGB(CurveModes mode)
{
    switch (mode) {
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
template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotShowPixmap(bool show)
{
    m_showPixmap = show;
    KdenliveSettings::setBezier_showpixmap(show);
    if (show) {
        if (m_mode == CurveModes::Hue) {
            m_edit->setPixmap(
                QPixmap::fromImage(ColorTools::hsvCurvePlane(m_edit->size(), QColor::fromHsv(200, 200, 200), ColorTools::COM_H, ColorTools::COM_H)));
        } else if (m_mode == CurveModes::Saturation) {
            m_edit->setPixmap(QPixmap());
        } else {
            auto color = modeToColorsRGB(m_mode);
            m_edit->setPixmap(QPixmap::fromImage(ColorTools::rgbCurvePlane(m_edit->size(), color, 1, palette().window().color().rgb())));
        }
    } else {
        m_edit->setPixmap(QPixmap());
    }
}

template <> void CurveParamWidget<BezierSplineEditor>::slotUpdatePointEntries(const BPoint &p, bool extremal)
{
    blockSignals(true);
    if (p == BPoint()) {
        m_ui.widgetPoint->setEnabled(false);
    } else {
        m_ui.widgetPoint->setEnabled(true);
        // disable irrelevant buttons if the point is extremal
        m_pX->setEnabled(!extremal);
        m_ui.buttonDeletePoint->setEnabled(!extremal);
        m_ui.buttonLinkHandles->setEnabled(!extremal);
        if (extremal && p.p.x() + 1e-4 >= 1.00) { // last point
            m_h2X->setEnabled(false);
            m_h2Y->setEnabled(false);
        } else {
            m_h2X->setEnabled(true);
            m_h2Y->setEnabled(true);
        }
        if (extremal && p.p.x() <= 0.01) { // first point
            m_h1X->setEnabled(false);
            m_h1Y->setEnabled(false);
        } else {
            m_h1X->setEnabled(true);
            m_h1Y->setEnabled(true);
        }

        for (auto elem : {m_pX, m_pY, m_h1X, m_h1Y, m_h2X, m_h2Y}) {
            elem->blockSignals(true);
        }
        m_pX->setValue(p.p.x());
        m_pY->setValue(p.p.y());
        m_h1X->setValue(p.h1.x());
        m_h1Y->setValue(p.h1.y());
        m_h2X->setValue(p.h2.x());
        m_h2Y->setValue(p.h2.y());
        for (auto elem : {m_pX, m_pY, m_h1X, m_h1Y, m_h2X, m_h2Y}) {
            elem->blockSignals(false);
        }
        m_ui.buttonLinkHandles->setChecked(p.handlesLinked);
    }
    blockSignals(false);
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotUpdatePointEntries(const BPoint &p, bool extremal)
{
    Q_UNUSED(p);
    Q_UNUSED(extremal);
    // Wrong slot called in curve widget
    Q_ASSERT(false);
}

template <> void CurveParamWidget<KisCurveWidget>::slotUpdatePointEntries(const QPointF &p, bool extremal)
{
    blockSignals(true);
    if (p == QPointF()) {
        m_ui.widgetPoint->setEnabled(false);
    } else {
        m_ui.widgetPoint->setEnabled(true);
        // disable irrelevant buttons if the point is extremal
        m_pX->setEnabled(!extremal);
        m_ui.buttonDeletePoint->setEnabled(!extremal);

        for (auto elem : {m_pX, m_pY}) {
            elem->blockSignals(true);
        }
        m_pX->setValue(p.x());
        m_pY->setValue(p.y());
        for (auto elem : {m_pX, m_pY}) {
            elem->blockSignals(false);
        }
    }
    blockSignals(false);
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotUpdatePointEntries(const QPointF &p, bool extremal)
{
    Q_UNUSED(p);
    Q_UNUSED(extremal);
    // Wrong slot called in curve widget
    Q_ASSERT(false);
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotShowComment(bool show)
{
    Q_UNUSED(show);
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotRefresh()
{
    if (m_model->data(m_index, AssetParameterModel::TypeRole).template value<ParamType>() == ParamType::Curve) {
        QList<QPointF> points;
        // Rounding gives really weird results. (int) (10 * 0.3) gives 2! So for now, add 0.5 to get correct result
        int number = m_model->data(m_index, AssetParameterModel::Enum3Role).toDouble() * 10 + 0.5;
        int start = m_model->data(m_index, AssetParameterModel::MinRole).toInt();
        // for the curve, inpoints are numbered: 6, 8, 10, 12, 14
        // outpoints, 7, 9, 11, 13,15 so we need to deduce these enums
        int inRef = (int)AssetParameterModel::Enum6Role + 2 * (start - 1);
        int outRef = (int)AssetParameterModel::Enum7Role + 2 * (start - 1);
        for (int j = start; j <= number; ++j) {
            double inVal = m_model->data(m_index, (AssetParameterModel::DataRoles)inRef).toDouble();
            double outVal = m_model->data(m_index, (AssetParameterModel::DataRoles)outRef).toDouble();
            points << QPointF(inVal, outVal);
            inRef += 2;
            outRef += 2;
        }
        if (!points.isEmpty()) {
            m_edit->setFromString(KisCubicCurve(points).toString());
        }
    } else {
        m_edit->setFromString(m_model->data(m_index, AssetParameterModel::ValueRole).toString());
    }
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::setMaxPoints(int max)
{
    m_edit->setMaxPoints(max);
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    emit updateHeight();
}
