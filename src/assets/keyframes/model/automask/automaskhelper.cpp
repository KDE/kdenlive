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
    generateImage(previewFile);
    //(void)QtConcurrent::run(&AutomaskHelper::generateImage, this, previewFile);
}

void AutomaskHelper::moveMonitorControlPoint(const QString &previewFile, int ix, int position, const QSize frameSize, int xPos, int yPos)
{
    const QPoint p(xPos, yPos);
    m_lastPos = position;
    QList<QPoint> pointsList;
    if (m_excludePoints.contains(position) && ix >= m_includePoints.value(position).size()) {
        // We moved an exclude point
        ix -= m_includePoints.value(position).size();
        m_excludePoints[position][ix] = p;
    } else {
        m_includePoints[position][ix] = p;
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
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(QRect(), points, pointsTypes, box);
    generateImage(previewFile);
    //(void)QtConcurrent::run(&AutomaskHelper::generateImage, this, previewFile);
}

void AutomaskHelper::addMonitorControlRect(const QString &previewFile, int position, const QSize frameSize, const QRect rect, bool extend)
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
    generateImage(previewFile);
    //(void)QtConcurrent::run(&AutomaskHelper::generateImage, this, previewFile);
}

void AutomaskHelper::launchSam(const QString &previewFile)
{
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
    connect(&m_samProcess, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        if (state == QProcess::NotRunning) {
            qDebug() << "===== SAM SCRIPT TERMINATED ========";
            if (m_samProcess.exitStatus() == QProcess::CrashExit) {
                const QString crashLog = m_samProcess.readAllStandardError();
                Q_EMIT showMessage(crashLog, KMessageWidget::Warning);
            }
        }
    });
    connect(&m_samProcess, &QProcess::readyReadStandardOutput, this, [this, previewFile]() {
        const QString command = m_samProcess.readAllStandardOutput().simplified();
        if (command == QLatin1String("preview ok")) {
            // Load preview image
            QUrl url = QUrl::fromLocalFile(previewFile);
            url.setQuery(QStringLiteral("pos=%1&ctrl=%2").arg(m_lastPos).arg(QDateTime::currentSecsSinceEpoch()));
            qDebug() << "---- IMAGE GENERATION DONE; RESULTING URL: " << url;
            pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay = url;
            Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();
        } else if (command == QLatin1String("mask ok")) {
            auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
            MaskTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), m_maskParams, binClip.get());
            /*MaskInfo mask;
            mask.maskName = m_maskParams.value(MaskTask::NAME);
            mask.maskFile = m_maskParams.value(MaskTask::OUTPUTFILE);
            mask.in = m_maskParams.value(MaskTask::ZONEIN).toInt();
            mask.out = m_maskParams.value(MaskTask::ZONEOUT).toInt();
            QMetaObject::invokeMethod(binClip.get(), "addMask", Qt::QueuedConnection, Q_ARG(MaskInfo, mask));*/
        } else if (command.startsWith(QLatin1String("INFO:"))) {
            const QString msg = command.section(QLatin1Char(':'), 1);
            Q_EMIT showMessage(msg, KMessageWidget::Information);
        } else {
            qDebug() << " OUTPUT READREADY : " << command;
        }
    });
    connect(&m_samProcess, &QProcess::readyReadStandardError, this, [this]() {
        QString output = m_samProcess.readAllStandardError();
        if (output.contains(QLatin1String("%|"))) {
            output = output.section(QLatin1String("%|"), 0, 0).section(QLatin1Char(' '), -1);
            qDebug() << " ERROR READREADY : " << output;
            bool ok;
            int progress = output.toInt(&ok);
            if (ok) {
                Q_EMIT updateProgress(progress);
            }
        } else {
            qDebug() << " ERROR : " << output;
        }
    });
    m_samProcess.setProcessChannelMode(QProcess::SeparateChannels);
    m_samProcess.setProgram(maskScript.first);
    QDir venvDir = QFileInfo(maskScript.first).dir();
    venvDir.cdUp();
    m_samProcess.setWorkingDirectory(venvDir.absolutePath());
    m_samProcess.setArguments(args);
    m_samProcess.start(QIODevice::ReadWrite | QIODevice::Text);
    m_samProcess.waitForStarted();
    // scriptJob.waitForFinished(-1);
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
    /*SamInterface sam;
    std::pair<QString, QString> maskScript = {sam.venvPythonExecs().python, sam.getScript(QStringLiteral("automask/sam-objectmask.py"))};*/
    QStringList args; /* = {
         maskScript.second, QStringLiteral("-I"), maskSrcFolder.absolutePath(),     QStringLiteral("-F"), QString::number(m_lastPos),    QStringLiteral("-O"),
         previewFile,       QStringLiteral("-M"), KdenliveSettings::samModelFile(), QStringLiteral("-C"), SamInterface::configForModel()};*/
    if (!pointsList.isEmpty()) {
        args << QStringLiteral("-P") << QStringLiteral("%1=%2").arg(m_lastPos).arg(pointsList.join(QLatin1Char(','))) << QStringLiteral("-L")
             << QStringLiteral("%1=%2").arg(m_lastPos).arg(labelsList.join(QLatin1Char(',')));
    }
    /*if (!KdenliveSettings::samDevice().isEmpty()) {
        args << QStringLiteral("-D") << KdenliveSettings::samDevice();
    }*/
    if (!box.isNull()) {
        args << QStringLiteral("-B") << QStringLiteral("%1=%2,%3,%4,%5").arg(m_lastPos).arg(box.x()).arg(box.y()).arg(box.right()).arg(box.bottom());
    }
    const QString samCommand = QStringLiteral("preview=%1\n").arg(args.join(QLatin1Char(' ')));
    qDebug() << "::: SEMNDING SAM COMMAND: " << samCommand;
    if (m_samProcess.state() == QProcess::Running) {
        m_samProcess.write(samCommand.toUtf8());
    } else {
        qDebug() << ":::: CANNOT COMMUNICATE WITH SAM PROCESS";
    }
    return;
    // scriptJob.setProcessChannelMode(QProcess::MergedChannels);
    // scriptJob.start(maskScript.first, args);
    // scriptJob.waitForFinished(-1);
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
    m_binId = binId;
    m_maskParams.insert(MaskTask::ZONEIN, QString::number(zone.x()));
    m_maskParams.insert(MaskTask::ZONEOUT, QString::number(zone.y()));
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
    m_maskParams.insert(MaskTask::INPUTFOLDER, maskSrcFolder.absolutePath());
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
    // Launch the sam analysis process
    m_samProcess.write(QStringLiteral("render=%1\n").arg(maskSrcFolder.absolutePath()).toUtf8());
    m_maskParams.insert(MaskTask::OUTPUTFOLDER, maskSrcFolder.absolutePath());
    m_maskParams.insert(MaskTask::NAME, maskName);
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
        m_maskParams.insert(MaskTask::POINTS, fullPointsList.join(QLatin1Char(';')));
        m_maskParams.insert(MaskTask::LABELS, fullLabelsList.join(QLatin1Char(';')));
    }
    if (!fullBoxList.isEmpty()) {
        m_maskParams.insert(MaskTask::BOX, fullBoxList.join(QLatin1Char(';')));
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
        m_maskParams.insert(MaskTask::OUTPUTFILE, outputFile);
    }
    return true;
}

void AutomaskHelper::abortJob()
{
    if (m_samProcess.state() == QProcess::Running) {
        m_samProcess.kill();
    }
}

void AutomaskHelper::monitorSeek(int)
{
    /*pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay =
        QUrl::fromLocalFile(QStringLiteral("/tmp/out_dir/color_img-%1.png").arg(pos, 5, 10, QLatin1Char('0')));
    Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();*/
}
