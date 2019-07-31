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

#include "abstractparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "buttonparamwidget.hpp"
#include "boolparamwidget.hpp"
#include "fontparamwidget.hpp"
#include "keywordparamwidget.hpp"
#include "coloreditwidget.hpp"
#include "curves/bezier/beziersplineeditor.h"
#include "curves/cubic/kis_cubic_curve.h"
#include "curves/cubic/kis_curve_widget.h"
#include "curves/curveparamwidget.h"
#include "doubleparamwidget.hpp"
#include "clickablelabelwidget.hpp"
#include "geometryeditwidget.hpp"
#include "hideparamwidget.hpp"
#include "keyframewidget.hpp"
#include "listparamwidget.h"
#include "lumaliftgainparam.hpp"
#include "positioneditwidget.hpp"
#include "slidewidget.hpp"
#include "switchparamwidget.hpp"
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

protected:
    QLabel *m_label;
};

AbstractParamWidget::AbstractParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : QWidget(parent)
    , m_model(std::move(model))
    , m_index(index)
{
}

AbstractParamWidget *AbstractParamWidget::construct(const std::shared_ptr<AssetParameterModel> &model, QModelIndex index, QSize frameSize, QWidget *parent)
{
    // We retrieve the parameter type
    auto type = model->data(index, AssetParameterModel::TypeRole).value<ParamType>();

    QString name = model->data(index, AssetParameterModel::NameRole).toString();

    AbstractParamWidget *widget = nullptr;

    switch (type) {
    case ParamType::Double:
        widget = new DoubleParamWidget(model, index, parent);
        break;
    case ParamType::List:
        widget = new ListParamWidget(model, index, parent);
        break;
    case ParamType::Bool:
        widget = new BoolParamWidget(model, index, parent);
        break;
    case ParamType::KeyframeParam:
    case ParamType::AnimatedRect:
    case ParamType::Roto_spline:
        widget = new KeyframeWidget(model, index, frameSize, parent);
        break;
    case ParamType::Geometry:
        widget = new GeometryEditWidget(model, index, frameSize, parent);
        break;
    case ParamType::Position:
        widget = new PositionEditWidget(model, index, parent);
        break;
    case ParamType::Color:
        widget = new ColorEditWidget(model, index, parent);
        break;
    case ParamType::ColorWheel:
        widget = new LumaLiftGainParam(model, index, parent);
        break;
    case ParamType::Wipe:
        widget = new SlideWidget(model, index, parent);
        break;
    case ParamType::Switch:
        widget = new SwitchParamWidget(model, index, parent);
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
    case ParamType::Animated:
    case ParamType::RestrictedAnim:
    //        widget = new AnimationWidget(model, index, range, parent);
    //        break;
    //  case ParamType::KeyframeParam:
    //        widget = new KeyframeEdit(model, index, parent);
    //        break;
    case ParamType::Addedgeometry:
        // not reimplemented
        widget = new Unsupported(model, index, parent);
        static_cast<Unsupported *>(widget)->setText(name);
    }

    return widget;
}
