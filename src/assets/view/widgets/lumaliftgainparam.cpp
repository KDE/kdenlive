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

static const double GAMMA_FACTOR = 2.0;
static const double GAIN_FACTOR = 4.0;

LumaLiftGainParam::LumaLiftGainParam(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    auto *flowLayout = new FlowLayout(this, 2, 2, 2);
    /*QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
*/
    m_locale.setNumberOptions(QLocale::OmitGroupSeparator);
    m_lift = new ColorWheel(QStringLiteral("lift"), i18n("Lift"), QColor(), this);
    connect(m_lift, &ColorWheel::colorChange, this, &LumaLiftGainParam::liftChanged);
    m_gamma = new ColorWheel(QStringLiteral("gamma"), i18n("Gamma"), QColor(), this);
    connect(m_gamma, &ColorWheel::colorChange, this, &LumaLiftGainParam::gammaChanged);
    m_gain = new ColorWheel(QStringLiteral("gain"), i18n("Gain"), QColor(), this);
    connect(m_gain, &ColorWheel::colorChange, this, &LumaLiftGainParam::gainChanged);
    QMap<QString, QModelIndex> indexes;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex local_index = m_model->index(i, 0);
        QString name = m_model->data(local_index, AssetParameterModel::NameRole).toString();
        indexes.insert(name, local_index);
    }

    flowLayout->addWidget(m_lift);
    flowLayout->addWidget(m_gamma);
    flowLayout->addWidget(m_gain);
    setLayout(flowLayout);
    slotRefresh();

    connect(this, &LumaLiftGainParam::liftChanged,
            [this, indexes]() {
                QColor liftColor = m_lift->color();
                emit valueChanged(indexes.value(QStringLiteral("lift_r")), m_locale.toString(liftColor.redF()), true);
                emit valueChanged(indexes.value(QStringLiteral("lift_g")), m_locale.toString(liftColor.greenF()), true);
                emit valueChanged(indexes.value(QStringLiteral("lift_b")), m_locale.toString(liftColor.blueF()), true);
            });
    connect(this, &LumaLiftGainParam::gammaChanged,
            [this, indexes]() {
                QColor gammaColor = m_gamma->color();
                emit valueChanged(indexes.value(QStringLiteral("gamma_r")), m_locale.toString(gammaColor.redF() * GAMMA_FACTOR), true);
                emit valueChanged(indexes.value(QStringLiteral("gamma_g")), m_locale.toString(gammaColor.greenF() * GAMMA_FACTOR), true);
                emit valueChanged(indexes.value(QStringLiteral("gamma_b")), m_locale.toString(gammaColor.blueF() * GAMMA_FACTOR), true);
            });
    connect(this, &LumaLiftGainParam::gainChanged,
            [this, indexes]() {
                QColor gainColor = m_gain->color();
                emit valueChanged(indexes.value(QStringLiteral("gain_r")), m_locale.toString(gainColor.redF()* GAIN_FACTOR), true);
                emit valueChanged(indexes.value(QStringLiteral("gain_g")), m_locale.toString(gainColor.greenF()* GAIN_FACTOR), true);
                emit valueChanged(indexes.value(QStringLiteral("gain_b")), m_locale.toString(gainColor.blueF()* GAIN_FACTOR), true);
            });
}

void LumaLiftGainParam::updateEffect(QDomElement &effect)
{
    QColor lift = m_lift->color();
    QColor gamma = m_gamma->color();
    QColor gain = m_gain->color();
    QMap<QString, double> values;
    values.insert(QStringLiteral("lift_r"), lift.redF());
    values.insert(QStringLiteral("lift_g"), lift.greenF());
    values.insert(QStringLiteral("lift_b"), lift.blueF());

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
                                                           m_locale.toDouble(pa.attribute(QStringLiteral("factor"), QStringLiteral("1")))));
        }
    }
}

void LumaLiftGainParam::slotShowComment(bool )
{
}

void LumaLiftGainParam::slotRefresh()
{
    qDebug()<<"//REFRESHING WIDGET START--------------__";
    QMap<QString, double> values;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex local_index = m_model->index(i, 0);
        QString name = m_model->data(local_index, AssetParameterModel::NameRole).toString();
        double val = m_locale.toDouble(m_model->data(local_index, AssetParameterModel::ValueRole).toString());
        values.insert(name, val);
    }

    QColor lift = QColor::fromRgbF(values.value(QStringLiteral("lift_r")), values.value(QStringLiteral("lift_g")), values.value(QStringLiteral("lift_b")));
    QColor gamma = QColor::fromRgbF(values.value(QStringLiteral("gamma_r")) / GAMMA_FACTOR, values.value(QStringLiteral("gamma_g")) / GAMMA_FACTOR,
                                    values.value(QStringLiteral("gamma_b")) / GAMMA_FACTOR);
    QColor gain = QColor::fromRgbF(values.value(QStringLiteral("gain_r")) / GAIN_FACTOR, values.value(QStringLiteral("gain_g")) / GAIN_FACTOR,
                                   values.value(QStringLiteral("gain_b")) / GAIN_FACTOR);
    qDebug()<<"//REFRESHING WIDGET START 2--------------__";
    m_lift->setColor(lift);
    m_gamma->setColor(gamma);
    m_gain->setColor(gain);
    qDebug()<<"//REFRESHING WIDGET START DONE--------------__";
}

void LumaLiftGainParam::slotSetRange(QPair<int, int>)
{
}
