/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#ifndef KDENLIVEDOCOBJECTMODEL_H
#define KDENLIVEDOCOBJECTMODEL_H

#include <QObject>
#include "definitions.h"

class TimelineWidget;
class ProjectItemModel;

class KdenliveDocObjectModel : public QObject
{
    Q_OBJECT
public:
    explicit KdenliveDocObjectModel(TimelineWidget *timeline, std::shared_ptr<ProjectItemModel> projectModel, QObject *parent = nullptr);
    std::pair<int,int> getItemInOut(const ObjectId &id);
    int getItemPosition(const ObjectId &id);
    TimelineWidget *timeline();

private:
    TimelineWidget *m_timeline;
    std::shared_ptr<ProjectItemModel> m_projectModel;
};

#endif
