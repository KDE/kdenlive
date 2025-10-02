/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "abstractparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "boolparamwidget.hpp"
#include "buttonparamwidget.hpp"
#include "clickablelabelwidget.hpp"
#include "coloreditwidget.hpp"
#include "curves/bezier/beziersplineeditor.h"
#include "curves/cubic/kis_cubic_curve.h"
#include "curves/cubic/kis_curve_widget.h"
#include "curves/curveparamwidget.h"
#include "doubleparamwidget.hpp"
#include "effectbuttonsparamwidget.hpp"
#include "fontparamwidget.hpp"
#include "geometryeditwidget.hpp"
#include "hideparamwidget.hpp"
#include "keyframecontainer.hpp"
#include "keywordparamwidget.hpp"
#include "listdependencyparamwidget.h"
#include "listparamwidget.h"
#include "lumaliftgainparam.hpp"
#include "multiswitchparamwidget.hpp"
#include "positioneditwidget.hpp"
#include "slidewidget.hpp"
#include "switchparamwidget.hpp"
#include "urllistparamwidget.h"
#include "urlparamwidget.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <utility>

// temporary place holder for parameters that don't currently have a display class
class Unsupported : public AbstractParamWidget
{
public:
    Unsupported(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
        : AbstractParamWidget(std::move(model), index, parent)
    {
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 0, 4, 0);
        m_label = new QLabel(this);
        lay->addWidget(m_label);
    }
    void setText(const QString &str) { m_label->setText(str); }
    void slotRefresh() override {}

private:
    QLabel *m_label;
};

AbstractParamWidget::AbstractParamWidget(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, QWidget *parent)
    : QWidget(parent)
    , m_model(std::move(model))
    , m_index(index)
{
    // setup comment
    const QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
}

QLabel *AbstractParamWidget::createLabel()
{
    auto *label = new QLabel(m_model->data(m_index, Qt::DisplayRole).toString());
    return label;
}

std::pair<AbstractParamWidget *, KeyframeContainer *> AbstractParamWidget::construct(const std::shared_ptr<AssetParameterModel> &model,
                                                                                     const QModelIndex &index, QSize frameSize, QWidget *parent,
                                                                                     QFormLayout *layout)
{
    // We retrieve the parameter type
    auto type = model->data(index, AssetParameterModel::TypeRole).value<ParamType>();

    if (AssetParameterModel::isAnimated(type)) {
        auto kfrBuilder = new KeyframeContainer(model, index, frameSize, parent, layout);
        return {nullptr, kfrBuilder};
    }

    AbstractParamWidget *widget = nullptr;
    QString name = model->data(index, AssetParameterModel::NameRole).toString();

    switch (type) {
    case ParamType::Double:
        widget = new DoubleParamWidget(model, index, parent);
        break;
    case ParamType::ListWithDependency:
        widget = new ListDependencyParamWidget(model, index, parent);
        break;
    case ParamType::List:
        widget = new ListParamWidget(model, index, parent);
        break;
    case ParamType::UrlList:
        widget = new UrlListParamWidget(model, index, parent);
        break;
    case ParamType::Bool:
        widget = new BoolParamWidget(model, index, parent);
        break;
    case ParamType::Geometry:
    case ParamType::FakeRect:
        widget = new GeometryEditWidget(model, index, frameSize, parent, layout);
        break;
    case ParamType::Position:
        widget = new PositionEditWidget(model, index, parent);
        break;
    case ParamType::FixedColor:
        widget = new ColorEditWidget(model, index, parent);
        break;
    case ParamType::Wipe:
        widget = new SlideWidget(model, index, parent);
        break;
    case ParamType::Switch:
        widget = new SwitchParamWidget(model, index, parent);
        break;
    case ParamType::EffectButtons:
        widget = new EffectButtonsParamWidget(model, index, parent);
        break;
    case ParamType::MultiSwitch:
        widget = new MultiSwitchParamWidget(model, index, parent);
        break;
    case ParamType::Url:
        widget = new UrlParamWidget(model, index, parent);
        break;
    case ParamType::Bezier_spline: {
        using Widget_t = CurveParamWidget<BezierSplineEditor>;
        widget = new Widget_t(model, index, parent);
        break;
    }
    case ParamType::Curve: {
        using Widget_t = CurveParamWidget<KisCurveWidget>;
        widget = new Widget_t(model, index, parent);
        break;
    }
    case ParamType::Hidden: {
        widget = new HideParamWidget(model, index, parent);
        break;
    }
    case ParamType::Filterjob: {
        widget = new ButtonParamWidget(model, index, parent);
        break;
    }
    case ParamType::Readonly: {
        widget = new ClickableLabelParamWidget(model, index, parent);
        break;
    }
    case ParamType::Fontfamily: {
        widget = new FontParamWidget(model, index, parent);
        break;
    }
    case ParamType::Keywords: {
        widget = new KeywordParamWidget(model, index, parent);
        break;
    }
    default:
        // not reimplemented
        widget = new Unsupported(model, index, parent);
        static_cast<Unsupported *>(widget)->setText(name);
    }

    return {widget, nullptr};
}
