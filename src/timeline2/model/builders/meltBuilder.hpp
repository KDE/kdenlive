/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef MELTBUILDER_H
#define MELTBUILDER_H
#include <memory>
#include <mlt++/MltTractor.h>
#include <QtCore/QString>
#include <QDateTime>

class TimelineItemModel;
class QProgressDialog;

/** @brief This function can be used to construct a TimelineModel object from a Mlt object hierarchy
 */
bool constructTimelineFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, Mlt::Tractor mlt_timeline, QProgressDialog *progressDialog = nullptr, const QString &originalDecimalPoint = QString(), const QString &chunks = QString(), const QString &dirty = QString(), const QDateTime &documentDate = QDateTime(), int enablePreview = 0, bool *projectErrors = nullptr);

#endif
