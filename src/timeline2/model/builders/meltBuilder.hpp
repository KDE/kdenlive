/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDateTime>
#include <QUuid>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <memory>
#include <mlt++/MltTractor.h>

class TimelineItemModel;
class ProjectItemModel;
class QProgressDialog;

/** @brief This function can be used to construct a TimelineModel object from a Mlt object hierarchy
 */

bool loadProjectBin(Mlt::Tractor tractor, const QUuid &activeUuid, QProgressDialog *progressDialog = nullptr);
void checkProjectWarnings();

bool constructTimelineFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, Mlt::Tractor mlt_timeline, QProgressDialog *progressDialog = nullptr,
                               const QString &originalDecimalPoint = QString(), const QString &chunks = QString(), bool enablePreview = false,
                               bool *projectErrors = nullptr);

bool constructTimelineFromTractor(const std::shared_ptr<TimelineItemModel> &timeline, const std::shared_ptr<ProjectItemModel> &projectModel,
                                  Mlt::Tractor tractor, QProgressDialog *progressDialog, const QString &originalDecimalPoint, const QString &chunks = QString(),
                                  const QString &dirty = QString(), bool enablePreview = false);
