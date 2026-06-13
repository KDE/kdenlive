/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "geometryeditwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "monitor/monitor.h"
#include "widgets/geometrywidget.h"
#include <QVBoxLayout>
#include <framework/mlt_types.h>
#include <mlt++/MltProperties.h>

GeometryEditWidget::GeometryEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent, QFormLayout *layout)
    : AbstractParamWidget(std::move(model), index, parent)
{
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
        rect = QRect(int(profileSize.width() * r.x), int(profileSize.height() * r.y), int(profileSize.width() * r.w), int(profileSize.height() * r.h));
        ;
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

    auto type = m_model->data(m_index, AssetParameterModel::TypeRole).value<ParamType>();
    std::pair<int, int> xRange{0, 0};
    std::pair<int, int> yRange{0, 0};
    if (type == ParamType::FakeRect || type == ParamType::AnimatedFakeRect) {
        QVariantMap mappedParams = m_model->data(index, AssetParameterModel::FakeRectRole).toMap();
        for (auto i = mappedParams.cbegin(), end = mappedParams.cend(); i != end; ++i) {
            const AssetRectInfo paramInfo = i.value().value<AssetRectInfo>();
            int index = paramInfo.positionForTarget();
            if (index % 2 == 0) {
                if (paramInfo.maximum > paramInfo.minimum) {
                    xRange = {int(paramInfo.minimum), int(paramInfo.maximum)};
                }
            } else {
                if (paramInfo.maximum > paramInfo.minimum) {
                    yRange = {int(paramInfo.minimum), int(paramInfo.maximum)};
                }
            }
        }
    }
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    m_geom.reset(new GeometryWidget(monitor, QPair<int, int>(start, end), rect, false, 100, frameSize, false,
                                    m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), this, layout, xRange, yRange));
    connect(m_geom.get(), &GeometryWidget::updateMonitorGeometry, this, [this](const QRect r) {
        if (m_model->isActive()) {
            pCore->getMonitor(m_model->monitorId)->setUpEffectGeometry(r);
        }
    });

    // Q_EMIT the signal of the base class when appropriate
    connect(m_geom.get(), &GeometryWidget::valueChanged, this, [this](const QString &val) { Q_EMIT valueChanged(m_index, val, true); });
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
        qDebug() << ":::: MONITOR SEEK INSIDE EFFECT!!!!!\n__________________";
        pCore->getMonitor(m_model->monitorId)->setEffectKeyframe(true, false);
    } else {
        if (m_geom->connectMonitor(false)) {
            pCore->getMonitor(m_model->monitorId)->setEffectKeyframe(false, true);
        }
    }
}

void GeometryEditWidget::slotInitMonitor(bool active, bool outside)
{
    m_geom->connectMonitor(active);
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (active) {
        // We have no keyframes, allow editing even if
        qDebug() << ":::: MONITOR SEEK OUTSIDE EFFECT:" << outside;
        monitor->setEffectKeyframe(true, outside);
        connect(monitor, &Monitor::seekPosition, this, &GeometryEditWidget::monitorSeek, Qt::UniqueConnection);
    } else {
        disconnect(monitor, &Monitor::seekPosition, this, &GeometryEditWidget::monitorSeek);
    }
    if (monitor->effectSceneDisplayed(SceneType::MonitorSceneGeometry)) {
        monitor->setUpEffectGeometry(m_geom->getRect());
    } else {
        // Scene is not ready yet
        connect(monitor, &Monitor::sceneChanged, this, [this](SceneType::MonitorSceneType sceneType) {
            if (sceneType == SceneType::MonitorSceneGeometry) {
                pCore->getMonitor(m_model->monitorId)->setUpEffectGeometry(m_geom->getRect());
            }
        });
    }
}
