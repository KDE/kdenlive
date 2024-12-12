/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "automaskhelper.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "jobs/masktask.h"
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
    bool ok;
    QDir maskSrcFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    QStringList args = {QStringLiteral("/home/six/git/sam2/venv/sam-objectmask.py"),
                        QStringLiteral("-I"),
                        maskSrcFolder.absolutePath(),
                        QStringLiteral("-P"),
                        QStringLiteral("%1=%2").arg(m_lastPos).arg(pointsList.join(QLatin1Char(','))),
                        QStringLiteral("-L"),
                        QStringLiteral("%1=%2").arg(m_lastPos).arg(labelsList.join(QLatin1Char(','))),
                        QStringLiteral("-F"),
                        QString::number(m_lastPos)};
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

void AutomaskHelper::generatePreview(const QString &binId, int in, int out, const QString &maskName, const QString &fileName)
{
    // Generate params
    QMap<int, QString> maskParams;
    bool ok;
    QDir maskSrcFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    maskParams.insert(MaskTask::INPUTFOLDER, maskSrcFolder.absolutePath());
    QString outFolder = QStringLiteral("output-frames");
    if (maskSrcFolder.exists(outFolder)) {
        QDir toRemove(maskSrcFolder.absoluteFilePath(outFolder));
        if (toRemove.dirName() == outFolder) {
            toRemove.removeRecursively();
        }
    }
    maskSrcFolder.mkpath(outFolder);
    if (!maskSrcFolder.cd(outFolder)) {
        return;
    }
    maskParams.insert(MaskTask::OUTPUTFOLDER, maskSrcFolder.absolutePath());
    maskParams.insert(MaskTask::OUTPUTFILE, fileName);
    maskParams.insert(MaskTask::NAME, maskName);
    QList<int> frames = m_includePoints.keys();
    QList<int> keys = m_excludePoints.keys();
    for (auto &k : keys) {
        if (!frames.contains(k)) {
            frames << k;
        }
    }
    keys = m_boxes.keys();
    for (auto &k : keys) {
        if (!frames.contains(k)) {
            frames << k;
        }
    }
    // Generate points strings
    QStringList fullPointsList;
    QStringList fullLabelsList;
    QStringList fullBoxList;
    for (auto &f : frames) {
        QStringList pointsList;
        QStringList labelsList;
        QStringList boxList;
        if (m_includePoints.contains(f)) {
            const QList<QPoint> points = m_includePoints.value(f);
            for (auto &p : points) {
                pointsList << QString::number(p.x());
                pointsList << QString::number(p.y());
                labelsList << QStringLiteral("1");
            }
        }
        if (m_excludePoints.contains(f)) {
            const QList<QPoint> points = m_excludePoints.value(f);
            for (auto &p : points) {
                pointsList << QString::number(p.x());
                pointsList << QString::number(p.y());
                labelsList << QStringLiteral("0");
            }
        }
        if (m_boxes.contains(f)) {
            const QList<QRect> rects = m_boxes.value(f);
            for (auto &r : rects) {
                boxList << QString::number(r.x());
                boxList << QString::number(r.y());
                boxList << QString::number(r.width());
                boxList << QString::number(r.height());
            }
        }
        if (!pointsList.isEmpty()) {
            QString pointsValue = QStringLiteral("%1=").arg(f);
            QString labelsValue = pointsValue;
            pointsValue.append(pointsList.join(QLatin1Char(',')));
            labelsValue.append(labelsList.join(QLatin1Char(',')));
            fullPointsList << pointsValue;
            fullLabelsList << labelsValue;
        }
        if (!boxList.isEmpty()) {
            QString boxValue = QStringLiteral("%1=").arg(f);
            ;
            boxValue.append(boxList.join(QLatin1Char(',')));
            fullBoxList << boxValue;
        }
    }
    if (!fullPointsList.isEmpty()) {
        maskParams.insert(MaskTask::POINTS, fullPointsList.join(QLatin1Char(';')));
        maskParams.insert(MaskTask::LABELS, fullLabelsList.join(QLatin1Char(';')));
    }
    if (!fullBoxList.isEmpty()) {
        maskParams.insert(MaskTask::BOX, fullBoxList.join(QLatin1Char(';')));
    }
    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
    if (clip) {
        MaskTask::start(ObjectId(KdenliveObjectType::BinClip, binId.toInt(), QUuid()), maskParams, in, out, clip.get());
    }
}

/*void AutomaskHelper::doGeneratePreview()
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
    bool ok;
    QDir maskSrcFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    QDir maskDstFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);

    QStringList args = {QStringLiteral("/home/six/git/sam2/venv/sam-objectmask.py"),
                        QStringLiteral("-I"),
                        maskSrcFolder.absolutePath(),
                        QStringLiteral("-P"),
                        QStringLiteral("%1=%2").arg(m_lastPos).arg(pointsList.join(QLatin1Char(','))),
                        QStringLiteral("-L"),
                        QStringLiteral("%1=%2").arg(m_lastPos).arg(labelsList.join(QLatin1Char(','))),
                        QStringLiteral("-O"),
                        QStringLiteral("/tmp/out_dir")};
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
}*/

void AutomaskHelper::monitorSeek(int pos)
{
    if (m_mode == AutomaskHelper::PREVIEWMODE) {
        pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay =
            QUrl::fromLocalFile(QStringLiteral("/tmp/out_dir/color_img-%1.png").arg(pos, 5, 10, QLatin1Char('0')));
        Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();
    }
}
