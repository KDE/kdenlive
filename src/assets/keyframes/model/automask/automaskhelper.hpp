/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <QVariant>

#include <memory>

class Monitor;
class ProjectClip;

class AutomaskHelper : public QObject
{
    Q_OBJECT

public:
    /** @brief Construct a keyframe list bound to the given effect
       @param init_value is the value taken by the param at time 0.
       @param model is the asset this parameter belong to
       @param index is the index of this parameter in its model
     */
    explicit AutomaskHelper(Monitor *monitor, std::shared_ptr<ProjectClip> clip, QObject *parent = nullptr);
    /** @brief Send data update to the monitor
     */
    void refreshParams(int pos);

public Q_SLOTS:
    void generatePreview();

private:
    Monitor *m_monitor;
    std::shared_ptr<ProjectClip> m_clip;
    int m_lastPos{0};
    QMap<int, QList<QPoint>> m_includePoints;
    QMap<int, QList<QPoint>> m_excludePoints;

private Q_SLOTS:
    void slotUpdateFromMonitorData(const QVariantList &v);
    void addMonitorControlPoint(int xPos, int yPos, bool exclude);
    void generateImage();
    void doGeneratePreview();
};
