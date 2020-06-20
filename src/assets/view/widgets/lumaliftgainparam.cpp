/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Some code was borrowed from shotcut                                   *
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

#include "lumaliftgainparam.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "colorwheel.h"
#include "utils/flowlayout.h"

#include <KLocalizedString>

static const double LIFT_FACTOR = 2.0;
static const double GAMMA_FACTOR = 2.0;
static const double GAIN_FACTOR = 4.0;

LumaLiftGainParam::LumaLiftGainParam(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    m_flowLayout = new FlowLayout(this, 2, 2, 2);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lift = new ColorWheel(QStringLiteral("lift"), i18n("Lift"), NegQColor(), this);
    m_lift->setFactorDefaultZero(LIFT_FACTOR, 0, 0.5);
    connect(m_lift, &ColorWheel::colorChange, this, &LumaLiftGainParam::liftChanged);
    m_gamma = new ColorWheel(QStringLiteral("gamma"), i18n("Gamma"), NegQColor(), this);
    m_gamma->setFactorDefaultZero(GAMMA_FACTOR, 1, 0);
    connect(m_gamma, &ColorWheel::colorChange, this, &LumaLiftGainParam::gammaChanged);
    m_gain = new ColorWheel(QStringLiteral("gain"), i18n("Gain"), NegQColor(), this);
    m_gain->setFactorDefaultZero(GAIN_FACTOR, 1, 0);
    connect(m_gain, &ColorWheel::colorChange, this, &LumaLiftGainParam::gainChanged);
    QMap<QString, QModelIndex> indexes;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex local_index = m_model->index(i, 0);
        QString name = m_model->data(local_index, AssetParameterModel::NameRole).toString();
        indexes.insert(name, local_index);
    }

    m_flowLayout->addWidget(m_lift);
    m_flowLayout->addWidget(m_gamma);
    m_flowLayout->addWidget(m_gain);
    setLayout(m_flowLayout);
    slotRefresh();

    connect(this, &LumaLiftGainParam::liftChanged, [this, indexes]() {
        NegQColor liftColor = m_lift->color();
        QList <QModelIndex> ixes{indexes.value(QStringLiteral("lift_r")),indexes.value(QStringLiteral("lift_g")), indexes.value(QStringLiteral("lift_b"))};
        QStringList values {QString::number(liftColor.redF(), 'f'), QString::number(liftColor.greenF(), 'f'), QString::number(liftColor.blueF(), 'f')};
        emit valuesChanged(ixes, values, true);
    });
    connect(this, &LumaLiftGainParam::gammaChanged, [this, indexes]() {
        NegQColor gammaColor = m_gamma->color();
        QList <QModelIndex> ixes{indexes.value(QStringLiteral("gamma_r")),indexes.value(QStringLiteral("gamma_g")), indexes.value(QStringLiteral("gamma_b"))};
        QStringList values {QString::number(gammaColor.redF() * GAMMA_FACTOR, 'f'), QString::number(gammaColor.greenF() * GAMMA_FACTOR, 'f'), QString::number(gammaColor.blueF() * GAMMA_FACTOR, 'f')};
        emit valuesChanged(ixes, values, true);
    });
    connect(this, &LumaLiftGainParam::gainChanged, [this, indexes]() {
        NegQColor gainColor = m_gain->color();
        QList <QModelIndex> ixes{indexes.value(QStringLiteral("gain_r")),indexes.value(QStringLiteral("gain_g")), indexes.value(QStringLiteral("gain_b"))};
        QStringList values {QString::number(gainColor.redF() * GAIN_FACTOR, 'f'), QString::number(gainColor.greenF() * GAIN_FACTOR, 'f'), QString::number(gainColor.blueF() * GAIN_FACTOR, 'f')};
        emit valuesChanged(ixes, values, true);
    });
}

void LumaLiftGainParam::updateEffect(QDomElement &effect)
{
    NegQColor lift = m_lift->color();
    NegQColor gamma = m_gamma->color();
    NegQColor gain = m_gain->color();
    QMap<QString, double> values;
    values.insert(QStringLiteral("lift_r"), lift.redF() * LIFT_FACTOR);
    values.insert(QStringLiteral("lift_g"), lift.greenF() * LIFT_FACTOR);
    values.insert(QStringLiteral("lift_b"), lift.blueF() * LIFT_FACTOR);

    values.insert(QStringLiteral("gamma_r"), gamma.redF() * GAMMA_FACTOR);
    values.insert(QStringLiteral("gamma_g"), gamma.greenF() * GAMMA_FACTOR);
    values.insert(QStringLiteral("gamma_b"), gamma.blueF() * GAMMA_FACTOR);

    values.insert(QStringLiteral("gain_r"), gain.redF() * GAIN_FACTOR);
    values.insert(QStringLiteral("gain_g"), gain.greenF() * GAIN_FACTOR);
    values.insert(QStringLiteral("gain_b"), gain.blueF() * GAIN_FACTOR);

    QDomNodeList namenode = effect.childNodes();
    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        if (pa.tagName() != QLatin1String("parameter")) {
            continue;
        }
        if (values.contains(pa.attribute(QStringLiteral("name")))) {
            pa.setAttribute(QStringLiteral("value"), (int)(values.value(pa.attribute(QStringLiteral("name"))) *
                                                           pa.attribute(QStringLiteral("factor"), QStringLiteral("1")).toDouble()));
        }
    }
}

void LumaLiftGainParam::resizeEvent(QResizeEvent *ev)
{
    QWidget::resizeEvent(ev);
    if (height() != m_flowLayout->miniHeight()) {
        emit updateHeight(m_flowLayout->miniHeight());
    }
}

void LumaLiftGainParam::slotShowComment(bool) {}

void LumaLiftGainParam::slotRefresh()
{
    QMap<QString, double> values;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex local_index = m_model->index(i, 0);
        QString name = m_model->data(local_index, AssetParameterModel::NameRole).toString();
        double val = m_model->data(local_index, AssetParameterModel::ValueRole).toDouble();
        values.insert(name, val);
    }
    m_lift->setColor({values.value(QStringLiteral("lift_r")), values.value(QStringLiteral("lift_g")), values.value(QStringLiteral("lift_b"))});
    m_gamma->setColor({values.value(QStringLiteral("gamma_r")), values.value(QStringLiteral("gamma_g")), values.value(QStringLiteral("gamma_b"))});
    m_gain->setColor({values.value(QStringLiteral("gain_r")), values.value(QStringLiteral("gain_g")), values.value(QStringLiteral("gain_b"))});
}
