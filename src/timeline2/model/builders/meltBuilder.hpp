/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef MELTBUILDER_H
#define MELTBUILDER_H
#include <memory>
#include <mlt++/MltTractor.h>
#include <mlt++/MltMultitrack.h>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUuid>
#include <QDateTime>

class TimelineItemModel;
class ProjectItemModel;
class QProgressDialog;

/** @brief This function can be used to construct a TimelineModel object from a Mlt object hierarchy
 */

bool constructTimelineFromTractor(const QUuid &uuid, const std::shared_ptr<TimelineItemModel> &timeline, const std::shared_ptr<ProjectItemModel> &projectModel, Mlt::Tractor tractor, QProgressDialog *progressDialog, const QString &originalDecimalPoint, QStringList timelines = QStringList(), const QString &chunks = QString(), const QString &dirty = QString(), const QDateTime &documentDate = QDateTime(), int enablePreview = 0, bool *projectErrors = nullptr);

bool constructTimelineFromMelt(const QUuid &uuid, const std::shared_ptr<TimelineItemModel> &timeline, const std::shared_ptr<ProjectItemModel> &projectModel, Mlt::Multitrack tractor, QProgressDialog *progressDialog = nullptr, const QString &originalDecimalPoint = QString(), const QString &chunks = QString(), const QString &dirty = QString(), const QDateTime &documentDate = QDateTime(), int enablePreview = 0, bool *projectErrors = nullptr);

#endif
