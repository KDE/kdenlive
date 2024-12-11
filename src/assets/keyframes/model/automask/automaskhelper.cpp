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

#include <QDateTime>
#include <QProcess>
#include <QSize>
#include <QtConcurrent>
#include <utility>

AutomaskHelper::AutomaskHelper(QObject *parent)
    : QObject(parent)
{
    // connect(this, &AutomaskHelper::previewImageReady, this, &AutomaskHelper::generateMask, Qt::QueuedConnection);
}

void AutomaskHelper::addMonitorControlPoint(int position, QSize frameSize, int xPos, int yPos, bool extend, bool exclude)
{
    const QPoint p(xPos, yPos);
    m_lastPos = position;
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
        if (!extend) {
            m_includePoints.clear();
            m_excludePoints.clear();
        } else {
            if (m_includePoints.contains(m_lastPos)) {
                pointsList = m_includePoints.value(m_lastPos);
                if (pointsList.contains(p)) {
                    // Already in
                    return;
                }
            }
        }
        pointsList.append(p);
        m_includePoints.insert(m_lastPos, pointsList);
    }
    QVariantList points;
    QVariantList pointsTypes;
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
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(QRect(), points, pointsTypes);
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
    QUrl url = QUrl::fromLocalFile(QStringLiteral("/tmp/preview.png"));
    url.setQuery(QStringLiteral("pos=%1&ctrl=%2").arg(m_lastPos).arg(QDateTime::currentSecsSinceEpoch()));
    qDebug() << "---- IMAGE GENERATION DONE; RESULTING URL: " << url;
    pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay = url;
    Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();
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
    m_mode = AutomaskHelper::PREVIEWMODE;
    // Now convert frames to video
    // ffmpeg -framerate 25 -pattern_type glob -i '*.png' -c:v ffv1 -pix_fmt yuva420p output.mkv
    args = {QStringLiteral("-framerate"),
            QString::number(pCore->getCurrentFps()),
            QStringLiteral("-pattern_type"),
            QStringLiteral("glob"),
            QStringLiteral("-i"),
            QStringLiteral("/tmp/out_dir/*.png"),
            QStringLiteral("-c:v"),
            QStringLiteral("ffv1"),
            QStringLiteral("-pix_fmt"),
            QStringLiteral("yuva420p"),
            QStringLiteral("/tmp/output.mkv")};
    scriptJob.start(KdenliveSettings::ffmpegpath(), args);
    scriptJob.waitForFinished(-1);
}

void AutomaskHelper::monitorSeek(int pos)
{
    if (m_mode == AutomaskHelper::PREVIEWMODE) {
        pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay =
            QUrl::fromLocalFile(QStringLiteral("/tmp/out_dir/color_img-%1.png").arg(pos, 5, 10, QLatin1Char('0')));
        Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();
    }
}
