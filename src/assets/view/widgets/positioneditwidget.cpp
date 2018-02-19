/***************************************************************************
                          positionedit.cpp  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "positioneditwidget.hpp"
#include "kdenlivesettings.h"
#include "timecodedisplay.h"
#include "core.h"
#include "monitor/monitormanager.h"
#include "assets/model/assetparametermodel.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

PositionEditWidget::PositionEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    auto *layout = new QHBoxLayout(this);
    QString name = m_model->data(m_index, Qt::DisplayRole).toString();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    //TODO: take absolute from effect data
    m_absolute = false;
    QLabel *label = new QLabel(name, this);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));

    m_display = new TimecodeDisplay(pCore->monitorManager()->timecode(), this);
    m_display->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred));

    layout->addWidget(label);
    layout->addWidget(m_slider);
    layout->addWidget(m_display);
    slotRefresh();

    connect(m_slider, &QAbstractSlider::valueChanged, m_display, static_cast<void (TimecodeDisplay::*)(int)>(&TimecodeDisplay::setValue));
    connect(m_slider, &QAbstractSlider::valueChanged, this, &PositionEditWidget::valueChanged);

    // emit the signal of the base class when appropriate
    connect(this->m_slider, &QAbstractSlider::valueChanged, [this](int val) { emit AbstractParamWidget::valueChanged(m_index, QString::number(val), true); });

    setToolTip(comment);
}

PositionEditWidget::~PositionEditWidget()
{
}

void PositionEditWidget::updateTimecodeFormat()
{
    m_display->slotUpdateTimeCodeFormat();
}

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
    emit valueChanged();
}

void PositionEditWidget::setAbsolute(bool absolute)
{
    m_absolute = absolute;
}

void PositionEditWidget::slotSetRange(QPair <int, int> range)
{
    if (m_absolute) {
        m_slider->setRange(range.first, range.second);
        m_display->setRange(range.first, range.second);
    } else {
        m_slider->setRange(0, range.second - range.first);
        m_display->setRange(0, range.second - range.first);
    }
}

void PositionEditWidget::slotRefresh()
{
    int min = m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    int max = min + m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt();
    int val = m_model->data(m_index, AssetParameterModel::ValueRole).toInt();
    m_slider->blockSignals(true);
    slotSetRange(QPair<int, int>(min, max));
    m_slider->setValue(val);
    m_display->setValue(val);
    m_slider->blockSignals(false);
}


bool PositionEditWidget::isValid() const
{
    return m_slider->minimum() != m_slider->maximum();
}

void PositionEditWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}
