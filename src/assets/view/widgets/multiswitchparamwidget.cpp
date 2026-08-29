/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "multiswitchparamwidget.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/model/assetparametermodel.hpp"

MultiSwitchParamWidget::MultiSwitchParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    m_widgetComment->setHidden(true);
    const QStringList keyframeShortcuts = m_model->data(m_index, AssetParameterModel::KeyframeTypesRole).toStringList();
    QString desc;
    for (auto k : keyframeShortcuts) {
        if (k.isEmpty()) {
            desc = KeyframeModel::getKeyframeDescriptionFromShortcut(QChar());
        } else {
            desc = KeyframeModel::getKeyframeDescriptionFromShortcut(k.at(0));
        }
        if (!desc.isEmpty()) {
            qDebug() << ":: ADDING MULTI ITEM: " << desc << " = " << k;
            methodCombo->addItem(desc, QVariant(k.at(0)));
        }
    }

    const QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    QChar mod;
    if (value.contains(QLatin1Char('='))) {
        const QString cut = value.section(QLatin1Char('='), 0, 0);
        if (!cut.isEmpty()) {
            mod = cut.back();
            if (mod.isDigit()) {
                mod = QChar();
            }
        }
    }
    if (!mod.isNull()) {
        int ix = methodCombo->findData(QVariant(mod));
        if (ix > -1) {
            methodCombo->setCurrentIndex(ix);
        }
    }
    // set check state
    slotRefresh();
    connect(methodCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() { paramChanged(m_checkBox->checkState()); });

    // Q_EMIT the signal of the base class when appropriate
    connect(m_checkBox, &QCheckBox::checkStateChanged, this, &MultiSwitchParamWidget::paramChanged);
}

void MultiSwitchParamWidget::paramChanged(int state)
{
    QString sep = methodCombo->currentData().toChar();
    qDebug() << "------- GOT SEPARATOR DATA: " << sep << " == " << methodCombo->currentData();
    sep.append(QLatin1Char('='));
    QString value;
    qDebug() << "------- GOT UPDATED SEPARATOR: " << sep;
    if (state == Qt::Checked) {
        value = m_model->data(m_index, AssetParameterModel::MaxRole).toString();
    } else {
        value = m_model->data(m_index, AssetParameterModel::MinRole).toString();
    }
    QStringList vals = value.split(QLatin1Char('='));
    // Remove possible keyframe qualifyer
    for (auto &v : vals) {
        if (!v.isEmpty() && !v.back().isDigit()) {
            v.chop(1);
        }
    }
    qDebug() << "::: READY TO SPLIT VALUES: " << vals;
    if (vals.size() > 3) {
        if (vals.at(0) == QLatin1String("0")) {
            vals[0] = QLatin1String("00:00:00.000");
        }
        if (vals.at(2).endsWith(QLatin1String(";-1"))) {
            // Replace -1 with out position
            int out = m_model->data(m_index, AssetParameterModel::OutRole).toInt() - m_model->data(m_index, AssetParameterModel::InRole).toInt();
            vals[2].chop(3);
            vals[2].append(QStringLiteral(";") + m_model->framesToTime(out));
        }
    }
    value = vals.join(sep);
    Q_EMIT valueChanged(m_index, value, true);
}

void MultiSwitchParamWidget::slotShowComment(bool show)
{
    if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }
}

void MultiSwitchParamWidget::slotRefresh()
{
    const QSignalBlocker bk(m_checkBox);
    QString max = m_model->data(m_index, AssetParameterModel::MaxRole).toString();
    QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    if (!max.contains(QLatin1Char('\n'))) {
        m_checkBox->setVisible(false);
    }
    qDebug() << "::::: COMPARING MULTISWITCH VAL: " << max << " == " << value;
    QChar mod;
    if (value.contains(QLatin1Char('='))) {
        const QString cut = value.section(QLatin1Char('='), 0, 0);
        if (!cut.isEmpty()) {
            mod = cut.back();
            if (mod.isDigit()) {
                mod = QChar();
            }
        }
    }
    QSignalBlocker bk2(methodCombo);
    if (!mod.isNull()) {
        QString toReplace = mod + QLatin1Char('=');
        value.replace(toReplace, QStringLiteral("="));
        int ix = methodCombo->findData(QVariant(mod));
        if (ix > -1) {
            methodCombo->setCurrentIndex(ix);
        }
    } else {
        methodCombo->setCurrentIndex(0);
    }
    bool convertToTime = false;
    if (value.contains(QLatin1Char(':'))) {
        convertToTime = true;
    }
    if (max.contains(QLatin1String("0=")) && convertToTime) {
        max.replace(QLatin1String("0="), QLatin1String("00:00:00.000="));
    }
    if (max.contains(QLatin1String("-1=")) && !value.contains(QLatin1String("-1="))) {
        // Replace -1 with out position
        int out = m_model->data(m_index, AssetParameterModel::OutRole).toInt() - m_model->data(m_index, AssetParameterModel::InRole).toInt();
        if (convertToTime) {
            max.replace(QLatin1String("-1="), QStringLiteral("%1=").arg(m_model->framesToTime(out)));
        } else {
            max.replace(QLatin1String("-1="), QStringLiteral("%1=").arg(out));
        }
    }
    qDebug() << "=== GOT FILTER IN ROLE: " << m_model->data(m_index, AssetParameterModel::InRole).toInt()
             << " / OUT: " << m_model->data(m_index, AssetParameterModel::OutRole).toInt();
    qDebug() << "==== COMPARING MULTISWITCH: " << value << " = " << max;
    m_checkBox->setChecked(value == max);
}
