/*
 * SPDX-FileCopyrightText: 2016 Nicolas Carion
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "bezier/beziersplineeditor.h"
#include "cubic/kis_curve_widget.h"
#include "curveparamwidget.h"
#include "kdenlivesettings.h"
#include "utils/colortools.h"
#include "widgets/dragvalue.h"
#include <KLocalizedString>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

/** @brief this label is a pixmap corresponding to a legend of the axis*/
template <typename CurveWidget_t> class ValueLabel : public QLabel
{
public:
    /** @brief Creates the widget
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

public Q_SLOTS:
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
            setPixmap(QPixmap::fromImage(ColorTools::rgbCurveLine(s, color, palette().window().color().rgb())).transformed(t));
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

static bool isIdentityCurve(const QString &s)
{
    return s.isEmpty() || s == QLatin1String("0/0;1/1;");
}

template <> void CurveParamWidget<KisCurveWidget>::slotTabChanged(int index)
{
    if (index < 0 || index >= m_tabIndexes.size()) return;

    // update m_index before loading so the modified signal fires with the correct index
    m_index = m_tabIndexes.at(index);

    // block the editor while loading to avoid spurious valueChanged
    m_edit->blockSignals(true);
    const QString stored = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    if (!stored.isEmpty()) {
        m_edit->setFromString(stored);
    } else {
        m_edit->reset();
    }
    m_edit->blockSignals(false);

    // clear point spinboxes on tab switch — no point is selected yet
    if (m_inSpin && m_outSpin) {
        QSignalBlocker bk1(m_inSpin);
        QSignalBlocker bk2(m_outSpin);
        m_inSpin->setValue(0.0);
        m_outSpin->setValue(0.0);
        m_inSpin->setEnabled(false);
        m_outSpin->setEnabled(false);
    }

    // apply the curve line color for this tab
    if (index < m_tabColors.size()) {
        m_edit->setCurveColor(m_tabColors.at(index));
    }

    // build ghost curves from every other channel that has been edited
    QList<QPair<KisCubicCurve, QColor>> ghosts;
    for (int i = 0; i < m_tabIndexes.size(); ++i) {
        if (i == index) continue;
        const QString val = m_model->data(m_tabIndexes.at(i), AssetParameterModel::ValueRole).toString();
        if (isIdentityCurve(val)) continue;
        KisCubicCurve curve;
        curve.fromString(val);
        const QColor color = (i < m_tabColors.size()) ? m_tabColors.at(i) : QColor();
        ghosts.append({curve, color});
    }
    m_edit->setGhostCurves(ghosts);

    // update background pixmap to match channel color
    const CurveModes modes[] = {CurveModes::RGB, CurveModes::Red, CurveModes::Green, CurveModes::Blue};
    if (index < 4) {
        m_mode = modes[index];
    }
    if (m_showPixmap) {
        slotShowPixmap(true);
    }
}

// no-op for BezierSplineEditor, keeps existing combobox
template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotTabChanged(int index)
{
    Q_UNUSED(index);
}

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
    layout->setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    layout->addWidget(m_edit);
    m_edit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *widget = new QWidget(this);
    widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_ui.setupUi(widget);
    widget->setFixedHeight(widget->minimumHeight());
    layout->addWidget(widget);

    // set up icons and initial button states
    m_ui.buttonShowPixmap->setIcon(QIcon(QPixmap::fromImage(ColorTools::rgbCurvePlane(QSize(16, 16), ColorTools::ColorsRGB::Luma, 0.8))));
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
    connect(m_edit, &CurveWidget_t::modified, [this]() { Q_EMIT valueChanged(m_index, m_edit->toString(), true); });
}

template <> void CurveParamWidget<KisCurveWidget>::deleteIrrelevantItems()
{
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
    connect(m_pX, &DragValue::customValueChanged, this, &CurveParamWidget<CurveWidget_t>::slotUpdatePointP);
    connect(m_pY, &DragValue::customValueChanged, this, &CurveParamWidget<CurveWidget_t>::slotUpdatePointP);
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
    connect(m_h1X, &DragValue::customValueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1);
    connect(m_h1Y, &DragValue::customValueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH1);
    connect(m_h2X, &DragValue::customValueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2);
    connect(m_h2Y, &DragValue::customValueChanged, this, &CurveParamWidget<BezierSplineEditor>::slotUpdatePointH2);
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

template <> void CurveParamWidget<KisCurveWidget>::slotRefresh()
{
    const ParamType paramType = m_model->data(m_index, AssetParameterModel::TypeRole).value<ParamType>();
    if (paramType == ParamType::AvCurve) {
        // each AvCurve param is a single channel — load directly from m_index
        const QString stored = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
        if (!stored.isEmpty()) {
            m_edit->setFromString(stored);
        } else {
            m_edit->reset();
        }
        // rebuild ghosts from model so saved-project state shows correctly on first load
        const int activeTab = m_tabBar ? m_tabBar->currentIndex() : 0;
        QList<QPair<KisCubicCurve, QColor>> ghosts;
        for (int i = 0; i < m_tabIndexes.size(); ++i) {
            if (i == activeTab) continue;
            const QString val = m_model->data(m_tabIndexes.at(i), AssetParameterModel::ValueRole).toString();
            if (isIdentityCurve(val)) continue;
            KisCubicCurve curve;
            curve.fromString(val);
            const QColor color = (i < m_tabColors.size()) ? m_tabColors.at(i) : QColor();
            ghosts.append({curve, color});
        }
        m_edit->setGhostCurves(ghosts);
    } else {
        // frei0r.curves / legacy Curve type: single-channel
        if (paramType == ParamType::Curve) {
            QList<QPointF> points;
            int number = qRound(m_model->data(m_index, AssetParameterModel::Enum3Role).toDouble() * 10);
            int start = m_model->data(m_index, AssetParameterModel::MinRole).toInt();
            int inRef = int(AssetParameterModel::Enum6Role) + 2 * (start - 1);
            int outRef = int(AssetParameterModel::Enum7Role) + 2 * (start - 1);
            for (int j = start; j <= number; ++j) {
                double inVal = m_model->data(m_index, AssetParameterModel::DataRoles(inRef)).toDouble();
                double outVal = m_model->data(m_index, AssetParameterModel::DataRoles(outRef)).toDouble();
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
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::slotRefresh()
{
    if (m_model->data(m_index, AssetParameterModel::TypeRole).template value<ParamType>() == ParamType::Curve) {
        QList<QPointF> points;
        int number = qRound(m_model->data(m_index, AssetParameterModel::Enum3Role).toDouble() * 10);
        int start = m_model->data(m_index, AssetParameterModel::MinRole).toInt();
        // for the curve, inpoints are numbered: 6, 8, 10, 12, 14
        // outpoints, 7, 9, 11, 13,15 so we need to deduce these enums
        int inRef = int(AssetParameterModel::Enum6Role) + 2 * (start - 1);
        int outRef = int(AssetParameterModel::Enum7Role) + 2 * (start - 1);
        for (int j = start; j <= number; ++j) {
            double inVal = m_model->data(m_index, AssetParameterModel::DataRoles(inRef)).toDouble();
            double outVal = m_model->data(m_index, AssetParameterModel::DataRoles(outRef)).toDouble();
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
    Q_EMIT updateHeight();
}

template <>
CurveParamWidget<KisCurveWidget>::CurveParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_mode(CurveModes::RGB)
    , m_showPixmap(KdenliveSettings::bezier_showpixmap())
{
    // construct curve editor
    m_edit = new KisCurveWidget(this);
    connect(m_edit, static_cast<void (KisCurveWidget::*)(const Point_t &, bool)>(&KisCurveWidget::currentPoint), this,
            static_cast<void (CurveParamWidget<KisCurveWidget>::*)(const Point_t &, bool)>(&CurveParamWidget<KisCurveWidget>::slotUpdatePointEntries));

    // construct and fill layout
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    const bool isAvCurve = m_model->data(m_index, AssetParameterModel::TypeRole).value<ParamType>() == ParamType::AvCurve;

    // frei0r.curves is limited to 5 control points; avfilter.curves has no such limit
    if (!isAvCurve) {
        m_edit->setMaxPoints(5);
    }

    // create tab bar (only for AvCurve; AssetParameterView will add tabs for the other channel params)
    m_tabBar = new QTabBar(this);
    m_tabBar->setExpanding(false);
    m_tabBar->setVisible(isAvCurve);

    auto *tabRowLayout = new QHBoxLayout();
    tabRowLayout->setSpacing(0);
    tabRowLayout->setContentsMargins(0, 0, 0, 0);
    tabRowLayout->addWidget(m_tabBar);
    tabRowLayout->addStretch(1);
    layout->addLayout(tabRowLayout);

    if (isAvCurve) {
        auto *pointRow = new QHBoxLayout();
        pointRow->setSpacing(4);
        pointRow->setContentsMargins(2, 2, 2, 2);

        auto *inLabel = new QLabel(i18n("In:"), this);
        m_inSpin = new QDoubleSpinBox(this);
        m_inSpin->setRange(0.0, 1.0);
        m_inSpin->setDecimals(3);
        m_inSpin->setSingleStep(0.001);
        m_inSpin->setEnabled(false);

        auto *outLabel = new QLabel(i18n("Out:"), this);
        m_outSpin = new QDoubleSpinBox(this);
        m_outSpin->setRange(0.0, 1.0);
        m_outSpin->setDecimals(3);
        m_outSpin->setSingleStep(0.001);
        m_outSpin->setEnabled(false);

        auto *resetChannelBtn = new QToolButton(this);
        resetChannelBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
        resetChannelBtn->setText(i18n("Reset curve"));
        resetChannelBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        resetChannelBtn->setAutoRaise(true);
        connect(resetChannelBtn, &QAbstractButton::clicked, m_edit, &KisCurveWidget::reset);

        pointRow->addWidget(inLabel);
        pointRow->addWidget(m_inSpin);
        pointRow->addSpacing(8);
        pointRow->addWidget(outLabel);
        pointRow->addWidget(m_outSpin);
        pointRow->addStretch(1);
        pointRow->addWidget(resetChannelBtn);
        layout->addLayout(pointRow);
    }

    if (isAvCurve) {
        // first tab label comes from this param's display name
        const QString firstName = m_model->data(m_index, Qt::DisplayRole).toString();
        m_tabBar->addTab(firstName);
        m_tabIndexes.append(m_index);
        const QString colorStr = m_model->data(m_index, AssetParameterModel::CurveColorRole).toString();
        m_tabColors.append(colorStr.isEmpty() ? QColor() : QColor(colorStr));
        if (!m_tabColors.isEmpty() && m_tabColors.first().isValid()) {
            m_edit->setCurveColor(m_tabColors.first());
        }
    }

    layout->addWidget(m_edit);
    m_edit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *widget = new QWidget(this);
    widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_ui.setupUi(widget);
    widget->setFixedHeight(widget->minimumHeight());
    layout->addWidget(widget);

    // set up icons and initial button states
    m_ui.buttonShowPixmap->setIcon(QIcon(QPixmap::fromImage(ColorTools::rgbCurvePlane(QSize(16, 16), ColorTools::ColorsRGB::RGB, 0.8))));
    m_ui.widgetPoint->setEnabled(false);
    m_edit->setGridLines(KdenliveSettings::bezier_gridlines());
    m_ui.buttonShowPixmap->setChecked(KdenliveSettings::bezier_showpixmap());

    // connect tab bar
    connect(m_tabBar, &QTabBar::currentChanged, this, &CurveParamWidget<KisCurveWidget>::slotTabChanged);

    // connect buttons
    connect(m_ui.buttonDeletePoint, &QAbstractButton::clicked, m_edit, &KisCurveWidget::slotDeleteCurrentPoint);
    connect(m_ui.buttonZoomIn, &QAbstractButton::clicked, m_edit, &KisCurveWidget::slotZoomIn);
    connect(m_ui.buttonZoomOut, &QAbstractButton::clicked, m_edit, &KisCurveWidget::slotZoomOut);
    connect(m_ui.buttonGridChange, &QAbstractButton::clicked, this, &CurveParamWidget<KisCurveWidget>::slotGridChange);
    connect(m_ui.buttonShowPixmap, &QAbstractButton::toggled, this, &CurveParamWidget<KisCurveWidget>::slotShowPixmap);
    connect(m_ui.buttonResetSpline, &QAbstractButton::clicked, m_edit, &KisCurveWidget::reset);

    if (isAvCurve) {
        connect(m_edit, static_cast<void (KisCurveWidget::*)(const QPointF &, bool)>(&KisCurveWidget::currentPoint), this, [this](const QPointF &p, bool extremal) {
            if (!m_inSpin || !m_outSpin) return;
            if (p == QPointF()) {
                m_inSpin->setEnabled(false);
                m_outSpin->setEnabled(false);
            } else {
                m_inSpin->setEnabled(!extremal);
                m_outSpin->setEnabled(true);
                QSignalBlocker bk1(m_inSpin);
                QSignalBlocker bk2(m_outSpin);
                m_inSpin->setValue(p.x());
                m_outSpin->setValue(p.y());
            }
        });

        connect(m_inSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
            QPointF p = m_edit->getCurrentPoint();
            if (p == QPointF()) return;
            m_edit->updateCurrentPoint(QPointF(val, p.y()));
        });

        connect(m_outSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
            QPointF p = m_edit->getCurrentPoint();
            if (p == QPointF()) return;
            m_edit->updateCurrentPoint(QPointF(p.x(), val));
        });
    }

    setupLayoutPoint();
    slotRefresh();
    deleteIrrelevantItems();

    // emit valueChanged for the currently active index; also refresh ghosts so
    // other tabs see an up-to-date curve if the user edits this one then switches away
    connect(m_edit, &KisCurveWidget::modified, [this]() {
        Q_EMIT valueChanged(m_index, m_edit->toString(), true);
        if (!m_tabIndexes.isEmpty()) {
            const int activeTab = m_tabBar ? m_tabBar->currentIndex() : 0;
            QList<QPair<KisCubicCurve, QColor>> ghosts;
            for (int i = 0; i < m_tabIndexes.size(); ++i) {
                if (i == activeTab) continue;
                const QString val = m_model->data(m_tabIndexes.at(i), AssetParameterModel::ValueRole).toString();
                if (isIdentityCurve(val)) continue;
                KisCubicCurve curve;
                curve.fromString(val);
                const QColor color = (i < m_tabColors.size()) ? m_tabColors.at(i) : QColor();
                ghosts.append({curve, color});
            }
            m_edit->setGhostCurves(ghosts);
        }
    });
}

template <> void CurveParamWidget<KisCurveWidget>::addAvCurveTab(const QModelIndex &index)
{
    if (!m_tabBar) return;
    const QString label = m_model->data(index, Qt::DisplayRole).toString();
    m_tabBar->addTab(label);
    m_tabIndexes.append(index);
    const QString colorStr = m_model->data(index, AssetParameterModel::CurveColorRole).toString();
    m_tabColors.append(colorStr.isEmpty() ? QColor() : QColor(colorStr));

    // rebuild ghosts each time a tab is added so that by the time the last tab
    // is registered the widget already has the full picture from saved state
    const int activeTab = m_tabBar->currentIndex();
    QList<QPair<KisCubicCurve, QColor>> ghosts;
    for (int i = 0; i < m_tabIndexes.size(); ++i) {
        if (i == activeTab) continue;
        const QString val = m_model->data(m_tabIndexes.at(i), AssetParameterModel::ValueRole).toString();
        if (isIdentityCurve(val)) continue;
        KisCubicCurve curve;
        curve.fromString(val);
        const QColor color = (i < m_tabColors.size()) ? m_tabColors.at(i) : QColor();
        ghosts.append({curve, color});
    }
    m_edit->setGhostCurves(ghosts);
}

template <typename CurveWidget_t> void CurveParamWidget<CurveWidget_t>::addAvCurveTab(const QModelIndex & /*index*/)
{
    // Not supported for non-KisCurveWidget types
}
