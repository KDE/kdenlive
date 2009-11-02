/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "projectsettings.h"
#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "docclipbase.h"
#include "titlewidget.h"
#include "effectslist.h"

#include <KStandardDirs>
#include <KMessageBox>
#include <KDebug>
#include <kio/directorysizejob.h>
#include <KIO/NetAccess>

#include <QDir>
#include <kmessagebox.h>

ProjectSettings::ProjectSettings(ProjectList *projectlist, QStringList lumas, int videotracks, int audiotracks, const QString projectPath, bool readOnlyTracks, bool savedProject, QWidget * parent) :
        QDialog(parent), m_savedProject(savedProject), m_projectList(projectlist), m_lumas(lumas)
{
    setupUi(this);

    list_search->setListWidget(files_list);

    QMap <QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMapIterator<QString, QString> i(profilesInfo);
    while (i.hasNext()) {
        i.next();
        profiles_list->addItem(i.key(), i.value());
    }
    project_folder->setMode(KFile::Directory);
    project_folder->setUrl(KUrl(projectPath));
    QString currentProf = KdenliveSettings::current_profile();

    for (int i = 0; i < profiles_list->count(); i++) {
        if (profiles_list->itemData(i).toString() == currentProf) {
            profiles_list->setCurrentIndex(i);
            break;
        }
    }

    m_buttonOk = buttonBox->button(QDialogButtonBox::Ok);
    //buttonOk->setEnabled(false);
    audio_thumbs->setChecked(KdenliveSettings::audiothumbnails());
    video_thumbs->setChecked(KdenliveSettings::videothumbnails());
    audio_tracks->setValue(audiotracks);
    video_tracks->setValue(videotracks);
    if (readOnlyTracks) {
        video_tracks->setEnabled(false);
        audio_tracks->setEnabled(false);
    }
    slotUpdateDisplay();
    if (m_projectList != NULL) {
        slotUpdateFiles();
        connect(clear_cache, SIGNAL(clicked()), this, SLOT(slotClearCache()));
        connect(delete_unused, SIGNAL(clicked()), this, SLOT(slotDeleteUnused()));
    } else tabWidget->widget(1)->setEnabled(false);
    connect(profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(project_folder, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButton(const QString &)));
}

void ProjectSettings::slotDeleteUnused()
{
    QStringList toDelete;
    QList <DocClipBase*> list = m_projectList->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        if (clip->numReferences() == 0 && clip->clipType() != SLIDESHOW) {
            KUrl url = clip->fileURL();
            if (!url.isEmpty() && !toDelete.contains(url.path())) toDelete << url.path();
        }
    }

    // make sure our urls are not used in another clip
    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        if (clip->numReferences() > 0) {
            KUrl url = clip->fileURL();
            if (!url.isEmpty() && toDelete.contains(url.path())) toDelete.removeAll(url.path());
        }
    }

    if (toDelete.count() == 0) {
        // No physical url to delete, we only remove unused clips from project (color clips for example have no physical url)
        if (KMessageBox::warningContinueCancel(this, i18n("This will remove all unused clips from your project."), i18n("Clean up project")) == KMessageBox::Cancel) return;
        m_projectList->cleanup();
        slotUpdateFiles();
        return;
    }
    if (KMessageBox::warningYesNoList(this, i18n("This will remove the following files from your hard drive.\nThis action cannot be undone, only use if you know what you are doing.\nAre you sure you want to continue?"), toDelete, i18n("Delete unused clips")) != KMessageBox::Yes) return;
    m_projectList->trashUnusedClips();
    slotUpdateFiles();
}

void ProjectSettings::slotClearCache()
{
    buttonBox->setEnabled(false);
    KIO::NetAccess::del(KUrl(project_folder->url().path(KUrl::AddTrailingSlash) + "thumbs/"), this);
    KStandardDirs::makeDir(project_folder->url().path(KUrl::AddTrailingSlash) + "thumbs/");
    buttonBox->setEnabled(true);
    slotUpdateFiles(true);
}

void ProjectSettings::slotUpdateFiles(bool cacheOnly)
{
    KIO::DirectorySizeJob * job = KIO::directorySize(project_folder->url().path(KUrl::AddTrailingSlash) + "thumbs/");
    job->exec();
    thumbs_count->setText(QString::number(job->totalFiles()));
    thumbs_size->setText(KIO::convertSize(job->totalSize()));
    delete job;
    if (cacheOnly) return;
    int unused = 0;
    int used = 0;
    KIO::filesize_t usedSize = 0;
    KIO::filesize_t unUsedSize = 0;
    QList <DocClipBase*> list = m_projectList->documentClipList();
    files_list->clear();

    // List all files that are used in the project. That also means:
    // images included in slideshow and titles, files in playlist clips
    // TODO: images used in luma transitions, files used for LADSPA effects?

    QStringList allFiles;
    allFiles << m_lumas;
    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        if (clip->clipType() == SLIDESHOW) {
            // special case, list all images
            QStringList files = extractSlideshowUrls(clip->fileURL());
            allFiles << files;
        } else if (!clip->fileURL().isEmpty()) allFiles.append(clip->fileURL().path());
        if (clip->clipType() == TEXT) {
            QStringList images = TitleWidget::extractImageList(clip->getProperty("xmldata"));
            allFiles << images;
        } else if (clip->clipType() == PLAYLIST) {
            QStringList files = extractPlaylistUrls(clip->fileURL().path());
            allFiles << files;
        }

        if (clip->numReferences() == 0) {
            unused++;
            unUsedSize += clip->fileSize();
        } else {
            used++;
            usedSize += clip->fileSize();
        }
    }
#if QT_VERSION >= 0x040500
    allFiles.removeDuplicates();
#endif
    files_count->setText(QString::number(allFiles.count()));
    files_list->addItems(allFiles);
    used_count->setText(QString::number(used));
    used_size->setText(KIO::convertSize(usedSize));
    unused_count->setText(QString::number(unused));
    unused_size->setText(KIO::convertSize(unUsedSize));
    delete_unused->setEnabled(unused > 0);
}

void ProjectSettings::accept()
{
    if (!m_savedProject && selectedProfile() != KdenliveSettings::current_profile())
        if (KMessageBox::warningContinueCancel(this, i18n("Changing the profile of your project cannot be undone.\nIt is recommended to save your project before attempting this operation that might cause some corruption in transitions.\n Are you sure you want to proceed?"), i18n("Confirm profile change")) == KMessageBox::Cancel) return;
    QDialog::accept();
}

void ProjectSettings::slotUpdateDisplay()
{
    QString currentProfile = profiles_list->itemData(profiles_list->currentIndex()).toString();
    QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(currentProfile);
    p_size->setText(values.value("width") + 'x' + values.value("height"));
    p_fps->setText(values.value("frame_rate_num") + '/' + values.value("frame_rate_den"));
    p_aspect->setText(values.value("sample_aspect_num") + '/' + values.value("sample_aspect_den"));
    p_display->setText(values.value("display_aspect_num") + '/' + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0) p_progressive->setText(i18n("Interlaced"));
    else p_progressive->setText(i18n("Progressive"));
}

void ProjectSettings::slotUpdateButton(const QString &path)
{
    if (path.isEmpty()) m_buttonOk->setEnabled(false);
    else {
        m_buttonOk->setEnabled(true);
        slotUpdateFiles(true);
    }
}

QString ProjectSettings::selectedProfile() const
{
    return profiles_list->itemData(profiles_list->currentIndex()).toString();
}

KUrl ProjectSettings::selectedFolder() const
{
    return project_folder->url();
}

QPoint ProjectSettings::tracks()
{
    QPoint p;
    p.setX(video_tracks->value());
    p.setY(audio_tracks->value());
    return p;
}

bool ProjectSettings::enableVideoThumbs() const
{
    return video_thumbs->isChecked();
}

bool ProjectSettings::enableAudioThumbs() const
{
    return audio_thumbs->isChecked();
}

//static
QStringList ProjectSettings::extractPlaylistUrls(QString path)
{
    QStringList urls;
    QDomDocument doc;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return urls;
    if (!doc.setContent(&file)) {
        file.close();
        return urls;
    }
    file.close();
    QString root = doc.documentElement().attribute("root");
    if (!root.isEmpty() && !root.endsWith('/')) root.append('/');
    QDomNodeList files = doc.elementsByTagName("producer");
    for (int i = 0; i < files.count(); i++) {
        QDomElement e = files.at(i).toElement();
        QString type = EffectsList::property(e, "mlt_service");
        if (type != "colour") {
            QString url = EffectsList::property(e, "resource");
            if (!url.isEmpty()) {
                if (!url.startsWith('/')) url.prepend(root);
                if (url.section('.', 0, -2).endsWith("/.all")) {
                    // slideshow clip, extract image urls
                    urls << extractSlideshowUrls(KUrl(url));
                } else urls << url;
                if (url.endsWith(".mlt") || url.endsWith(".kdenlive")) {
                    //TODO: Do something to avoid infinite loops if 2 files reference themselves...
                    urls << extractPlaylistUrls(url);
                }
            }
        }
    }

    // luma files for transitions
    files = doc.elementsByTagName("transition");
    for (int i = 0; i < files.count(); i++) {
        QDomElement e = files.at(i).toElement();
        QString url = EffectsList::property(e, "luma");
        if (!url.isEmpty()) {
            if (!url.startsWith('/')) url.prepend(root);
            urls << url;
        }
    }

    return urls;
}


//static
QStringList ProjectSettings::extractSlideshowUrls(KUrl url)
{
    QStringList urls;
    QString path = url.directory(KUrl::AppendTrailingSlash);
    QString ext = url.path().section('.', -1);
    QDir dir(path);
    QStringList filters;
    filters << "*." + ext;
    dir.setNameFilters(filters);
    QStringList result = dir.entryList(QDir::Files);
    for (int j = 0; j < result.count(); j++) {
        urls.append(path + result.at(j));
    }
    return urls;
}

#include "projectsettings.moc"


