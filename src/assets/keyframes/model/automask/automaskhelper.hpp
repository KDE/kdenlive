/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"

#include <KMessageWidget>

#include <QDir>
#include <QObject>
#include <QPoint>
#include <QProcess>
#include <QVariant>

#include <memory>

class Monitor;
class ProjectClip;
class SamInterface;

class AutomaskHelper : public QObject
{
    Q_OBJECT

public:
    /** @brief Construct a keyframe list bound to the given effect
       @param init_value is the value taken by the param at time 0.
       @param model is the asset this parameter belong to
       @param index is the index of this parameter in its model
     */
    explicit AutomaskHelper(QObject *parent = nullptr);
    void launchSam(const QDir &previewFolder, int offset, const ObjectId &ownerForFilter = ObjectId(), bool autoAdd = false);
    bool jobRunning() const;
    void terminate();
    /** @brief Remove all masks tmp data */
    void cleanup();
    void loadData(const QString points, const QString labels, const QString boxes, int in, const QDir &previewFolder);

public Q_SLOTS:
    bool generateMask(const QString &binId, const QString &maskName, const QPoint &zone);
    void monitorSeek(int pos);
    void addMonitorControlPoint(int position, const QSize frameSize, int xPos, int yPos, bool extend, bool exclude);
    void moveMonitorControlPoint(int ix, int position, const QSize frameSize, int xPos, int yPos);
    void addMonitorControlRect(int position, const QSize frameSize, const QRect rect, bool extend);
    void abortJob();

private:
    Monitor *m_monitor;
    std::shared_ptr<ProjectClip> m_clip;
    int m_lastPos{0};
    int m_offset{0};
    QMap<int, QList<QPoint>> m_includePoints;
    QMap<int, QList<QPoint>> m_excludePoints;
    QMap<int, QRect> m_boxes;
    QProcess m_samProcess;
    QDir m_previewFolder;
    QString m_errorLog;
    QProcess::ProcessState m_jobStatus{QProcess::NotRunning};
    QMap<int, QString> m_maskParams;
    QString m_binId;
    bool m_killedOnRequest{false};
    bool m_maskCreationMode{false};
    ObjectId m_ownerForFilter{KdenliveObjectType::NoItem, {}};

private Q_SLOTS:
    void generateImage();

Q_SIGNALS:
    void showMessage(const QString &message, KMessageWidget::MessageType type = KMessageWidget::Information);
    void updateProgress(int progress);
    void samJobFinished(bool failed);
};
