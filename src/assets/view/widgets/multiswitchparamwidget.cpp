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

    // setup the comment
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
    const QMap<KeyframeType, QString> keyframeTypes = KeyframeModel::getKeyframeTypes();
    m_labelComment->setText(comment);
    m_widgetComment->setHidden(true);
    // Linear
    methodCombo->addItem(keyframeTypes.value(KeyframeType::Linear), QVariant(QChar()));
    // Cubic In
    methodCombo->addItem(keyframeTypes.value(KeyframeType::CubicIn), QVariant(QLatin1Char('g')));
    // Exponential In
    methodCombo->addItem(keyframeTypes.value(KeyframeType::ExponentialIn), QVariant(QLatin1Char('p')));
    // Cubic Out
    methodCombo->addItem(keyframeTypes.value(KeyframeType::CubicOut), QVariant(QLatin1Char('h')));
    // Exponential Out
    methodCombo->addItem(keyframeTypes.value(KeyframeType::ExponentialOut), QVariant(QLatin1Char('q')));

    const QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    QChar mod = value.section(QLatin1Char('='), 0, -2).back();
    if (mod.isDigit()) {
        mod = QChar();
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_checkBox, &QCheckBox::checkStateChanged, this, &MultiSwitchParamWidget::paramChanged);
#else
    connect(m_checkBox, &QCheckBox::stateChanged, this, &MultiSwitchParamWidget::paramChanged);
#endif
}

void MultiSwitchParamWidget::paramChanged(int state)
{
    const QString sep = methodCombo->currentData().toChar().isNull() ? QStringLiteral("=") : methodCombo->currentData().toChar() + QLatin1Char('=');
    QString value;
    if (state == Qt::Checked) {
        value = m_model->data(m_index, AssetParameterModel::MaxRole).toString();
    } else {
        value = m_model->data(m_index, AssetParameterModel::MinRole).toString();
    }
    QStringList vals = value.split(QLatin1Char('='));
    if (vals.size() > 3) {
        for (auto &v : vals) {
            if (!v.back().isDigit()) {
                // Remove existing separator
                v.chop(1);
            }
        }
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
    QChar mod = value.section(QLatin1Char('='), 0, -2).back();
    if (mod.isDigit()) {
        mod = QChar();
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
            max.replace(QLatin1String("-1="), QString("%1=").arg(m_model->framesToTime(out)));
        } else {
            max.replace(QLatin1String("-1="), QString("%1=").arg(out));
        }
    }
    qDebug() << "=== GOT FILTER IN ROLE: " << m_model->data(m_index, AssetParameterModel::InRole).toInt()
             << " / OUT: " << m_model->data(m_index, AssetParameterModel::OutRole).toInt();
    qDebug() << "==== COMPARING MULTISWITCH: " << value << " = " << max;
    m_checkBox->setChecked(value == max);
}
