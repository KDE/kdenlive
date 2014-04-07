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


#include "widgets/lumaliftgain.h"
#include "widgets/colorwheel.h"

#include <QLabel>
#include <QHBoxLayout>

#include <KDebug>
#include <KLocale>

static const double LIFT_FACTOR = 1.0;
static const double GAMMA_FACTOR = 2.0;
static const double GAIN_FACTOR = 4.0;

LumaLiftGain::LumaLiftGain(const QDomNodeList &nodes, QWidget* parent) :
        QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    QMap <QString, double> values;
    for (int i = 0; i < nodes.count() ; ++i) {
        QDomElement pa = nodes.item(i).toElement();
        if (pa.tagName() != "parameter") continue;
        double val = m_locale.toDouble(pa.attribute("value")) / m_locale.toDouble(pa.attribute("factor"));
        values.insert(pa.attribute("name"), val);
    }
          
    QColor lift = QColor::fromRgbF(values.value("lift_r"),
                                     values.value("lift_g"),
                                     values.value("lift_b"));
    QColor gamma = QColor::fromRgbF(values.value("gamma_r") / GAMMA_FACTOR,
                                      values.value("gamma_g") / GAMMA_FACTOR,
                                      values.value("gamma_b") / GAMMA_FACTOR);
    QColor gain = QColor::fromRgbF(values.value("gain_r") / GAIN_FACTOR,
                                     values.value("gain_g") / GAIN_FACTOR,
                                     values.value("gain_b") / GAIN_FACTOR);

    QLabel *label = new QLabel(i18n("Lift"), this);
    m_lift = new ColorWheel(lift, this);
    connect(m_lift, SIGNAL(colorChange(const QColor &)), this, SIGNAL(valueChanged()));
    QLabel *label2 = new QLabel(i18n("Gamma"), this);
    m_gamma = new ColorWheel(gamma, this);
    connect(m_gamma, SIGNAL(colorChange(const QColor &)), this, SIGNAL(valueChanged()));
    QLabel *label3 = new QLabel(i18n("Gain"), this);
    m_gain = new ColorWheel(gain, this);
    connect(m_gain, SIGNAL(colorChange(const QColor &)), this, SIGNAL(valueChanged()));

    layout->addWidget(label);
    layout->addWidget(m_lift);
    layout->addWidget(label2);
    layout->addWidget(m_gamma);
    layout->addWidget(label3);
    layout->addWidget(m_gain);
    setLayout(layout);
}

void LumaLiftGain::updateEffect(QDomElement &effect)
{
    QColor lift = m_lift->color();
    QColor gamma = m_gamma->color();
    QColor gain = m_gain->color();
    QMap <QString, double> values;
    values.insert("lift_r", lift.redF());
    values.insert("lift_g", lift.greenF());
    values.insert("lift_b", lift.blueF());
    
    values.insert("gamma_r", gamma.redF() * GAMMA_FACTOR);
    values.insert("gamma_g", gamma.greenF() * GAMMA_FACTOR);
    values.insert("gamma_b", gamma.blueF() * GAMMA_FACTOR);
    
    values.insert("gain_r", gain.redF() * GAIN_FACTOR);
    values.insert("gain_g", gain.greenF() * GAIN_FACTOR);
    values.insert("gain_b", gain.blueF() * GAIN_FACTOR);
    
    QDomNodeList namenode = effect.childNodes();
    for (int i = 0; i < namenode.count() ; ++i) {
        QDomElement pa = namenode.item(i).toElement();
        if (pa.tagName() != "parameter") continue;
        if (values.contains(pa.attribute("name"))) {
            pa.setAttribute("value", (int) (values.value(pa.attribute("name")) * m_locale.toDouble(pa.attribute("factor", "1"))));
        }
    }    
}


#include "lumaliftgain.moc"
