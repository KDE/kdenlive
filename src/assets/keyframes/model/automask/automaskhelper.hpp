/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <QPoint>
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

public Q_SLOTS:
    bool generateMask(const QString &binId, const QString &maskName, const QPoint &zone);
    void monitorSeek(int pos);
    void addMonitorControlPoint(const QString &previewFile, int position, const QSize frameSize, int xPos, int yPos, bool extend, bool exclude);
    void moveMonitorControlPoint(const QString &previewFile, int ix, int position, const QSize frameSize, int xPos, int yPos);
    void addMonitorControlRect(const QString &previewFile, int position, const QSize frameSize, const QRect rect, bool extend);

private:
    Monitor *m_monitor;
    std::shared_ptr<ProjectClip> m_clip;
    int m_lastPos{0};
    QMap<int, QList<QPoint>> m_includePoints;
    QMap<int, QList<QPoint>> m_excludePoints;
    QMap<int, QRect> m_boxes;

private Q_SLOTS:
    void generateImage(const QString &previewFile);
};
