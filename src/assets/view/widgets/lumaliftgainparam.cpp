/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    Some code was borrowed from shotcut
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "lumaliftgainparam.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "colorwheel.h"
#include "utils/flowlayout.h"

#include <KLocalizedString>

static const double LIFT_FACTOR = 1.0;
static const double GAMMA_FACTOR = 2.0;
static const double GAIN_FACTOR = 4.0;

LumaLiftGainParam::LumaLiftGainParam(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, QWidget *parent)
    : QWidget(parent)
    , m_model(std::move(model))
    , m_index(index)
{
    m_flowLayout = new FlowLayout(this, 2, 2, 2);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lift = new ColorWheel(QStringLiteral("lift"), i18n("Lift"), NegQColor::fromRgbF(0.5, 0.5, 0.5), this);
    m_lift->setFactorDefaultZero(LIFT_FACTOR, 0, 0);
    connect(m_lift, &ColorWheel::colorChange, this, &LumaLiftGainParam::liftChanged);
    m_gamma = new ColorWheel(QStringLiteral("gamma"), i18n("Gamma"), NegQColor::fromRgbF(1. / GAMMA_FACTOR, 1. / GAMMA_FACTOR, 1. / GAMMA_FACTOR), this);
    m_gamma->setFactorDefaultZero(GAMMA_FACTOR, 1, 0);
    connect(m_gamma, &ColorWheel::colorChange, this, &LumaLiftGainParam::gammaChanged);
    m_gain = new ColorWheel(QStringLiteral("gain"), i18n("Gain"), NegQColor::fromRgbF(1. / GAIN_FACTOR, 1. / GAIN_FACTOR, 1. / GAIN_FACTOR), this);
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
    m_flowLayout->setSpacing(0);
    setLayout(m_flowLayout);
    slotRefresh(0);

    connect(this, &LumaLiftGainParam::liftChanged, [this, indexes](const NegQColor &sourceColor, const NegQColor &color, bool createUndo) {
        QList<QModelIndex> ixes{indexes.value(QStringLiteral("lift_r")), indexes.value(QStringLiteral("lift_g")), indexes.value(QStringLiteral("lift_b"))};
        QStringList sourceValues{QString::number(sourceColor.redF() * 2 - 1, 'f'), QString::number(sourceColor.greenF() * 2 - 1, 'f'),
                                 QString::number(sourceColor.blueF() * 2 - 1, 'f')};
        QStringList values{QString::number(color.redF() * 2 - 1, 'f'), QString::number(color.greenF() * 2 - 1, 'f'),
                           QString::number(color.blueF() * 2 - 1, 'f')};
        Q_EMIT valuesChanged(ixes, sourceValues, values, createUndo);
    });
    connect(this, &LumaLiftGainParam::gammaChanged, [this, indexes](const NegQColor &sourceColor, const NegQColor &color, bool createUndo) {
        QList<QModelIndex> ixes{indexes.value(QStringLiteral("gamma_r")), indexes.value(QStringLiteral("gamma_g")), indexes.value(QStringLiteral("gamma_b"))};
        QStringList sourceValues{QString::number(sourceColor.redF() * GAMMA_FACTOR, 'f'), QString::number(sourceColor.greenF() * GAMMA_FACTOR, 'f'),
                                 QString::number(sourceColor.blueF() * GAMMA_FACTOR, 'f')};
        QStringList values{QString::number(color.redF() * GAMMA_FACTOR, 'f'), QString::number(color.greenF() * GAMMA_FACTOR, 'f'),
                           QString::number(color.blueF() * GAMMA_FACTOR, 'f')};
        Q_EMIT valuesChanged(ixes, sourceValues, values, createUndo);
    });
    connect(this, &LumaLiftGainParam::gainChanged, [this, indexes](const NegQColor &sourceColor, const NegQColor &color, bool createUndo) {
        QList<QModelIndex> ixes{indexes.value(QStringLiteral("gain_r")), indexes.value(QStringLiteral("gain_g")), indexes.value(QStringLiteral("gain_b"))};
        QStringList sourceValues{QString::number(sourceColor.redF() * GAIN_FACTOR, 'f'), QString::number(sourceColor.greenF() * GAIN_FACTOR, 'f'),
                                 QString::number(sourceColor.blueF() * GAIN_FACTOR, 'f')};
        QStringList values{QString::number(color.redF() * GAIN_FACTOR, 'f'), QString::number(color.greenF() * GAIN_FACTOR, 'f'),
                           QString::number(color.blueF() * GAIN_FACTOR, 'f')};
        Q_EMIT valuesChanged(ixes, sourceValues, values, createUndo);
    });
}

void LumaLiftGainParam::updateEffect(QDomElement &effect)
{
    NegQColor lift = m_lift->color();
    NegQColor gamma = m_gamma->color();
    NegQColor gain = m_gain->color();
    QMap<QString, double> values;
    values.insert(QStringLiteral("lift_r"), lift.redF() * 2 - 1);
    values.insert(QStringLiteral("lift_g"), lift.greenF() * 2 - 1);
    values.insert(QStringLiteral("lift_b"), lift.blueF() * 2 - 1);

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
            pa.setAttribute(QStringLiteral("value"),
                            int(values.value(pa.attribute(QStringLiteral("name"))) * pa.attribute(QStringLiteral("factor"), QStringLiteral("1")).toDouble()));
        }
    }
}

void LumaLiftGainParam::resizeEvent(QResizeEvent *ev)
{
    QWidget::resizeEvent(ev);
    if (height() != m_flowLayout->miniHeight()) {
        setFixedHeight(m_flowLayout->miniHeight());
        Q_EMIT updateHeight(m_flowLayout->miniHeight());
    }
}

int LumaLiftGainParam::miniHeight()
{
    return m_flowLayout->miniHeight();
}

void LumaLiftGainParam::slotRefresh(int pos)
{
    QMap<QString, double> values;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const QModelIndex local_index = m_model->index(i, 0);
        QString name = m_model->data(local_index, AssetParameterModel::NameRole).toString();
        double val = m_model->getKeyframeModel()->getInterpolatedValue(pos, local_index).toDouble();
        values.insert(name, val);
    }
    m_lift->setColor({(values.value(QStringLiteral("lift_r")) + 1) / 2., (values.value(QStringLiteral("lift_g")) + 1) / 2.,
                      (values.value(QStringLiteral("lift_b")) + 1) / 2.});
    m_gamma->setColor({values.value(QStringLiteral("gamma_r")), values.value(QStringLiteral("gamma_g")), values.value(QStringLiteral("gamma_b"))});
    m_gain->setColor({values.value(QStringLiteral("gain_r")), values.value(QStringLiteral("gain_g")), values.value(QStringLiteral("gain_b"))});
}
