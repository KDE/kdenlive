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

void AutomaskHelper::addMonitorControlPoint(int position, const QSize frameSize, int xPos, int yPos, bool extend, bool exclude)
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
        if (!extend && (m_includePoints.contains(m_lastPos) || m_boxes.contains(m_lastPos))) {
            m_includePoints.remove(m_lastPos);
            m_excludePoints.remove(m_lastPos);
            m_boxes.remove(m_lastPos);
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
    QVariantList keyframes;
    for (auto i = m_includePoints.cbegin(), end = m_includePoints.cend(); i != end; ++i) {
        keyframes << i.key();
    }
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(QRect(), points, pointsTypes, keyframes, box);
    generateImage();
}

void AutomaskHelper::moveMonitorControlPoint(int ix, int position, const QSize frameSize, int xPos, int yPos)
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
    QVariantList keyframes;
    for (auto i = m_includePoints.cbegin(), end = m_includePoints.cend(); i != end; ++i) {
        keyframes << i.key();
    }
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(QRect(), points, pointsTypes, keyframes, box);
    generateImage();
}

void AutomaskHelper::addMonitorControlRect(int position, const QSize frameSize, const QRect rect, bool extend)
{
    m_lastPos = position;
    QList<QPoint> pointsList;
    if (!extend && (m_includePoints.contains(m_lastPos) || m_boxes.contains(m_lastPos))) {
        m_includePoints.remove(m_lastPos);
        m_excludePoints.remove(m_lastPos);
        m_boxes.remove(m_lastPos);
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
    QVariantList keyframes;
    for (auto i = m_includePoints.cbegin(), end = m_includePoints.cend(); i != end; ++i) {
        keyframes << i.key();
    }
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(QRect(), points, pointsTypes, keyframes, rect);
    generateImage();
}

void AutomaskHelper::launchSam(const QDir &previewFolder, int offset, const ObjectId &ownerForFilter, bool autoAdd, int previewPos)
{
    m_ownerForFilter = ownerForFilter;
    m_maskCreationMode = true;
    QStringList pointsList;
    QStringList labelsList;
    m_previewFolder = previewFolder;
    m_offset = offset;
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
    QStringList args = {maskScript.second,
                        QStringLiteral("-I"),
                        maskSrcFolder.absolutePath(),
                        QStringLiteral("-F"),
                        QString::number(m_lastPos),
                        QStringLiteral("-O"),
                        previewFolder.absolutePath(),
                        QStringLiteral("-M"),
                        KdenliveSettings::samModelFile(),
                        QStringLiteral("-C"),
                        SamInterface::configForModel()};
    if (!pointsList.isEmpty()) {
        args << QStringLiteral("-P") << QStringLiteral("%1=%2").arg(m_lastPos).arg(pointsList.join(QLatin1Char(','))) << QStringLiteral("-L")
             << QStringLiteral("%1=%2").arg(m_lastPos).arg(labelsList.join(QLatin1Char(',')));
    }
    if (!KdenliveSettings::samDevice().isEmpty()) {
        args << QStringLiteral("-D") << KdenliveSettings::samDevice();
    }
    if (KdenliveSettings::sam_offload_video()) {
        args << QStringLiteral("--offload");
    }
    if (!box.isNull()) {
        args << QStringLiteral("-B") << QStringLiteral("%1=%2,%3,%4,%5").arg(m_lastPos).arg(box.x()).arg(box.y()).arg(box.right()).arg(box.bottom());
    }
    if (KdenliveSettings::maskBorderWidth() > 0) {
        args << QStringLiteral("--border") << QString::number(KdenliveSettings::maskBorderWidth()) << QStringLiteral("--bordercolor")
             << QStringLiteral("%1,%2,%3,%4")
                    .arg(KdenliveSettings::maskBorderColor().red())
                    .arg(KdenliveSettings::maskBorderColor().green())
                    .arg(KdenliveSettings::maskBorderColor().blue())
                    .arg(KdenliveSettings::maskBorderColor().alpha());
    }
    args << QStringLiteral("--color")
         << QStringLiteral("%1,%2,%3,%4")
                .arg(KdenliveSettings::maskColor().red())
                .arg(KdenliveSettings::maskColor().green())
                .arg(KdenliveSettings::maskColor().blue())
                .arg(KdenliveSettings::maskColor().alpha());

    connect(&m_samProcess, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        if (state == QProcess::NotRunning) {
            qDebug() << "===== SAM SCRIPT TERMINATED ========";
            pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
            m_jobStatus = QProcess::NotRunning;
            bool jobFailed = m_killedOnRequest;
            if (m_killedOnRequest) {
                Q_EMIT showMessage(QStringLiteral(), KMessageWidget::Information);
            } else if (m_samProcess.exitStatus() == QProcess::CrashExit || m_samProcess.exitCode() != 0) {
                jobFailed = true;
                Q_EMIT showMessage(m_errorLog, KMessageWidget::Warning);
            }
            m_maskCreationMode = false;
            Q_EMIT samJobFinished(jobFailed);
        }
        m_errorLog.clear();
        m_killedOnRequest = false;
    });
    connect(&m_samProcess, &QProcess::readyReadStandardOutput, this, [this, autoAdd]() {
        const QString command = m_samProcess.readAllStandardOutput().simplified();
        if (command.startsWith(QLatin1String("preview ok"))) {
            // Load preview image
            int frame = command.section(QLatin1Char(' '), -1).toInt();
            m_jobStatus = QProcess::NotRunning;
            QUrl url = QUrl::fromLocalFile(m_previewFolder.absoluteFilePath(QStringLiteral("preview-%1.png").arg(frame, 5, 10, QLatin1Char('0'))));
            url.setQuery(QStringLiteral("pos=%1&ctrl=%2").arg(m_lastPos).arg(QDateTime::currentSecsSinceEpoch()));
            qDebug() << "---- IMAGE GENERATION DONE; RESULTING URL: " << url;
            pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay = url;
            Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();
        } else if (command == QLatin1String("mask ok")) {
            m_jobStatus = QProcess::NotRunning;
            auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
            Q_ASSERT(binClip);
            MaskTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), m_ownerForFilter, m_maskParams, binClip.get(), autoAdd);
            // Ensure we hide the progress bar on completion
            Q_EMIT updateProgress(100);
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
            bool ok;
            int progress = output.toInt(&ok);
            if (ok) {
                Q_EMIT updateProgress(progress);
            }
        } else {
            m_errorLog.append(output);
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
    if (previewPos > -1 && (!m_includePoints.isEmpty() || !m_boxes.isEmpty())) {
        m_lastPos = previewPos - m_offset;
        generateImage();
    }
}

void AutomaskHelper::generateImage()
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
    /*SamInterface sam;
    std::pair<QString, QString> maskScript = {sam.venvPythonExecs().python, sam.getScript(QStringLiteral("automask/sam-objectmask.py"))};*/
    QStringList args = {QStringLiteral("-F"), QString::number(m_lastPos)};
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
    if (KdenliveSettings::maskBorderWidth() > 0) {
        args << QStringLiteral("--border") << QString::number(KdenliveSettings::maskBorderWidth()) << QStringLiteral("--bordercolor")
             << QStringLiteral("%1,%2,%3,%4")
                    .arg(KdenliveSettings::maskBorderColor().red())
                    .arg(KdenliveSettings::maskBorderColor().green())
                    .arg(KdenliveSettings::maskBorderColor().blue())
                    .arg(KdenliveSettings::maskBorderColor().alpha());
    } else {
        args << QStringLiteral("--border") << QString::number(0);
    }
    args << QStringLiteral("--color")
         << QStringLiteral("%1,%2,%3,%4")
                .arg(KdenliveSettings::maskColor().red())
                .arg(KdenliveSettings::maskColor().green())
                .arg(KdenliveSettings::maskColor().blue())
                .arg(KdenliveSettings::maskColor().alpha());
    const QString samCommand = QStringLiteral("preview=%1\n").arg(args.join(QLatin1Char(' ')));
    qDebug() << "::: SEMNDING SAM COMMAND: " << samCommand;
    if (m_samProcess.state() == QProcess::Running) {
        m_jobStatus = QProcess::Running;
        m_samProcess.write(samCommand.toUtf8());
    } else {
        qDebug() << ":::: CANNOT COMMUNICATE WITH SAM PROCESS";
    }
}

void AutomaskHelper::updateMaskParams()
{
    QStringList args;
    if (KdenliveSettings::maskBorderWidth() > 0) {
        args << QStringLiteral("--border") << QString::number(KdenliveSettings::maskBorderWidth()) << QStringLiteral("--bordercolor")
             << QStringLiteral("%1,%2,%3,%4")
                    .arg(KdenliveSettings::maskBorderColor().red())
                    .arg(KdenliveSettings::maskBorderColor().green())
                    .arg(KdenliveSettings::maskBorderColor().blue())
                    .arg(KdenliveSettings::maskBorderColor().alpha());
    } else {
        args << QStringLiteral("--border") << QString::number(0);
    }
    args << QStringLiteral("--color")
         << QStringLiteral("%1,%2,%3,%4")
                .arg(KdenliveSettings::maskColor().red())
                .arg(KdenliveSettings::maskColor().green())
                .arg(KdenliveSettings::maskColor().blue())
                .arg(KdenliveSettings::maskColor().alpha());
    const QString samCommand = QStringLiteral("edit=%1\n").arg(args.join(QLatin1Char(' ')));
    qDebug() << "::: SEMNDING SAM COMMAND: " << samCommand;
    if (m_samProcess.state() == QProcess::Running) {
        m_jobStatus = QProcess::Running;
        m_samProcess.write(samCommand.toUtf8());
    } else {
        qDebug() << ":::: CANNOT COMMUNICATE WITH SAM PROCESS";
    }
}

bool AutomaskHelper::generateMask(const QString &binId, const QString &maskName, const QString &maskFile, const QPoint &zone)
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
    m_jobStatus = QProcess::Running;
    m_samProcess.write(QStringLiteral("render=%1\n").arg(maskSrcFolder.absolutePath()).toUtf8());
    m_maskParams.insert(MaskTask::OUTPUTFOLDER, maskSrcFolder.absolutePath());
    m_maskParams.insert(MaskTask::NAME, maskName);
    // Generate points strings
    QStringList fullIncludePoints;
    QStringList fullExcludePoints;
    QStringList fullBoxList;
    for (auto i = m_includePoints.cbegin(), end = m_includePoints.cend(); i != end; ++i) {
        auto pts = i.value();
        QStringList pointsData;
        for (auto &p : pts) {
            pointsData << QString::number(p.x()) << QString::number(p.y());
        }
        fullIncludePoints << QStringLiteral("%1=%2").arg(i.key()).arg(pointsData.join(QLatin1Char(',')));
    }
    for (auto i = m_excludePoints.cbegin(), end = m_excludePoints.cend(); i != end; ++i) {
        auto pts = i.value();
        QStringList pointsData;
        for (auto &p : pts) {
            pointsData << QString::number(p.x()) << QString::number(p.y());
        }
        fullExcludePoints << QStringLiteral("%1=%2").arg(i.key()).arg(pointsData.join(QLatin1Char(',')));
    }
    for (auto i = m_boxes.cbegin(), end = m_boxes.cend(); i != end; ++i) {
        auto pts = i.value();
        QStringList pointsData = {QString::number(pts.x()), QString::number(pts.y()), QString::number(pts.right()), QString::number(pts.bottom())};
        fullBoxList << QStringLiteral("%1=%2").arg(i.key()).arg(pointsData.join(QLatin1Char(',')));
    }

    if (!fullIncludePoints.isEmpty()) {
        m_maskParams.insert(MaskTask::INCLUDEPOINTS, fullIncludePoints.join(QLatin1Char(';')));
    }
    if (!fullExcludePoints.isEmpty()) {
        m_maskParams.insert(MaskTask::EXCLUDEPOINTS, fullExcludePoints.join(QLatin1Char(';')));
    }
    if (!fullBoxList.isEmpty()) {
        m_maskParams.insert(MaskTask::BOXES, fullBoxList.join(QLatin1Char(';')));
    }
    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
    if (clip) {
        bool ok;
        QDir maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
        if (!ok) {
            return false;
        }
        int ix = 1;
        QString outputFile;
        if (!maskFile.isEmpty()) {
            outputFile = maskFile;
        } else {
            const QString baseName = QStringLiteral("%1-%2-%3").arg(QStringUtils::getCleanFileName(maskName)).arg(zone.x()).arg(zone.y());
            outputFile = maskFolder.absoluteFilePath(baseName + QStringLiteral(".mkv"));
            while (QFile::exists(outputFile)) {
                QString secondName = QStringLiteral("%1-%2.mkv").arg(baseName).arg(ix, 4, 10, QLatin1Char('0'));
                outputFile = maskFolder.absoluteFilePath(secondName);
                ix++;
            }
        }
        m_maskParams.insert(MaskTask::OUTPUTFILE, outputFile);
    }
    return true;
}

void AutomaskHelper::abortJob()
{
    if (m_samProcess.state() == QProcess::Running) {
        m_killedOnRequest = true;
        m_samProcess.kill();
    } else {
        Q_EMIT samJobFinished(false);
    }
}

bool AutomaskHelper::jobRunning() const
{
    return m_maskCreationMode || m_jobStatus == QProcess::Running;
}

void AutomaskHelper::sceneUpdated(MonitorSceneType sceneType)
{
    if (sceneType == MonitorSceneAutoMask) {
        monitorSeek(m_seekPos);
    }
    disconnect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::sceneChanged, this, &AutomaskHelper::sceneUpdated);
}

void AutomaskHelper::monitorSeek(int pos)
{
    Monitor *mon = pCore->getMonitor(Kdenlive::ClipMonitor);
    if (!mon->effectSceneDisplayed(MonitorSceneAutoMask)) {
        qDebug() << "::::: SEEKING WHILE MONITOR NOT READY!!!";
        // Monitor mask scene is not yet loaded
        m_seekPos = pos;
        connect(mon, &Monitor::sceneChanged, this, &AutomaskHelper::sceneUpdated, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
        return;
    }
    pos -= m_offset;
    QUrl url = QUrl::fromLocalFile(m_previewFolder.absoluteFilePath(QStringLiteral("preview-%1.png").arg(pos, 5, 10, QLatin1Char('0'))));
    mon->getControllerProxy()->m_previewOverlay = url;
    Q_EMIT mon->getControllerProxy()->previewOverlayChanged();
    // Update keyframe points
    QList<QPoint> pointsList;
    QVariantList points;
    QVariantList pointsTypes;
    QSize frameSize = pCore->getCurrentFrameDisplaySize();
    pointsList = m_includePoints.value(pos);
    for (auto &p : pointsList) {
        points << QPointF(double(p.x()) / frameSize.width(), double(p.y()) / frameSize.height());
        pointsTypes << 1;
    }
    pointsList = m_excludePoints.value(pos);
    for (auto &p : pointsList) {
        points << QPointF(double(p.x()) / frameSize.width(), double(p.y()) / frameSize.height());
        pointsTypes << 0;
    }
    QVariantList keyframes;
    for (auto i = m_includePoints.cbegin(), end = m_includePoints.cend(); i != end; ++i) {
        keyframes << i.key();
    }
    QRect box;
    if (m_boxes.contains(pos)) {
        box = m_boxes.value(pos);
    }
    mon->setUpEffectGeometry(QRect(), points, pointsTypes, keyframes, box);
}

void AutomaskHelper::terminate()
{
    if (m_samProcess.state() == QProcess::Running) {
        m_killedOnRequest = true;
        m_samProcess.blockSignals(true);
        m_samProcess.kill();
        // m_samProcess.write("q\n");
        // m_samProcess.waitForFinished(1000);
    }
}

void AutomaskHelper::cleanup()
{
    // We switched to another clip, delete all data if any
    m_includePoints.clear();
    m_excludePoints.clear();
    m_boxes.clear();
    // Remove source and preview data
    if (m_previewFolder.dirName() == QLatin1String("source-frames") && m_previewFolder.exists()) {
        m_previewFolder.removeRecursively();
    }
}

void AutomaskHelper::loadData(const QString ipoints, const QString epoints, const QString boxes, int in, const QDir &previewFolder)
{
    m_includePoints.clear();
    m_excludePoints.clear();
    m_boxes.clear();
    m_offset = in;
    m_previewFolder = previewFolder;
    QSet<int> keyframes;
    QStringList pts = ipoints.split(QLatin1Char(';'));
    for (auto &p : pts) {
        int frame = p.section(QLatin1Char('='), 0, 0).toInt();
        keyframes << frame;
        const QStringList pointsData = p.section(QLatin1Char('='), 1).split(QLatin1Char(','));
        if (pointsData.size() % 2 != 0) {
            // Invalid data
            continue;
        }
        QList<QPoint> pointsList;
        for (int ix = 0; ix < pointsData.size(); ix += 2) {
            pointsList << QPoint(pointsData.at(ix).toInt(), pointsData.at(ix + 1).toInt());
        }
        m_includePoints.insert(frame, pointsList);
    }
    pts = epoints.split(QLatin1Char(';'));
    for (auto &p : pts) {
        int frame = p.section(QLatin1Char('='), 0, 0).toInt();
        keyframes << frame;
        const QStringList pointsData = p.section(QLatin1Char('='), 1).split(QLatin1Char(','));
        if (pointsData.size() % 2 != 0) {
            // Invalid data
            continue;
        }
        QList<QPoint> pointsList;
        for (int ix = 0; ix < pointsData.size(); ix += 2) {
            pointsList << QPoint(pointsData.at(ix).toInt(), pointsData.at(ix + 1).toInt());
        }
        m_excludePoints.insert(frame, pointsList);
    }
    pts = boxes.split(QLatin1Char(';'));
    for (auto &p : pts) {
        int frame = p.section(QLatin1Char('='), 0, 0).toInt();
        keyframes << frame;
        QStringList pointsData = p.section(QLatin1Char('='), 1).split(QLatin1Char(','));
        if (pointsData.size() != 4) {
            // Invalid data
            continue;
        }
        QRect r(pointsData.at(0).toInt(), pointsData.at(1).toInt(), pointsData.at(2).toInt(), pointsData.at(3).toInt());
        m_boxes.insert(frame, r);
    }
}
