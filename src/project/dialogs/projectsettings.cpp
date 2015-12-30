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
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "utils/KoIconUtils.h"
#include "titler/titlewidget.h"
#include "utils/KoIconUtils.h"
#include "effectslist/effectslist.h"
#include "dialogs/profilesdialog.h"
#include "dialogs/encodingprofilesdialog.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/bincontroller.h"

#include <KMessageBox>
#include <QDebug>
#include <kio/directorysizejob.h>
#include <klocalizedstring.h>
#include <KIO/FileCopyJob>

#include <QTemporaryFile>
#include <QDir>
#include <kmessagebox.h>
#include <QFileDialog>

ProjectSettings::ProjectSettings(KdenliveDoc *doc, QMap <QString, QString> metadata, const QStringList &lumas, int videotracks, int audiotracks, const QString &projectPath, bool readOnlyTracks, bool savedProject, QWidget * parent) :
    QDialog(parent), m_savedProject(savedProject), m_lumas(lumas)
{
    setupUi(this);

    list_search->setTreeWidget(files_list);
    project_folder->setMode(KFile::Directory);
    project_folder->setUrl(QUrl(projectPath));
    loadProfiles();

    manage_profiles->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    manage_profiles->setToolTip(i18n("Manage project profiles"));
    connect(manage_profiles, SIGNAL(clicked(bool)), this, SLOT(slotEditProfiles()));

    m_buttonOk = buttonBox->button(QDialogButtonBox::Ok);
    //buttonOk->setEnabled(false);
    audio_thumbs->setChecked(KdenliveSettings::audiothumbnails());
    video_thumbs->setChecked(KdenliveSettings::videothumbnails());
    audio_tracks->setValue(audiotracks);
    video_tracks->setValue(videotracks);
    connect(generate_proxy, SIGNAL(toggled(bool)), proxy_minsize, SLOT(setEnabled(bool)));
    connect(generate_imageproxy, SIGNAL(toggled(bool)), proxy_imageminsize, SLOT(setEnabled(bool)));

    if (doc) {
        enable_proxy->setChecked(doc->getDocumentProperty(QStringLiteral("enableproxy")).toInt());
        generate_proxy->setChecked(doc->getDocumentProperty(QStringLiteral("generateproxy")).toInt());
        proxy_minsize->setValue(doc->getDocumentProperty(QStringLiteral("proxyminsize")).toInt());
        m_proxyparameters = doc->getDocumentProperty(QStringLiteral("proxyparams"));
        generate_imageproxy->setChecked(doc->getDocumentProperty(QStringLiteral("generateimageproxy")).toInt());
        proxy_imageminsize->setValue(doc->getDocumentProperty(QStringLiteral("proxyimageminsize")).toInt());
        m_proxyextension = doc->getDocumentProperty(QStringLiteral("proxyextension"));
    }
    else {
        enable_proxy->setChecked(KdenliveSettings::enableproxy());
        generate_proxy->setChecked(KdenliveSettings::generateproxy());
        proxy_minsize->setValue(KdenliveSettings::proxyminsize());
        m_proxyparameters = KdenliveSettings::proxyparams();
        generate_imageproxy->setChecked(KdenliveSettings::generateimageproxy());
        proxy_imageminsize->setValue(KdenliveSettings::proxyimageminsize());
        m_proxyextension = KdenliveSettings::proxyextension();
    }

    proxy_minsize->setEnabled(generate_proxy->isChecked());
    proxy_imageminsize->setEnabled(generate_imageproxy->isChecked());


    loadProxyProfiles();

    // Proxy GUI stuff
    proxy_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    proxy_showprofileinfo->setToolTip(i18n("Show default profile parameters"));
    proxy_manageprofile->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    proxy_manageprofile->setToolTip(i18n("Manage proxy profiles"));

    connect(proxy_manageprofile, SIGNAL(clicked(bool)), this, SLOT(slotManageEncodingProfile()));
    proxy_profile->setToolTip(i18n("Select default proxy profile"));

    connect(proxy_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateProxyParams()));
    proxyparams->setVisible(false);
    proxyparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    connect(proxy_showprofileinfo, SIGNAL(clicked(bool)), proxyparams, SLOT(setVisible(bool)));

    if (readOnlyTracks) {
        video_tracks->setEnabled(false);
        audio_tracks->setEnabled(false);
    }

    // Metadata list
    QTreeWidgetItem *item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Title"));
    item->setData(0, Qt::UserRole, QString("meta.attr.title.markup"));
    if (metadata.contains("meta.attr.title.markup")) {
	item->setText(1, metadata.value("meta.attr.title.markup"));
	metadata.remove("meta.attr.title.markup");
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Author"));
    item->setData(0, Qt::UserRole, QString("meta.attr.author.markup"));
    if (metadata.contains("meta.attr.author.markup")) {
	item->setText(1, metadata.value("meta.attr.author.markup"));
	metadata.remove("meta.attr.author.markup");
    }
    else if (metadata.contains("meta.attr.artist.markup")) {
	item->setText(0, i18n("Artist"));
	item->setData(0, Qt::UserRole, QString("meta.attr.artist.markup"));
	item->setText(1, metadata.value("meta.attr.artist.markup"));
	metadata.remove("meta.attr.artist.markup");
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Copyright"));
    item->setData(0, Qt::UserRole, QString("meta.attr.copyright.markup"));
    if (metadata.contains("meta.attr.copyright.markup")) {
	item->setText(1, metadata.value("meta.attr.copyright.markup"));
	metadata.remove("meta.attr.copyright.markup");
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Year"));
    item->setData(0, Qt::UserRole, QString("meta.attr.year.markup"));
    if (metadata.contains("meta.attr.year.markup")) {
	item->setText(1, metadata.value("meta.attr.year.markup"));
	metadata.remove("meta.attr.year.markup");
    }
    else if (metadata.contains("meta.attr.date.markup")) {
	item->setText(0, i18n("Date"));
	item->setData(0, Qt::UserRole, QString("meta.attr.date.markup"));
	item->setText(1, metadata.value("meta.attr.date.markup"));
	metadata.remove("meta.attr.date.markup");
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    
    QMap<QString, QString>::const_iterator meta = metadata.constBegin();
    while (meta != metadata.constEnd()) {
	item = new QTreeWidgetItem(metadata_list, QStringList() << meta.key().section('.', 2,2));
	item->setData(0, Qt::UserRole, meta.key());
	item->setText(1, meta.value());
	item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
	++meta;
    }
    
    connect(add_metadata, SIGNAL(clicked()), this, SLOT(slotAddMetadataField()));
    connect(delete_metadata, SIGNAL(clicked()), this, SLOT(slotDeleteMetadataField()));
    add_metadata->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
    delete_metadata->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-remove")));
    
    slotUpdateDisplay();
    if (doc != NULL) {
        slotUpdateFiles();
        connect(clear_cache, SIGNAL(clicked()), this, SLOT(slotClearCache()));
        connect(delete_unused, SIGNAL(clicked()), this, SLOT(slotDeleteUnused()));
        connect(delete_proxies, SIGNAL(clicked()), this, SLOT(slotDeleteProxies()));
    } else tabWidget->widget(1)->setEnabled(false);
    connect(profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(project_folder, SIGNAL(textChanged(QString)), this, SLOT(slotUpdateButton(QString)));
    connect(button_export, SIGNAL(clicked()), this, SLOT(slotExportToText()));
}

void ProjectSettings::slotDeleteUnused()
{
    QStringList toDelete;
    //TODO
    /*
    QList <DocClipBase*> list = m_projectList->documentClipList();
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip->numReferences() == 0 && clip->clipType() != SlideShow) {
            QUrl url = clip->fileURL();
            if (url.isValid() && !toDelete.contains(url.path())) toDelete << url.path();
        }
    }

    // make sure our urls are not used in another clip
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip->numReferences() > 0) {
            QUrl url = clip->fileURL();
            if (url.isValid() && toDelete.contains(url.path())) toDelete.removeAll(url.path());
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
    */
}

void ProjectSettings::slotClearCache()
{
    buttonBox->setEnabled(false);
    // Delete and recteate the thumbs directory
    QDir dir(project_folder->url().path());
    // Try to make sure we delete the correct directory
    if (dir.cd("thumbs") && dir.dirName() == "thumbs") {
        dir.removeRecursively();
        dir.setPath(project_folder->url().path());
        dir.mkdir("thumbs");
    }
    buttonBox->setEnabled(true);
    slotUpdateFiles(true);
}

void ProjectSettings::slotDeleteProxies()
{
    if (KMessageBox::warningContinueCancel(this, i18n("Deleting proxy clips will disable proxies for this project.")) != KMessageBox::Continue) return;
    buttonBox->setEnabled(false);
    enable_proxy->setChecked(false);
    emit disableProxies();
    // Delete and recteate the proxy directory
    QDir dir(project_folder->url().path() + QDir::separator() + "proxy/");
    // Try to make sure we delete the correct directory
    if (dir.exists() && dir.dirName() == "proxy") {
        dir.removeRecursively();
        dir.setPath(project_folder->url().path());
        dir.mkdir("proxy");
    }
    buttonBox->setEnabled(true);
    slotUpdateFiles(true);
}

void ProjectSettings::slotUpdateFiles(bool cacheOnly)
{
    QDir folder(project_folder->url().path());
    folder.cd("thumbs");
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(folder.absolutePath()));
    job->exec();
    thumbs_count->setText(QString::number(job->totalFiles()));
    thumbs_size->setText(KIO::convertSize(job->totalSize()));
    folder.cd("../proxy");
    job = KIO::directorySize(QUrl::fromLocalFile(folder.absolutePath()));
    job->exec();
    proxy_count->setText(QString::number(job->totalFiles()));
    proxy_size->setText(KIO::convertSize(job->totalSize()));
    delete job;
    if (cacheOnly) return;
    int unused = 0;
    int used = 0;
    KIO::filesize_t usedSize = 0;
    KIO::filesize_t unUsedSize = 0;
    QList <ClipController*> list = pCore->binController()->getControllerList();
    files_list->clear();

    // List all files that are used in the project. That also means:
    // images included in slideshow and titles, files in playlist clips
    // TODO: images used in luma transitions?

    // Setup categories
    QTreeWidgetItem *videos = new QTreeWidgetItem(files_list, QStringList() << i18n("Video clips"));
    videos->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("video-x-generic")));
    videos->setExpanded(true);
    QTreeWidgetItem *sounds = new QTreeWidgetItem(files_list, QStringList() << i18n("Audio clips"));
    sounds->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("audio-x-generic")));
    sounds->setExpanded(true);
    QTreeWidgetItem *images = new QTreeWidgetItem(files_list, QStringList() << i18n("Image clips"));
    images->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("image-x-generic")));
    images->setExpanded(true);
    QTreeWidgetItem *slideshows = new QTreeWidgetItem(files_list, QStringList() << i18n("Slideshow clips"));
    slideshows->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("image-x-generic")));
    slideshows->setExpanded(true);
    QTreeWidgetItem *texts = new QTreeWidgetItem(files_list, QStringList() << i18n("Text clips"));
    texts->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("text-plain")));
    texts->setExpanded(true);
    QTreeWidgetItem *playlists = new QTreeWidgetItem(files_list, QStringList() << i18n("Playlist clips"));
    playlists->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("video-mlt-playlist")));
    playlists->setExpanded(true);
    QTreeWidgetItem *others = new QTreeWidgetItem(files_list, QStringList() << i18n("Other clips"));
    others->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("unknown")));
    others->setExpanded(true);
    int count = 0;
    QStringList allFonts;
    foreach(const QString & file, m_lumas) {
        count++;
        new QTreeWidgetItem(images, QStringList() << file);
    }

    for (int i = 0; i < list.count(); ++i) {
        ClipController *clip = list.at(i);
        if (clip->clipType() == SlideShow) {
            QStringList subfiles = extractSlideshowUrls(clip->clipUrl());
            foreach(const QString & file, subfiles) {
                count++;
                new QTreeWidgetItem(slideshows, QStringList() << file);
            }
        } else if (!clip->clipUrl().isEmpty()) {
            //allFiles.append(clip->fileURL().path());
            switch (clip->clipType()) {
            case Text:
                new QTreeWidgetItem(texts, QStringList() << clip->clipUrl().path());
                break;
            case Audio:
                new QTreeWidgetItem(sounds, QStringList() << clip->clipUrl().path());
                break;
            case Image:
                new QTreeWidgetItem(images, QStringList() << clip->clipUrl().path());
                break;
            case Playlist:
                new QTreeWidgetItem(playlists, QStringList() << clip->clipUrl().path());
                break;
            case Unknown:
                new QTreeWidgetItem(others, QStringList() << clip->clipUrl().path());
                break;
            default:
                new QTreeWidgetItem(videos, QStringList() << clip->clipUrl().path());
                break;
            }
            count++;
        }
        if (clip->clipType() == Text) {
            QStringList imagefiles = TitleWidget::extractImageList(clip->property("xmldata"));
            QStringList fonts = TitleWidget::extractFontList(clip->property("xmldata"));
            foreach(const QString & file, imagefiles) {
                count++;
                new QTreeWidgetItem(images, QStringList() << file);
            }
            allFonts << fonts;
        } else if (clip->clipType() == Playlist) {
            QStringList files = extractPlaylistUrls(clip->clipUrl().path());
            foreach(const QString & file, files) {
                count++;
                new QTreeWidgetItem(others, QStringList() << file);
            }
        }

        //TODO
        if (false /*clip->numReferences() == 0*/) {
            unused++;
            //unUsedSize += clip->fileSize();
        } else {
            used++;
            //usedSize += clip->fileSize();
        }
    }
    allFonts.removeDuplicates();
    // Hide unused categories
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        if (files_list->topLevelItem(i)->childCount() == 0) {
            files_list->topLevelItem(i)->setHidden(true);
        }
    }
    files_count->setText(QString::number(count));
    fonts_list->addItems(allFonts);
    if (allFonts.isEmpty()) {
        fonts_list->setHidden(true);
        label_fonts->setHidden(true);
    }
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
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    QString currentProfile = profiles_list->itemData(profiles_list->currentIndex()).toString();
    QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(currentProfile);
    p_size->setText(values.value("width") + 'x' + values.value("height"));
    p_fps->setText(values.value("frame_rate_num") + '/' + values.value("frame_rate_den"));
    p_aspect->setText(values.value("sample_aspect_num") + '/' + values.value("sample_aspect_den"));
    p_display->setText(values.value("display_aspect_num") + '/' + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0) {
        p_progressive->setText(i18n("Interlaced (%1 fields per second)",
                                    locale.toString((double)2 * values.value("frame_rate_num").toInt() / values.value("frame_rate_den").toInt(), 'f', 2)));
    } else {
        p_progressive->setText(i18n("Progressive"));
    }
    p_colorspace->setText(ProfilesDialog::getColorspaceDescription(values.value("colorspace").toInt()));
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

QUrl ProjectSettings::selectedFolder() const
{
    return project_folder->url();
}

QPoint ProjectSettings::tracks() const
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

bool ProjectSettings::useProxy() const
{
    return enable_proxy->isChecked();
}

bool ProjectSettings::generateProxy() const
{
    return generate_proxy->isChecked();
}

bool ProjectSettings::generateImageProxy() const
{
    return generate_imageproxy->isChecked();
}

int ProjectSettings::proxyMinSize() const
{
    return proxy_minsize->value();
}

int ProjectSettings::proxyImageMinSize() const
{
    return proxy_imageminsize->value();
}

QString ProjectSettings::proxyParams() const
{
    QString params = proxy_profile->itemData(proxy_profile->currentIndex()).toString();
    return params.section(';', 0, 0);
}

QString ProjectSettings::proxyExtension() const
{
    QString params = proxy_profile->itemData(proxy_profile->currentIndex()).toString();
    return params.section(';', 1, 1);
}

//static
QStringList ProjectSettings::extractPlaylistUrls(const QString &path)
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
    for (int i = 0; i < files.count(); ++i) {
        QDomElement e = files.at(i).toElement();
        QString type = EffectsList::property(e, "mlt_service");
        if (type != "colour") {
            QString url = EffectsList::property(e, "resource");
            if (type == "framebuffer") {
                url = url.section('?', 0, 0);
            }
            if (!url.isEmpty()) {
                if (!url.startsWith('/')) url.prepend(root);
                if (url.section('.', 0, -2).endsWith(QLatin1String("/.all"))) {
                    // slideshow clip, extract image urls
                    urls << extractSlideshowUrls(QUrl(url));
                } else urls << url;
                if (url.endsWith(QLatin1String(".mlt")) || url.endsWith(QLatin1String(".kdenlive"))) {
                    //TODO: Do something to avoid infinite loops if 2 files reference themselves...
                    urls << extractPlaylistUrls(url);
                }
            }
        }
    }

    // luma files for transitions
    files = doc.elementsByTagName("transition");
    for (int i = 0; i < files.count(); ++i) {
        QDomElement e = files.at(i).toElement();
        QString url = EffectsList::property(e, "resource");
        if (!url.isEmpty()) {
            if (!url.startsWith('/')) url.prepend(root);
            urls << url;
        }
    }

    return urls;
}


//static
QStringList ProjectSettings::extractSlideshowUrls(const QUrl &url)
{
    QStringList urls;
    QString path = url.adjusted(QUrl::RemoveFilename).path();
    QString ext = url.path().section('.', -1);
    QDir dir(path);
    if (url.path().contains(".all.")) {
        // this is a mime slideshow, like *.jpeg
        QStringList filters;
        filters << "*." + ext;
        dir.setNameFilters(filters);
        QStringList result = dir.entryList(QDir::Files);
        urls.append(path + filters.at(0) + " (" + i18np("1 image found", "%1 images found", result.count()) + ')');
    } else {
        // this is a pattern slideshow, like sequence%4d.jpg
        QString filter = url.fileName();
        QString ext = filter.section('.', -1);
        filter = filter.section('%', 0, -2);
        QString regexp = '^' + filter + "\\d+\\." + ext + '$';
        QRegExp rx(regexp);
        int count = 0;
        QStringList result = dir.entryList(QDir::Files);
        foreach(const QString & path, result) {
            if (rx.exactMatch(path)) count++;
        }
        urls.append(url.path() + " (" + i18np("1 image found", "%1 images found", count) + ')');
    }
    return urls;
}

void ProjectSettings::slotExportToText()
{
    QString savePath = QFileDialog::getSaveFileName(this, QString(), project_folder->url().path(), "text/plain");
    if (savePath.isEmpty()) return;
    QString data;
    data.append(i18n("Project folder: %1",  project_folder->url().path()) + '\n');
    data.append(i18n("Project profile: %1",  profiles_list->currentText()) + '\n');
    data.append(i18n("Total clips: %1 (%2 used in timeline).", files_count->text(), used_count->text()) + "\n\n");
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        if (files_list->topLevelItem(i)->childCount() > 0) {
            data.append('\n' + files_list->topLevelItem(i)->text(0) + ":\n\n");
            for (int j = 0; j < files_list->topLevelItem(i)->childCount(); ++j) {
                data.append(files_list->topLevelItem(i)->child(j)->text(0) + '\n');
            }
        }
    }
    QTemporaryFile tmpfile;
    if (!tmpfile.open()) {
        qWarning() << "/////  CANNOT CREATE TMP FILE in: " << tmpfile.fileName();
        return;
    }
    QFile xmlf(tmpfile.fileName());
    if (!xmlf.open(QIODevice::WriteOnly))
        return;
    xmlf.write(data.toUtf8());
    if (xmlf.error() != QFile::NoError) {
        xmlf.close();
        return;
    }
    xmlf.close();
    KIO::FileCopyJob *copyjob = KIO::file_copy(QUrl::fromLocalFile(tmpfile.fileName()), QUrl::fromLocalFile(savePath));
    copyjob->exec();
}

void ProjectSettings::slotUpdateProxyParams()
{
    QString params = proxy_profile->itemData(proxy_profile->currentIndex()).toString();
    proxyparams->setPlainText(params.section(';', 0, 0));
}

const QMap <QString, QString> ProjectSettings::metadata() const
{
    QMap <QString, QString> metadata;
    for (int i = 0; i < metadata_list->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = metadata_list->topLevelItem(i);
        if (!item->text(1).simplified().isEmpty()) {
            // Insert metadata entry
            QString key = item->data(0, Qt::UserRole).toString();
	    if (key.isEmpty()) key = "meta.attr." + item->text(0).simplified() + ".markup";
            QString value = item->text(1);
            metadata.insert(key, value);
        }
    }
    return metadata;
}

void ProjectSettings::slotAddMetadataField()
{
    QTreeWidgetItem *item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("field_name"));
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void ProjectSettings::slotDeleteMetadataField()
{
    QTreeWidgetItem *item = metadata_list->currentItem();
    if (item) delete item;
}

void ProjectSettings::loadProfiles()
{
    profiles_list->clear();
    QMap <QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMapIterator<QString, QString> i(profilesInfo);
    while (i.hasNext()) {
        i.next();
        profiles_list->addItem(i.key(), i.value());
    }

    QString currentProf = KdenliveSettings::current_profile();

    for (int i = 0; i < profiles_list->count(); ++i) {
        if (profiles_list->itemData(i).toString() == currentProf) {
            profiles_list->setCurrentIndex(i);
            break;
        }
    }
}

void ProjectSettings::slotEditProfiles()
{
    ProfilesDialog *w = new ProfilesDialog;
    w->exec();
    loadProfiles();
    emit refreshProfiles();
    delete w;
}

void ProjectSettings::slotManageEncodingProfile()
{
    QPointer<EncodingProfilesDialog> d = new EncodingProfilesDialog(0);
    d->exec();
    delete d;
    loadProxyProfiles();
}

void ProjectSettings::loadProxyProfiles()
{
   // load proxy profiles
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::DataLocation);
    KConfigGroup group(&conf, "proxy");
    QMap <QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> k(values);
    int ix = -1;
    proxy_profile->clear();
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            QString params = k.value().section(';', 0, 0);
            QString extension = k.value().section(';', 1, 1);
            if (ix == -1 && ((params == m_proxyparameters && extension == m_proxyextension) || (m_proxyparameters.isEmpty() || m_proxyextension.isEmpty()))) {
                // this is the current profile
                ix = proxy_profile->count();
            }
            proxy_profile->addItem(k.key(), k.value());
        }
    }
    if (ix == -1) {
        // Current project proxy settings not found
        ix = proxy_profile->count();
        proxy_profile->addItem(i18n("Current Settings"), QString(m_proxyparameters + ';' + m_proxyextension));
    }
    proxy_profile->setCurrentIndex(ix);
    slotUpdateProxyParams();
}

