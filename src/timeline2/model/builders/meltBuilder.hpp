/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QtCore/QString>
#include <memory>
#include <mlt++/MltTractor.h>

#include <QDateTime>
#include <QStringList>
#include <QUuid>

class TimelineItemModel;
class ProjectItemModel;
class QProgressDialog;

bool constructTimelineFromTractor(const QUuid &uuid, const std::shared_ptr<TimelineItemModel> &timeline, const std::shared_ptr<ProjectItemModel> &projectModel,
                                  Mlt::Tractor tractor, QProgressDialog *progressDialog, const QString &originalDecimalPoint,
                                  QStringList timelines = QStringList(), const QString &chunks = QString(), const QString &dirty = QString(),
                                  const QDateTime &documentDate = QDateTime(), int enablePreview = 0, bool *projectErrors = nullptr);

/** @brief This function can be used to construct a TimelineModel object from a Mlt object hierarchy
 */
bool constructTimelineFromMelt(const QUuid &uuid, const std::shared_ptr<TimelineItemModel> &timeline, Mlt::Multitrack mlt_timeline,
                               QProgressDialog *progressDialog = nullptr, const QString &originalDecimalPoint = QString(), const QString &chunks = QString(),
                               const QString &dirty = QString(), int enablePreview = 0, bool *projectErrors = nullptr);
