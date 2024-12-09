/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "automaskhelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"

#include <QProcess>
#include <QSize>
#include <QtConcurrent>
#include <utility>

AutomaskHelper::AutomaskHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QPersistentModelIndex index, QObject *parent)
    : KeyframeMonitorHelper(monitor, std::move(model), std::move(index), parent)
{
    connect(m_monitor, &Monitor::addMonitorControlPoint, this, &AutomaskHelper::addMonitorControlPoint, Qt::UniqueConnection);
}

void AutomaskHelper::addMonitorControlPoint(int xPos, int yPos, bool exclude)
{
    const QPoint p(xPos, yPos);
    if (exclude) {
        if (!m_excludePoints.contains(p)) {
            m_excludePoints.append(p);
        }
    } else {
        if (!m_includePoints.contains(p)) {
            m_includePoints.append(p);
        }
    }
    QtConcurrent::run(&AutomaskHelper::generateImage, this, m_includePoints, m_excludePoints);
}

void AutomaskHelper::generateImage(const QList<QPoint> includes, const QList<QPoint> excludes)
{
    QProcess scriptJob;
    QStringList args = {QStringLiteral("/home/seven/git/sam2/venv/test-sam.py")};
    scriptJob.start(KdenliveSettings::pythonPath(), args);
    scriptJob.waitForFinished(-1);
    Q_EMIT previewImageReady();
}

void AutomaskHelper::slotUpdateFromMonitorData(const QVariantList &v)
{
    const QVariantList points = QVariant(v).toList();
    QSize frameSize = pCore->getCurrentFrameSize();
    int ix = 0;
    for (const auto &point : points) {
        QPointF pt = point.toPointF();
        double x = (pt.x() / frameSize.width() + 1) / 3;
        double y = (pt.y() / frameSize.height() + 1) / 3;
        Q_EMIT updateKeyframeData(m_indexes.at(ix), x);
        Q_EMIT updateKeyframeData(m_indexes.at(ix + 1), y);
        ix += 2;
    }
}

void AutomaskHelper::refreshParams(int pos)
{
    QVariantList points{QPointF(), QPointF(), QPointF(), QPointF()};
    QSize frameSize = pCore->getCurrentFrameSize();
    for (const auto &ix : std::as_const(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type != ParamType::KeyframeParam) {
            continue;
        }
        int paramName = m_model->data(ix, AssetParameterModel::NameRole).toInt();
        if (paramName > 7) {
            continue;
        }
        double value = m_model->getKeyframeModel()->getInterpolatedValue(pos, ix).toDouble();
        value = ((3 * value) - 1) * (paramName % 2 == 0 ? frameSize.width() : frameSize.height());
        switch (paramName) {
        case 0:
            points[0] = QPointF(value, points.at(0).toPointF().y());
            break;
        case 1:
            points[0] = QPointF(points.at(0).toPointF().x(), value);
            break;
        case 2:
            points[1] = QPointF(value, points.at(1).toPointF().y());
            break;
        case 3:
            points[1] = QPointF(points.at(1).toPointF().x(), value);
            break;
        case 4:
            points[2] = QPointF(value, points.at(2).toPointF().y());
            break;
        case 5:
            points[2] = QPointF(points.at(2).toPointF().x(), value);
            break;
        case 6:
            points[3] = QPointF(value, points.at(3).toPointF().y());
            break;
        case 7:
            points[3] = QPointF(points.at(3).toPointF().x(), value);
            break;
        default:
            break;
        }
    }
    if (m_monitor) {
        m_monitor->setUpEffectGeometry(QRect(), points);
    }
}
