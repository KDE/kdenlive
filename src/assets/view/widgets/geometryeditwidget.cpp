/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "geometryeditwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "widgets/timecodedisplay.h"
#include "widgets/geometrywidget.h"
#include <QVBoxLayout>
#include <framework/mlt_types.h>
#include <mlt++/MltProperties.h>

GeometryEditWidget::GeometryEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    auto *layout = new QVBoxLayout(this);
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    const QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString().simplified();
    int start = m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    int end = start + m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt();
    QRect rect;
    if (value.contains(QLatin1Char('%'))) {
        QSize profileSize = pCore->getCurrentFrameSize();
        Mlt::Properties mlt_prop;
        m_model->passProperties(mlt_prop);
        mlt_prop.set("rect", value.toUtf8().data());
        mlt_rect r = mlt_prop.get_rect("rect");
        rect = QRect(int(profileSize.width() * r.x), int(profileSize.height() * r.y), int(profileSize.width()* r.w), int(profileSize.height() * r.h));;
    } else {
        QStringList vals = value.split(QLatin1Char(' '));
        if (vals.count() >= 4) {
            rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
        }
    }
    if (rect.isNull()) {
        // Cannot read value, use random default
        rect = QRect(50, 50, 200, 200);
    }
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    m_geom = new GeometryWidget(monitor, QPair<int, int>(start, end), rect, 100, frameSize, false,
                                m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), this);
    m_geom->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));
    connect(m_geom, &GeometryWidget::updateMonitorGeometry, this, [this](const QRect r) {
        if (m_model->isActive()) {
            pCore->getMonitor(m_model->monitorId)->setUpEffectGeometry(r);
        }
    });
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_geom);
    setFixedHeight(m_geom->sizeHint().height());

    // emit the signal of the base class when appropriate
    connect(this->m_geom, &GeometryWidget::valueChanged, this, [this](const QString &val) { emit valueChanged(m_index, val, true); });

    setToolTip(comment);
}

GeometryEditWidget::~GeometryEditWidget() = default;

void GeometryEditWidget::slotRefresh()
{
    const QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString().simplified();
    QRect rect;
    QStringList vals = value.split(QLatin1Char(' '));
    int start = m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    int end = start + m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt();
    m_geom->slotSetRange(QPair<int, int>(start, end));
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
    int start = m_model->data(m_index, AssetParameterModel::ParentPositionRole).toInt();
    int end = start + m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt();
    if (pos >= start && pos < end) {
        m_geom->connectMonitor(true);
        pCore->getMonitor(m_model->monitorId)->setEffectKeyframe(true);
    } else {
        m_geom->connectMonitor(false);
    }
}

void GeometryEditWidget::slotInitMonitor(bool active)
{
    m_geom->connectMonitor(active);
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (active) {
        monitor->setEffectKeyframe(true);
        connect(monitor, &Monitor::seekPosition, this, &GeometryEditWidget::monitorSeek, Qt::UniqueConnection);
    } else {
        disconnect(monitor, &Monitor::seekPosition, this, &GeometryEditWidget::monitorSeek);
    }
}
