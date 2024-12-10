/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "automaskhelper.hpp"
#include "bin/projectclip.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"

#include <QProcess>
#include <QSize>
#include <QtConcurrent>
#include <utility>

AutomaskHelper::AutomaskHelper(Monitor *monitor, std::shared_ptr<ProjectClip> clip, QObject *parent)
    : QObject(parent)
    , m_monitor(monitor)
    , m_clip(clip)
{
    connect(m_monitor, &Monitor::addMonitorControlPoint, this, &AutomaskHelper::addMonitorControlPoint, Qt::UniqueConnection);
    // connect(this, &AutomaskHelper::previewImageReady, this, &AutomaskHelper::generateMask, Qt::QueuedConnection);
}

void AutomaskHelper::addMonitorControlPoint(int xPos, int yPos, bool exclude)
{
    const QPoint p(xPos, yPos);
    m_lastPos = m_monitor->position();
    qDebug() << "================\n\nADDING MONITOR POINT: " << m_lastPos << " = " << p << "\n\n==================";
    QList<QPoint> pointsList;
    if (exclude) {
        if (m_excludePoints.contains(m_lastPos)) {
            pointsList = m_excludePoints.value(m_lastPos);
            if (pointsList.contains(p)) {
                // Already in
                return;
            }
        }
        pointsList.append(p);
        m_excludePoints.insert(m_lastPos, pointsList);
    } else {
        if (m_includePoints.contains(m_lastPos)) {
            pointsList = m_includePoints.value(m_lastPos);
            if (pointsList.contains(p)) {
                // Already in
                return;
            }
        }
        pointsList.append(p);
        m_includePoints.insert(m_lastPos, pointsList);
    }
    QVariantList points;
    QVariantList pointsTypes;
    const QSize frameSize = m_clip->frameSize();
    pointsList = m_includePoints.value(m_lastPos);
    for (auto &p : pointsList) {
        points << QPointF(double(p.x()) / frameSize.width(), double(p.y()) / frameSize.height());
        pointsTypes << 1;
    }
    pointsList = m_excludePoints.value(m_lastPos);
    for (auto &p : pointsList) {
        points << QPointF(double(p.x()) / frameSize.width(), double(p.y()) / frameSize.height());
        pointsTypes << 0;
    }
    qDebug() << "===== ADDED CONTROL POINT; TOTAL: " << "EXCLUDING: " << exclude << ", " << points.size() << ", CENTERS: " << pointsTypes;
    m_monitor->setUpEffectGeometry(QRect(), points, pointsTypes);
    QtConcurrent::run(&AutomaskHelper::generateImage, this);
}

void AutomaskHelper::generateImage()
{
    QProcess scriptJob;
    QStringList pointsList;
    QStringList labelsList;
    if (m_includePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_includePoints.value(m_lastPos);
        for (auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("1");
        }
    }
    if (m_excludePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_excludePoints.value(m_lastPos);
        for (auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("0");
        }
    }
    QStringList args = {QStringLiteral("/home/six/git/sam2/venv/sam-objectmask.py"),
                        QStringLiteral("-P"),
                        pointsList.join(QLatin1Char(',')),
                        QStringLiteral("-L"),
                        labelsList.join(QLatin1Char(',')),
                        QStringLiteral("preview")};
    qDebug() << "---- STARTING IMAGE GENERATION: " << args;
    const QString exec("/home/six/git/sam2/venv/bin/python3");
    scriptJob.start(exec, args);
    scriptJob.waitForFinished(-1);
    Q_EMIT m_monitor->getControllerProxy()->previewOverlayChanged();
}

void AutomaskHelper::generatePreview()
{
    QtConcurrent::run(&AutomaskHelper::doGeneratePreview, this);
}

void AutomaskHelper::doGeneratePreview()
{
    QProcess scriptJob;
    QStringList pointsList;
    QStringList labelsList;
    if (m_includePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_includePoints.value(m_lastPos);
        for (auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("1");
        }
    }
    if (m_excludePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_excludePoints.value(m_lastPos);
        for (auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("0");
        }
    }
    QStringList args = {QStringLiteral("/home/six/git/sam2/venv/sam-objectmask.py"),
                        QStringLiteral("-P"),
                        pointsList.join(QLatin1Char(',')),
                        QStringLiteral("-L"),
                        labelsList.join(QLatin1Char(',')),
                        QStringLiteral("-O"),
                        QStringLiteral("/tmp/out_dir"),
                        QStringLiteral("render")};
    qDebug() << "---- STARTING IMAGE GENERATION: " << args;
    const QString exec("/home/six/git/sam2/venv/bin/python3");
    qDebug() << "//// STARTING PREVIEW GENERATION WITH: " << args;
    scriptJob.start(exec, args);
    scriptJob.waitForFinished(-1);
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
        // Q_EMIT updateKeyframeData(m_indexes.at(ix), x);
        // Q_EMIT updateKeyframeData(m_indexes.at(ix + 1), y);
        ix += 2;
    }
}

void AutomaskHelper::refreshParams(int pos)
{
    QVariantList points{QPointF(), QPointF(), QPointF(), QPointF()};
    QSize frameSize = pCore->getCurrentFrameSize();
    /*for (const auto &ix : std::as_const(m_indexes)) {
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
    }*/
    if (m_monitor) {
        m_monitor->setUpEffectGeometry(QRect(), points);
    }
}
