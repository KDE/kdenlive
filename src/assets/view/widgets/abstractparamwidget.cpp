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
#include "animationwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "boolparamwidget.hpp"
#include "lumaliftgainparam.hpp"
#include "doubleparamwidget.hpp"
#include "geometryeditwidget.hpp"
#include "keyframeedit.h"
#include "keyframewidget.hpp"
#include "listparamwidget.h"
#include "positioneditwidget.hpp"
#include "coloreditwidget.hpp"

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
    void slotSetRange(QPair<int, int>) override {}

protected:
    QLabel *m_label;
};

AbstractParamWidget::AbstractParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : QWidget(parent)
    , m_model(std::move(model))
    , m_index(index)
{
}

AbstractParamWidget *AbstractParamWidget::construct(const std::shared_ptr<AssetParameterModel> &model, QModelIndex index, QPair<int, int> range,
                                                    QSize frameSize, QWidget *parent)
{
    // We retrieve the parameter type
    auto type = model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
    qDebug() << "paramtype " << (int)type;

    QString name = model->data(index, AssetParameterModel::NameRole).toString();

    AbstractParamWidget *widget;

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
        widget = new KeyframeWidget(model, index, parent);
        break;
    case ParamType::Geometry:
        widget = new GeometryEditWidget(model, index, range, frameSize, parent);
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
    case ParamType::Animated:
    case ParamType::RestrictedAnim:
    //        widget = new AnimationWidget(model, index, range, parent);
    //        break;
    //  case ParamType::KeyframeParam:
    //        widget = new KeyframeEdit(model, index, parent);
    //        break;
    case ParamType::Switch:
    case ParamType::Addedgeometry:
    case ParamType::Curve:
    case ParamType::Bezier_spline:
    case ParamType::Roto_spline:
    case ParamType::Wipe:
    case ParamType::Url:
    case ParamType::Keywords:
    case ParamType::Fontfamily:
    case ParamType::Filterjob:
    case ParamType::Readonly:
        // not reimplemented
        widget = new Unsupported(model, index, parent);
        static_cast<Unsupported *>(widget)->setText(name);
    }

    return widget;
}
