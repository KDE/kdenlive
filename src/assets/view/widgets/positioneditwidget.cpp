/*
    SPDX-FileCopyrightText: 2008 Marco Gittler <g.marco@freenet.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "positioneditwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitormanager.h"
#include "widgets/timecodedisplay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

PositionEditWidget::PositionEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    auto *layout = new QHBoxLayout(this);
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));

    m_display = new TimecodeDisplay(this);
    m_display->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred));

    layout->addWidget(m_slider, 0, Qt::AlignVCenter);
    layout->addWidget(m_display);
    layout->setContentsMargins(6, 0, 6, 0);
    m_inverted = m_model->data(m_index, AssetParameterModel::DefaultRole).toInt() < 0;
    m_toTime = m_model->data(m_index, AssetParameterModel::ToTimePosRole).toBool();
    slotRefresh();
    setMinimumHeight(m_display->sizeHint().height());

    connect(m_slider, &QAbstractSlider::valueChanged, m_display, static_cast<void (TimecodeDisplay::*)(int)>(&TimecodeDisplay::setValue));
    connect(m_display, &TimecodeDisplay::timeCodeEditingFinished, m_slider, &QAbstractSlider::setValue);
    connect(m_slider, &QAbstractSlider::valueChanged, this, &PositionEditWidget::valueChanged);

    // Q_EMIT the signal of the base class when appropriate
    connect(this->m_slider, &QAbstractSlider::valueChanged, this, [this](int val) {
        if (m_inverted) {
            val = m_model->data(m_index, AssetParameterModel::ParentInRole).toInt() + m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt() -
                  1 - val;
        } else if (!m_model->data(m_index, AssetParameterModel::RelativePosRole).toBool()) {
            val += m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
        }

        QString value;
        if (m_toTime) {
            value = m_model->framesToTime(val);
        } else {
            value = QString::number(val);
        }

        Q_EMIT AbstractParamWidget::valueChanged(m_index, value, true);
    });

    setToolTip(comment);
}

PositionEditWidget::~PositionEditWidget() = default;

int PositionEditWidget::getPosition() const
{
    return m_slider->value();
}

void PositionEditWidget::setPosition(int pos)
{
    m_slider->setValue(pos);
}

void PositionEditWidget::slotUpdatePosition()
{
    m_slider->blockSignals(true);
    m_slider->setValue(m_display->getValue());
    m_slider->blockSignals(false);
    Q_EMIT valueChanged();
}

void PositionEditWidget::slotRefresh()
{
    const QSignalBlocker bk(m_slider);
    int min = m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    int max = min + m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt();
    const QSignalBlocker blocker(m_slider);
    const QSignalBlocker blocker2(m_display);
    QVariant value = m_model->data(m_index, AssetParameterModel::ValueRole);
    int val;
    if (value.isNull()) {
        val = m_model->data(m_index, AssetParameterModel::DefaultRole).toInt();
        if (m_inverted) {
            val = -val - 1;
        }
    } else {
        if (value.userType() == QMetaType::QString) {
            val = m_model->time_to_frames(value.toString());
        } else {
            val = value.toInt();
        }
        if (m_inverted) {
            if (val < 0) {
                val = -val;
            } else {
                val = max - val - 1;
            }
        }
    }
    m_slider->setRange(0, max - min - 1);
    m_display->setRange(0, max - min - 1);
    if (!m_inverted && !m_model->data(m_index, AssetParameterModel::RelativePosRole).toBool()) {
        val -= min;
    }
    m_slider->setValue(val);
    m_display->setValue(val);
}

bool PositionEditWidget::isValid() const
{
    return m_slider->minimum() != m_slider->maximum();
}

void PositionEditWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}
