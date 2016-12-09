/***************************************************************************
 *   Copyright (C) 2014 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "effectstack/widgets/lumaliftgain.h"
#include "effectstack/widgets/colorwheel.h"
#include "utils/flowlayout.h"

#include <KLocalizedString>

static const double GAMMA_FACTOR = 2.0;
static const double GAIN_FACTOR = 4.0;

LumaLiftGain::LumaLiftGain(const QDomNodeList &nodes, QWidget *parent) :
    QWidget(parent)
{
    FlowLayout *flowLayout = new FlowLayout(this, 2, 2, 2);
    /*QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);*/

    QMap<QString, double> values;
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement pa = nodes.item(i).toElement();
        if (pa.tagName() != QLatin1String("parameter")) {
            continue;
        }
        double val = m_locale.toDouble(pa.attribute(QStringLiteral("value"))) / m_locale.toDouble(pa.attribute(QStringLiteral("factor")));
        values.insert(pa.attribute(QStringLiteral("name")), val);
    }

    QColor lift = QColor::fromRgbF(values.value(QStringLiteral("lift_r")),
                                   values.value(QStringLiteral("lift_g")),
                                   values.value(QStringLiteral("lift_b")));
    QColor gamma = QColor::fromRgbF(values.value(QStringLiteral("gamma_r")) / GAMMA_FACTOR,
                                    values.value(QStringLiteral("gamma_g")) / GAMMA_FACTOR,
                                    values.value(QStringLiteral("gamma_b")) / GAMMA_FACTOR);
    QColor gain = QColor::fromRgbF(values.value(QStringLiteral("gain_r")) / GAIN_FACTOR,
                                   values.value(QStringLiteral("gain_g")) / GAIN_FACTOR,
                                   values.value(QStringLiteral("gain_b")) / GAIN_FACTOR);

    m_lift = new ColorWheel(QStringLiteral("lift"), i18n("Lift"), lift, this);
    connect(m_lift, &ColorWheel::colorChange, this, &LumaLiftGain::valueChanged);
    m_gamma = new ColorWheel(QStringLiteral("gamma"), i18n("Gamma"), gamma, this);
    connect(m_gamma, &ColorWheel::colorChange, this, &LumaLiftGain::valueChanged);
    m_gain = new ColorWheel(QStringLiteral("gain"), i18n("Gain"), gain, this);
    connect(m_gain, &ColorWheel::colorChange, this, &LumaLiftGain::valueChanged);

    flowLayout->addWidget(m_lift);
    flowLayout->addWidget(m_gamma);
    flowLayout->addWidget(m_gain);
    setLayout(flowLayout);

    /*layout->addWidget(label);
    layout->addWidget(m_lift);
    layout->addWidget(label2);
    layout->addWidget(m_gamma);
    layout->addWidget(label3);
    layout->addWidget(m_gain);
    setLayout(layout);*/
}

void LumaLiftGain::updateEffect(QDomElement &effect)
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
            pa.setAttribute(QStringLiteral("value"), (int)(values.value(pa.attribute(QStringLiteral("name"))) * m_locale.toDouble(pa.attribute(QStringLiteral("factor"), QStringLiteral("1")))));
        }
    }
}

