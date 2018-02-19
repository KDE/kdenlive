/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "geometryeditwidget.hpp"
#include "kdenlivesettings.h"
#include "timecodedisplay.h"
#include "core.h"
#include "monitor/monitor.h"
#include "widgets/geometrywidget.h"
#include "monitor/monitormanager.h"
#include "assets/model/assetparametermodel.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

GeometryEditWidget::GeometryEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QPair<int, int> range, QSize frameSize, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_monitor(pCore->getMonitor(m_model->monitorId))
    , m_range(range)
{
    auto *layout = new QVBoxLayout(this);
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    const QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString().simplified();
    QRect rect;
    QStringList vals = value.split(QLatin1Char(' '));
    if (vals.count() >= 4) {
        rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
    }
    m_geom = new GeometryWidget(m_monitor, range, rect, frameSize, false, this);
    m_geom->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));
    connect(m_monitor, &Monitor::seekPosition, this, &GeometryEditWidget::monitorSeek, Qt::UniqueConnection);
    /*QString name = m_model->data(m_index, AssetParameterModel::NameRole).toString();
    QLabel *label = new QLabel(name, this);
    layout->addWidget(label);*/
    layout->addWidget(m_geom);

    // emit the signal of the base class when appropriate
    connect(this->m_geom, &GeometryWidget::valueChanged, [this](const QString val) { 
        emit AbstractParamWidget::valueChanged(m_index, val, true); });

    setToolTip(comment);
}

GeometryEditWidget::~GeometryEditWidget()
{
}

void GeometryEditWidget::slotSetRange(QPair <int, int> /*range*/)
{
    bool ok;
    int startPos = m_model->data(m_index, AssetParameterModel::ParentInRole).toInt(&ok);
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    m_range = QPair <int, int>(startPos, startPos + duration);
    m_geom->slotSetRange(m_range);
}

void GeometryEditWidget::slotRefresh()
{
    const QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString().simplified();
    QRect rect;
    QStringList vals = value.split(QLatin1Char(' '));
    if (vals.count() >= 4) {
        rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
        m_geom->setValue(rect);
    }
}

void GeometryEditWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}

void GeometryEditWidget::monitorSeek(int pos)
{
    // Update monitor scene for geometry params
    if (pos >= m_range.first && pos < m_range.second) {
        m_geom->connectMonitor(true);
        m_monitor->setEffectKeyframe(true);
    } else {
        m_geom->connectMonitor(false);
        m_monitor->setEffectKeyframe(false);
    }
}

