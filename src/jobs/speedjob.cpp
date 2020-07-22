/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "speedjob.hpp"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "jobmanager.h"
#include "kdenlivesettings.h"
#include "project/clipstabilize.h"
#include "ui_scenecutdialog_ui.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QDialog>
#include <QDoubleSpinBox>

#include <KIO/RenameDialog>
#include <KUrlRequester>
#include <KLineEdit>
#include <mlt++/Mlt.h>

SpeedJob::SpeedJob(const QString &binId, double speed, bool warp_pitch, QString destUrl)
    : MeltJob(binId, SPEEDJOB, false, -1, -1)
    , m_speed(speed)
    , m_warp_pitch(warp_pitch)
    , m_destUrl(std::move(destUrl))
{
    m_requiresFilter = false;
}

const QString SpeedJob::getDescription() const
{
    return i18n("Change clip speed");
}

void SpeedJob::configureConsumer()
{
    m_consumer = std::make_unique<Mlt::Consumer>(*m_profile.get(), "xml", m_destUrl.toUtf8().constData());
    m_consumer->set("terminate_on_pause", 1);
    m_consumer->set("title", "Speed Change");
    m_consumer->set("real_time", -1);
}

void SpeedJob::configureProducer()
{
    if (!qFuzzyCompare(m_speed, 1.0)) {
        QString resource = m_producer->get("resource");
        m_producer = std::make_unique<Mlt::Producer>(*m_profile.get(), "timewarp", QStringLiteral("%1:%2").arg(QString::fromStdString(std::to_string(m_speed)), resource).toUtf8().constData());
        if (m_in > 0) {
            m_in /= m_speed;
        }
        if (m_out > 0) {
            m_out /= m_speed;
        }
        if (m_warp_pitch) {
            m_producer->set("warp_pitch", 1);
        }
    }
}

void SpeedJob::configureFilter() {}

// static
int SpeedJob::prepareJob(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString)
{
    // Show config dialog
    QDialog d(qApp->activeWindow());
    d.setWindowTitle(i18n("Clip Speed"));
    QDialogButtonBox buttonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    QLabel labUrl(&d);
    KUrlRequester fileUrl(&d);
    auto binClip = pCore->projectItemModel()->getClipByBinID(binIds.front().section(QLatin1Char('/'), 0, 0));
    QDir folder = QFileInfo(binClip->url()).absoluteDir();
    folder.mkpath(i18n("Speed Change"));
    folder.cd(i18n("Speed Change"));
    if (binIds.size() > 1) {
        labUrl.setText(i18n("Destination Folder"));
        fileUrl.setMode(KFile::Directory);
        fileUrl.setUrl(QUrl::fromLocalFile(folder.absolutePath()));
    } else {
        labUrl.setText(i18n("Destination File"));
        fileUrl.setMode(KFile::File);
        QString filePath = QFileInfo(binClip->url()).fileName().section(QLatin1Char('.'), 0, -2);
        filePath.append(QStringLiteral(".mlt"));
        fileUrl.setUrl(QUrl::fromLocalFile(folder.absoluteFilePath(filePath)));
    }
    QFontMetrics fm = fileUrl.lineEdit()->fontMetrics();
    fileUrl.setMinimumWidth(fm.boundingRect(fileUrl.text().left(50)).width() * 1.4);
    QLabel lab(&d);
    lab.setText(i18n("Percentage"));
    QDoubleSpinBox speedInput(&d);
    speedInput.setRange(-100000, 100000);
    speedInput.setValue(100);
    speedInput.setSuffix(QLatin1String("%"));
    speedInput.setFocus();
    speedInput.selectAll();
    QCheckBox cb(i18n("Pitch compensation"), &d);
    cb.setChecked(true);
    l->addWidget(&labUrl);
    l->addWidget(&fileUrl);
    l->addWidget(&lab);
    l->addWidget(&speedInput);
    l->addWidget(&cb);
    l->addWidget(&buttonBox);
    d.connect(&buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(&buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return -1;
    }
    double speed = speedInput.value();
    bool warp_pitch = cb.isChecked();
    std::unordered_map<QString, QString> destinations; // keys are binIds, values are path to target files
    for (const auto &binId : binIds) {
        QString mltfile;
        if (binIds.size() == 1) {
            // converting only 1 clip
            mltfile = fileUrl.url().toLocalFile();
        } else {
            QDir dir(fileUrl.url().toLocalFile());
            binClip = pCore->projectItemModel()->getClipByBinID(binId.section(QLatin1Char('/'), 0, 0));
            mltfile = QFileInfo(binClip->url()).fileName().section(QLatin1Char('.'), 0, -2);
            mltfile.append(QString("-%1.mlt").arg(QString::number((int)speed)));
            mltfile = dir.absoluteFilePath(mltfile);
        }
        // Filter several clips, destination points to a folder
        if (QFile::exists(mltfile)) {
            KIO::RenameDialog renameDialog(qApp->activeWindow(), i18n("File already exists"), QUrl::fromLocalFile(mltfile), QUrl::fromLocalFile(mltfile), KIO::RenameDialog_Option::RenameDialog_Overwrite );
            if (renameDialog.exec() != QDialog::Rejected) {
                QUrl final = renameDialog.newDestUrl();
                if (final.isValid()) {
                    mltfile = final.toLocalFile();
                }
            } else {
                return -1;
            }
        }
        destinations[binId] = mltfile;
    }
    // Now we have to create the jobs objects. This is trickier than usual, since the parameters are different for each job (each clip has its own
    // destination). We have to construct a lambda that does that.

    auto createFn = [dest = std::move(destinations), fSpeed = speed / 100.0, warp_pitch](const QString &id) { return std::make_shared<SpeedJob>(id, fSpeed, warp_pitch, dest.at(id)); };

    // We are now all set to create the job. Note that we pass all the parameters directly through the lambda, hence there are no extra parameters to the
    // function
    using local_createFn_t = std::function<std::shared_ptr<SpeedJob>(const QString &)>;
    return emit ptr->startJob<SpeedJob>(binIds, parentId, std::move(undoString), local_createFn_t(std::move(createFn)));
}

bool SpeedJob::commitResult(Fun &undo, Fun &redo)
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
    const QString folderName(i18n("Speed Change"));

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
    QStringList ids = pCore->projectItemModel()->getClipByUrl(QFileInfo(m_destUrl));
    if (!ids.isEmpty()) {
        // CLip was already inserted in bin, will be reloaded automatically, don't add twice
        return true;
    }
    auto id = ClipCreator::createClipFromFile(m_destUrl, folderId, pCore->projectItemModel(), undo, redo);
    return id != QStringLiteral("-1");
}
