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
#include "pythoninterfaces/saminterface.h"
#include "utils/qstringutils.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QDateTime>
#include <QProcess>
#include <QSize>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

AutomaskHelper::AutomaskHelper(QObject *parent)
    : QObject(parent)
{
}

void AutomaskHelper::addMonitorControlPoint(const QString &previewFile, int position, const QSize frameSize, int xPos, int yPos, bool extend, bool exclude)
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
            m_boxes.clear();
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
    QRect box;
    if (m_boxes.contains(m_lastPos)) {
        box = m_boxes.value(m_lastPos);
    }
    qDebug() << "===== ADDED CONTROL POINT; TOTAL: " << "EXCLUDING: " << exclude << ", " << points.size() << ", CENTERS: " << pointsTypes;
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(QRect(), points, pointsTypes, box);
    (void)QtConcurrent::run(&AutomaskHelper::generateImage, this, previewFile);
}

void AutomaskHelper::addMonitorControlRect(const QString &previewFile, int position, const QSize frameSize, const QRect rect, bool extend, bool exclude)
{
    m_lastPos = position;
    QList<QPoint> pointsList;
    if (!extend) {
        m_includePoints.clear();
        m_excludePoints.clear();
        m_boxes.clear();
    }
    m_boxes.insert(m_lastPos, rect);
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
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(QRect(), points, pointsTypes, rect);
    (void)QtConcurrent::run(&AutomaskHelper::generateImage, this, previewFile);
}

void AutomaskHelper::generateImage(const QString &previewFile)
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
    QRect box;
    if (m_boxes.contains(m_lastPos)) {
        box = m_boxes.value(m_lastPos);
    }
    bool ok;
    QDir maskSrcFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    SamInterface sam;
    std::pair<QString, QString> maskScript = {sam.venvPythonExecs().python, sam.getScript(QStringLiteral("automask/sam-objectmask.py"))};
    QStringList args = {
        maskScript.second, QStringLiteral("-I"), maskSrcFolder.absolutePath(),     QStringLiteral("-F"), QString::number(m_lastPos),    QStringLiteral("-O"),
        previewFile,       QStringLiteral("-M"), KdenliveSettings::samModelFile(), QStringLiteral("-C"), SamInterface::configForModel()};
    if (!pointsList.isEmpty()) {
        args << QStringLiteral("-P") << QStringLiteral("%1=%2").arg(m_lastPos).arg(pointsList.join(QLatin1Char(','))) << QStringLiteral("-L")
             << QStringLiteral("%1=%2").arg(m_lastPos).arg(labelsList.join(QLatin1Char(',')));
    }
    if (!KdenliveSettings::samDevice().isEmpty()) {
        args << QStringLiteral("-D") << KdenliveSettings::samDevice();
    }
    if (!box.isNull()) {
        args << QStringLiteral("-B") << QStringLiteral("%1=%2,%3,%4,%5").arg(m_lastPos).arg(box.x()).arg(box.y()).arg(box.right()).arg(box.bottom());
    }
    qDebug() << "---- STARTING IMAGE GENERATION: " << maskScript.first << " = " << args;
    scriptJob.setProcessChannelMode(QProcess::MergedChannels);
    scriptJob.start(maskScript.first, args);
    scriptJob.waitForFinished(-1);
    if (!QFile::exists(previewFile) || scriptJob.exitStatus() != 0) {
        // TODO error handling
        const QString logDetails = scriptJob.readAllStandardOutput();
        QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to render mask %1", previewFile)),
                                  Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, logDetails));
    } else {
        QUrl url = QUrl::fromLocalFile(previewFile);
        url.setQuery(QStringLiteral("pos=%1&ctrl=%2").arg(m_lastPos).arg(QDateTime::currentSecsSinceEpoch()));
        qDebug() << "---- IMAGE GENERATION DONE; RESULTING URL: " << url;
        pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay = url;
        Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();
    }
}

bool AutomaskHelper::generateMask(const QString &binId, const QString &maskName, const QPoint &zone)
{
    // Generate params
    QMap<int, QString> maskParams;
    bool ok;
    QDir maskSrcFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return false;
    }
    if (!m_includePoints.contains(0) && !m_boxes.contains(0)) {
        KMessageBox::information(pCore->getMonitor(Kdenlive::ClipMonitor), i18n("You must define include points in the first video frame of the sequence."));
        pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(zone.x());
        return false;
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
        return false;
    }
    maskParams.insert(MaskTask::OUTPUTFOLDER, maskSrcFolder.absolutePath());
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
        if (!pointsList.isEmpty()) {
            QString pointsValue = QStringLiteral("%1=").arg(f);
            QString labelsValue = pointsValue;
            pointsValue.append(pointsList.join(QLatin1Char(',')));
            labelsValue.append(labelsList.join(QLatin1Char(',')));
            fullPointsList << pointsValue;
            fullLabelsList << labelsValue;
        }
        if (m_boxes.contains(f)) {
            QRect rect = m_boxes.value(f);
            const QString boxValue = QStringLiteral("%1=%2,%3,%4,%5")
                                         .arg(QString::number(f), QString::number(rect.x()), QString::number(rect.y()), QString::number(rect.right()),
                                              QString::number(rect.bottom()));
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
        bool ok;
        QDir maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
        if (!ok) {
            return false;
        }
        int ix = 1;
        // QString baseName = QStringLiteral("%1-%2-%3.mkv").arg(clip->getControlUuid()).arg(zone.x()).arg(zone.y());
        QString baseName = QStringLiteral("%1-%2-%3").arg(QStringUtils::getCleanFileName(maskName)).arg(zone.x()).arg(zone.y());
        QString outputFile = maskFolder.absoluteFilePath(baseName + QStringLiteral(".mkv"));
        while (QFile::exists(outputFile)) {
            baseName = QStringLiteral("%1-%2.mkv").arg(baseName).arg(ix, 10, 4);
            outputFile = maskFolder.absoluteFilePath(baseName);
            ix++;
        }
        maskParams.insert(MaskTask::OUTPUTFILE, outputFile);
        SamInterface sam;
        std::pair<QString, QString> maskScript = {sam.venvPythonExecs().python, sam.getScript(QStringLiteral("automask/sam-objectmask.py"))};
        MaskTask::start(ObjectId(KdenliveObjectType::BinClip, binId.toInt(), QUuid()), maskParams, maskScript, zone.x(), zone.y(), clip.get());
    }
    return true;
}

void AutomaskHelper::monitorSeek(int)
{
    /*pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay =
        QUrl::fromLocalFile(QStringLiteral("/tmp/out_dir/color_img-%1.png").arg(pos, 5, 10, QLatin1Char('0')));
    Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();*/
}
