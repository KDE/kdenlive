/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *                                                                         *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "stabilizejob.hpp"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "jobmanager.h"
#include "kdenlivesettings.h"
#include "project/clipstabilize.h"

#include <klocalizedstring.h>

#include <memory>
#include <mlt++/Mlt.h>
StabilizeJob::StabilizeJob(const QString &binId, const QString &filterName, QString destUrl, std::unordered_map<QString, QString> filterParams)
    : MeltJob(binId, STABILIZEJOB, false, -1, -1)
    , m_filterName(filterName)
    , m_destUrl(std::move(destUrl))
    , m_filterParams(std::move(filterParams))
{
    Q_ASSERT(supportedFilters().count(filterName) > 0);
}

const QString StabilizeJob::getDescription() const
{
    return i18n("Stabilize clips");
}
void StabilizeJob::configureConsumer()
{
    m_consumer = std::make_unique<Mlt::Consumer>(*m_profile.get(), "xml", m_destUrl.toUtf8().constData());
    m_consumer->set("all", 1);
    m_consumer->set("title", "Stabilized");
    m_consumer->set("real_time", -1);
}

void StabilizeJob::configureFilter()
{

    m_filter = std::make_unique<Mlt::Filter>(*m_profile.get(), m_filterName.toUtf8().data());
    if ((m_filter == nullptr) || !m_filter->is_valid()) {
        m_errorMessage.append(i18n("Cannot create filter %1", m_filterName));
        return;
    }

    // Process filter params
    for (const auto &it : m_filterParams) {
        m_filter->set(it.first.toUtf8().constData(), it.second.toUtf8().constData());
    }
    QString targetFile = m_destUrl + QStringLiteral(".trf");
    m_filter->set("filename", targetFile.toUtf8().constData());
    m_filter->set_in_and_out(0, length - 1);
}

// static
std::unordered_set<QString> StabilizeJob::supportedFilters()
{
    return {QLatin1String("vidstab")};
}

// static
int StabilizeJob::prepareJob(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString,
                             const QString &filterName)
{
    Q_ASSERT(supportedFilters().count(filterName) > 0);
    if (filterName == QLatin1String("vidstab")) {
        // vidstab
        QScopedPointer<ClipStabilize> d(new ClipStabilize(binIds, filterName));
        if (d->exec() == QDialog::Accepted) {
            std::unordered_map<QString, QString> filterParams = d->filterParams();
            QString destination = d->destination();
            std::unordered_map<QString, QString> destinations; // keys are binIds, values are path to target files
            for (const auto &binId : binIds) {
                auto binClip = pCore->projectItemModel()->getClipByBinID(binId.section(QLatin1Char('/'), 0, 0));
                if (binIds.size() == 1) {
                    // We only have one clip, destination points to the final url
                    destinations[binId] = destination;
                } else {
                    // Filter several clips, destination points to a folder
                    QString mltfile = destination + QFileInfo(binClip->url()).fileName() + QStringLiteral(".mlt");
                    destinations[binId] = mltfile;
                }
            }
            // Now we have to create the jobs objects. This is trickier than usual, since the parameters are different for each job (each clip has its own
            // destination). We have to construct a lambda that does that.

            auto createFn = [dest = std::move(destinations), fName = filterName, fParams = std::move(filterParams)](const QString &id) {
                return std::make_shared<StabilizeJob>(id, fName, dest.at(id), fParams);
            };

            // We are now all set to create the job. Note that we pass all the parameters directly through the lambda, hence there are no extra parameters to
            // the function
            using local_createFn_t = std::function<std::shared_ptr<StabilizeJob>(const QString &)>;
            return emit ptr->startJob<StabilizeJob>(binIds, parentId, std::move(undoString), local_createFn_t(std::move(createFn)));
        }
    }
    return -1;
}

bool StabilizeJob::commitResult(Fun &undo, Fun &redo)
{
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        qDebug() << "ERROR: Trying to consume invalid results";
        return false;
    }
    m_resultConsumed = true;
    if (!m_successful) {
        return false;
    }

    auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);

    // We store the stabilized clips in a sub folder with this name
    const QString folderName(i18n("Stabilized"));

    QString folderId = QStringLiteral("-1");
    bool found = false;
    // We first try to see if it exists
    auto containingFolder = std::static_pointer_cast<ProjectFolder>(binClip->parent());
    for (int i = 0; i < containingFolder->childCount(); ++i) {
        auto currentItem = std::static_pointer_cast<AbstractProjectItem>(containingFolder->child(i));
        if (currentItem->itemType() == AbstractProjectItem::FolderItem && currentItem->name() == folderName) {
            found = true;
            folderId = currentItem->clipId();
            break;
        }
    }

    if (!found) {
        // if it was not found, we create it
        pCore->projectItemModel()->requestAddFolder(folderId, folderName, binClip->parent()->clipId(), undo, redo);
    }

    auto id = ClipCreator::createClipFromFile(m_destUrl, folderId, pCore->projectItemModel(), undo, redo);
    return id != QStringLiteral("-1");
}
