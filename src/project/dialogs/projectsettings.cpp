/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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
#include "effectslist/effectslist.h"
#include "dialogs/profilesdialog.h"
#include "dialogs/encodingprofilesdialog.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/bincontroller.h"
#include "project/dialogs/temporarydata.h"
#include "project/dialogs/profilewidget.h"
#include "bin/bin.h"

#include <KMessageBox>
#include "kdenlive_debug.h"
#include <kio/directorysizejob.h>
#include <klocalizedstring.h>
#include <KIO/FileCopyJob>

#include <QTemporaryFile>
#include <QDir>
#include <kmessagebox.h>
#include <QFileDialog>
#include <QInputDialog>

class NoEditDelegate: public QStyledItemDelegate
{
public:
    NoEditDelegate(QObject *parent = nullptr): QStyledItemDelegate(parent) {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE
    {
        Q_UNUSED(parent);
        Q_UNUSED(option);
        Q_UNUSED(index);
        return nullptr;
    }
};

ProjectSettings::ProjectSettings(KdenliveDoc *doc, QMap<QString, QString> metadata, const QStringList &lumas, int videotracks, int audiotracks, const QString &/*projectPath*/, bool readOnlyTracks, bool savedProject, QWidget *parent) :
    QDialog(parent)
    , m_savedProject(savedProject)
    , m_lumas(lumas)
{
    setupUi(this);
    tabWidget->setTabBarAutoHide(true);
    QVBoxLayout *vbox = new QVBoxLayout;
    m_pw = new ProfileWidget(this);
    vbox->addWidget(m_pw);
    profile_box->setLayout(vbox);
    profile_box->setTitle(i18n("Select the profile (preset) of the project"));

    list_search->setTreeWidget(files_list);
    project_folder->setMode(KFile::Directory);

    m_buttonOk = buttonBox->button(QDialogButtonBox::Ok);
    //buttonOk->setEnabled(false);
    audio_thumbs->setChecked(KdenliveSettings::audiothumbnails());
    video_thumbs->setChecked(KdenliveSettings::videothumbnails());
    audio_tracks->setValue(audiotracks);
    video_tracks->setValue(videotracks);
    connect(generate_proxy, &QAbstractButton::toggled, proxy_minsize, &QWidget::setEnabled);
    connect(generate_imageproxy, &QAbstractButton::toggled, proxy_imageminsize, &QWidget::setEnabled);

    QString currentProf;
    if (doc) {
        currentProf = KdenliveSettings::current_profile();
        enable_proxy->setChecked(doc->getDocumentProperty(QStringLiteral("enableproxy")).toInt());
        generate_proxy->setChecked(doc->getDocumentProperty(QStringLiteral("generateproxy")).toInt());
        proxy_minsize->setValue(doc->getDocumentProperty(QStringLiteral("proxyminsize")).toInt());
        m_proxyparameters = doc->getDocumentProperty(QStringLiteral("proxyparams"));
        generate_imageproxy->setChecked(doc->getDocumentProperty(QStringLiteral("generateimageproxy")).toInt());
        proxy_imageminsize->setValue(doc->getDocumentProperty(QStringLiteral("proxyimageminsize")).toInt());
        m_proxyextension = doc->getDocumentProperty(QStringLiteral("proxyextension"));
        m_previewparams = doc->getDocumentProperty(QStringLiteral("previewparameters"));
        m_previewextension = doc->getDocumentProperty(QStringLiteral("previewextension"));
        QString storageFolder = doc->getDocumentProperty(QStringLiteral("storagefolder"));
        if (!storageFolder.isEmpty()) {
            custom_folder->setChecked(true);
        }
        project_folder->setUrl(QUrl::fromLocalFile(doc->projectTempFolder()));
        TemporaryData *cacheWidget = new TemporaryData(doc, true, this);
        connect(cacheWidget, &TemporaryData::disableProxies, this, &ProjectSettings::disableProxies);
        connect(cacheWidget, &TemporaryData::disablePreview, this, &ProjectSettings::disablePreview);
        tabWidget->addTab(cacheWidget, i18n("Cache Data"));
    } else {
        currentProf = KdenliveSettings::default_profile();
        enable_proxy->setChecked(KdenliveSettings::enableproxy());
        generate_proxy->setChecked(KdenliveSettings::generateproxy());
        proxy_minsize->setValue(KdenliveSettings::proxyminsize());
        m_proxyparameters = KdenliveSettings::proxyparams();
        generate_imageproxy->setChecked(KdenliveSettings::generateimageproxy());
        proxy_imageminsize->setValue(KdenliveSettings::proxyimageminsize());
        m_proxyextension = KdenliveSettings::proxyextension();
        m_previewparams = KdenliveSettings::previewparams();
        m_previewextension = KdenliveSettings::previewextension();
        custom_folder->setChecked(KdenliveSettings::customprojectfolder());
        project_folder->setUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    }

    // Select profile
    m_pw->loadProfile(currentProf);

    proxy_minsize->setEnabled(generate_proxy->isChecked());
    proxy_imageminsize->setEnabled(generate_imageproxy->isChecked());

    loadProxyProfiles();
    loadPreviewProfiles();

    // Proxy GUI stuff
    proxy_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    proxy_showprofileinfo->setToolTip(i18n("Show default profile parameters"));
    proxy_manageprofile->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    proxy_manageprofile->setToolTip(i18n("Manage proxy profiles"));

    connect(proxy_manageprofile, &QAbstractButton::clicked, this, &ProjectSettings::slotManageEncodingProfile);
    proxy_profile->setToolTip(i18n("Select default proxy profile"));

    connect(proxy_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateProxyParams()));
    proxyparams->setVisible(false);
    proxyparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    connect(proxy_showprofileinfo, &QAbstractButton::clicked, proxyparams, &QWidget::setVisible);

    // Preview GUI stuff
    preview_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    preview_showprofileinfo->setToolTip(i18n("Show default profile parameters"));
    preview_manageprofile->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    preview_manageprofile->setToolTip(i18n("Manage timeline preview profiles"));

    connect(preview_manageprofile, &QAbstractButton::clicked, this, &ProjectSettings::slotManagePreviewProfile);
    preview_profile->setToolTip(i18n("Select default preview profile"));

    connect(preview_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatePreviewParams()));
    previewparams->setVisible(false);
    previewparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    connect(preview_showprofileinfo, &QAbstractButton::clicked, previewparams, &QWidget::setVisible);

    if (readOnlyTracks) {
        video_tracks->setEnabled(false);
        audio_tracks->setEnabled(false);
    }

    metadata_list->setItemDelegateForColumn(0, new NoEditDelegate(this));
    connect(metadata_list, &QTreeWidget::itemDoubleClicked, this, &ProjectSettings::slotEditMetadata);

    // Metadata list
    QTreeWidgetItem *item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Title"));
    item->setData(0, Qt::UserRole, QStringLiteral("meta.attr.title.markup"));
    if (metadata.contains(QStringLiteral("meta.attr.title.markup"))) {
        item->setText(1, metadata.value(QStringLiteral("meta.attr.title.markup")));
        metadata.remove(QStringLiteral("meta.attr.title.markup"));
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Author"));
    item->setData(0, Qt::UserRole, QStringLiteral("meta.attr.author.markup"));
    if (metadata.contains(QStringLiteral("meta.attr.author.markup"))) {
        item->setText(1, metadata.value(QStringLiteral("meta.attr.author.markup")));
        metadata.remove(QStringLiteral("meta.attr.author.markup"));
    } else if (metadata.contains(QStringLiteral("meta.attr.artist.markup"))) {
        item->setText(0, i18n("Artist"));
        item->setData(0, Qt::UserRole, QStringLiteral("meta.attr.artist.markup"));
        item->setText(1, metadata.value(QStringLiteral("meta.attr.artist.markup")));
        metadata.remove(QStringLiteral("meta.attr.artist.markup"));
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Copyright"));
    item->setData(0, Qt::UserRole, QStringLiteral("meta.attr.copyright.markup"));
    if (metadata.contains(QStringLiteral("meta.attr.copyright.markup"))) {
        item->setText(1, metadata.value(QStringLiteral("meta.attr.copyright.markup")));
        metadata.remove(QStringLiteral("meta.attr.copyright.markup"));
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item = new QTreeWidgetItem(metadata_list, QStringList() << i18n("Year"));
    item->setData(0, Qt::UserRole, QStringLiteral("meta.attr.year.markup"));
    if (metadata.contains(QStringLiteral("meta.attr.year.markup"))) {
        item->setText(1, metadata.value(QStringLiteral("meta.attr.year.markup")));
        metadata.remove(QStringLiteral("meta.attr.year.markup"));
    } else if (metadata.contains(QStringLiteral("meta.attr.date.markup"))) {
        item->setText(0, i18n("Date"));
        item->setData(0, Qt::UserRole, QStringLiteral("meta.attr.date.markup"));
        item->setText(1, metadata.value(QStringLiteral("meta.attr.date.markup")));
        metadata.remove(QStringLiteral("meta.attr.date.markup"));
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QMap<QString, QString>::const_iterator meta = metadata.constBegin();
    while (meta != metadata.constEnd()) {
        item = new QTreeWidgetItem(metadata_list, QStringList() << meta.key().section(QLatin1Char('.'), 2, 2));
        item->setData(0, Qt::UserRole, meta.key());
        item->setText(1, meta.value());
        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        ++meta;
    }

    connect(add_metadata, &QAbstractButton::clicked, this, &ProjectSettings::slotAddMetadataField);
    connect(delete_metadata, &QAbstractButton::clicked, this, &ProjectSettings::slotDeleteMetadataField);
    add_metadata->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
    delete_metadata->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-remove")));

    if (doc != nullptr) {
        slotUpdateFiles();
        connect(delete_unused, &QAbstractButton::clicked, this, &ProjectSettings::slotDeleteUnused);
    } else {
        tabWidget->removeTab(2);
        tabWidget->removeTab(1);
    }
    connect(project_folder, &KUrlRequester::textChanged, this, &ProjectSettings::slotUpdateButton);
    connect(button_export, &QAbstractButton::clicked, this, &ProjectSettings::slotExportToText);
    // Delete unused files is not implemented
    delete_unused->setVisible(false);
}

void ProjectSettings::slotEditMetadata(QTreeWidgetItem *item, int)
{
    metadata_list->editItem(item, 1);
}

void ProjectSettings::slotDeleteUnused()
{
    QStringList toDelete;
    //TODO
    /*
    QList<DocClipBase*> list = m_projectList->documentClipList();
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

void ProjectSettings::slotUpdateFiles(bool cacheOnly)
{
    // Get list of current project hashes
    QStringList hashes = pCore->binController()->getProjectHashes();
    m_projectProxies.clear();
    m_projectThumbs.clear();
    if (cacheOnly) {
        return;
    }
    QList<ClipController *> list = pCore->binController()->getControllerList();
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
    foreach (const QString &file, m_lumas) {
        count++;
        new QTreeWidgetItem(images, QStringList() << file);
    }

    for (int i = 0; i < list.count(); ++i) {
        ClipController *clip = list.at(i);
        if (clip->clipType() == Color) {
            // ignore color clips in list, there is no real file
            continue;
        }
        if (clip->clipType() == SlideShow) {
            const QStringList subfiles = extractSlideshowUrls(clip->clipUrl());
            for (const QString &file : subfiles) {
                count++;
                new QTreeWidgetItem(slideshows, QStringList() << file);
            }
            continue;
        } else if (!clip->clipUrl().isEmpty()) {
            //allFiles.append(clip->fileURL().path());
            switch (clip->clipType()) {
            case Text:
                new QTreeWidgetItem(texts, QStringList() << clip->clipUrl());
                break;
            case Audio:
                new QTreeWidgetItem(sounds, QStringList() << clip->clipUrl());
                break;
            case Image:
                new QTreeWidgetItem(images, QStringList() << clip->clipUrl());
                break;
            case Playlist:
                new QTreeWidgetItem(playlists, QStringList() << clip->clipUrl());
                break;
            case Unknown:
                new QTreeWidgetItem(others, QStringList() << clip->clipUrl());
                break;
            default:
                new QTreeWidgetItem(videos, QStringList() << clip->clipUrl());
                break;
            }
            count++;
        }
        if (clip->clipType() == Text) {
            const QStringList imagefiles = TitleWidget::extractImageList(clip->property(QStringLiteral("xmldata")));
            const QStringList fonts = TitleWidget::extractFontList(clip->property(QStringLiteral("xmldata")));
            for (const QString &file : imagefiles) {
                count++;
                new QTreeWidgetItem(images, QStringList() << file);
            }
            allFonts << fonts;
        } else if (clip->clipType() == Playlist) {
            const QStringList files = extractPlaylistUrls(clip->clipUrl());
            for (const QString &file : files) {
                count++;
                new QTreeWidgetItem(others, QStringList() << file);
            }
        }
    }

    uint used = 0;
    uint unUsed = 0;
    qint64 usedSize = 0;
    qint64 unUsedSize = 0;
    pCore->bin()->getBinStats(&used, &unUsed, &usedSize, &unUsedSize);
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
    unused_count->setText(QString::number(unUsed));
    unused_size->setText(KIO::convertSize(unUsedSize));
    delete_unused->setEnabled(unUsed > 0);
}

const QString ProjectSettings::selectedPreview() const
{
    return preview_profile->itemData(preview_profile->currentIndex()).toString();
}

void ProjectSettings::accept()
{
    if (selectedProfile().isEmpty()) {
        KMessageBox::error(this, i18n("Please select a video profile"));
        return;
    }
    QString params = preview_profile->itemData(preview_profile->currentIndex()).toString();
    if (!params.isEmpty()) {
        if (params.section(QLatin1Char(';'), 0, 0) != m_previewparams || params.section(QLatin1Char(';'), 1, 1) != m_previewextension) {
            // Timeline preview settings changed, warn
            if (KMessageBox::warningContinueCancel(this, i18n("You changed the timeline preview profile. This will remove all existing timeline previews for this project.\n Are you sure you want to proceed?"), i18n("Confirm profile change")) == KMessageBox::Cancel) {
                return;
            }
        }
    }
    if (!m_savedProject && selectedProfile() != KdenliveSettings::current_profile()) {
        if (KMessageBox::warningContinueCancel(this, i18n("Changing the profile of your project cannot be undone.\nIt is recommended to save your project before attempting this operation that might cause some corruption in transitions.\n Are you sure you want to proceed?"), i18n("Confirm profile change")) == KMessageBox::Cancel) {
            return;
        }
    }
    QDialog::accept();
}

void ProjectSettings::slotUpdateButton(const QString &path)
{
    if (path.isEmpty()) {
        m_buttonOk->setEnabled(false);
    } else {
        m_buttonOk->setEnabled(true);
        slotUpdateFiles(true);
    }
}

QString ProjectSettings::selectedProfile() const
{
    return m_pw->selectedProfile();
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
    return params.section(QLatin1Char(';'), 0, 0);
}

QString ProjectSettings::proxyExtension() const
{
    QString params = proxy_profile->itemData(proxy_profile->currentIndex()).toString();
    return params.section(QLatin1Char(';'), 1, 1);
}

//static
QStringList ProjectSettings::extractPlaylistUrls(const QString &path)
{
    QStringList urls;
    QDomDocument doc;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return urls;
    }
    if (!doc.setContent(&file)) {
        file.close();
        return urls;
    }
    file.close();
    QString root = doc.documentElement().attribute(QStringLiteral("root"));
    if (!root.isEmpty() && !root.endsWith(QLatin1Char('/'))) {
        root.append(QLatin1Char('/'));
    }
    QDomNodeList files = doc.elementsByTagName(QStringLiteral("producer"));
    for (int i = 0; i < files.count(); ++i) {
        QDomElement e = files.at(i).toElement();
        QString type = EffectsList::property(e, QStringLiteral("mlt_service"));
        if (type != QLatin1String("colour")) {
            QString url = EffectsList::property(e, QStringLiteral("resource"));
            if (type == QLatin1String("timewarp")) {
                url = EffectsList::property(e, QStringLiteral("warp_resource"));
            } else if (type == QLatin1String("framebuffer")) {
                url = url.section(QLatin1Char('?'), 0, 0);
            }
            if (!url.isEmpty()) {
                if (QFileInfo(url).isRelative()) {
                    url.prepend(root);
                }
                if (url.section(QLatin1Char('.'), 0, -2).endsWith(QLatin1String("/.all"))) {
                    // slideshow clip, extract image urls
                    urls << extractSlideshowUrls(url);
                } else {
                    urls << url;
                }
                if (url.endsWith(QLatin1String(".mlt")) || url.endsWith(QLatin1String(".kdenlive"))) {
                    //TODO: Do something to avoid infinite loops if 2 files reference themselves...
                    urls << extractPlaylistUrls(url);
                }
            }
        }
    }

    // luma files for transitions
    files = doc.elementsByTagName(QStringLiteral("transition"));
    for (int i = 0; i < files.count(); ++i) {
        QDomElement e = files.at(i).toElement();
        QString url = EffectsList::property(e, QStringLiteral("resource"));
        if (!url.isEmpty()) {
            if (QFileInfo(url).isRelative()) {
                url.prepend(root);
            }
            urls << url;
        }
    }

    return urls;
}

//static
QStringList ProjectSettings::extractSlideshowUrls(const QString &url)
{
    QStringList urls;
    QString path = QFileInfo(url).absolutePath();
    QDir dir(path);
    if (url.contains(QStringLiteral(".all."))) {
        // this is a mime slideshow, like *.jpeg
        QString ext = url.section(QLatin1Char('.'), -1);
        QStringList filters;
        filters << QStringLiteral("*.") + ext;
        dir.setNameFilters(filters);
        QStringList result = dir.entryList(QDir::Files);
        urls.append(path + filters.at(0) + QStringLiteral(" (") + i18np("1 image found", "%1 images found", result.count()) + QLatin1Char(')'));
    } else {
        // this is a pattern slideshow, like sequence%4d.jpg
        QString filter = QFileInfo(url).fileName();
        QString ext = filter.section(QLatin1Char('.'), -1);
        filter = filter.section(QLatin1Char('%'), 0, -2);
        QString regexp = QLatin1Char('^') + filter + QStringLiteral("\\d+\\.") + ext + QLatin1Char('$');
        QRegExp rx(regexp);
        int count = 0;
        const QStringList result = dir.entryList(QDir::Files);
        for (const QString &p: result) {
            if (rx.exactMatch(p)) {
                count++;
            }
        }
        urls.append(url + QStringLiteral(" (") + i18np("1 image found", "%1 images found", count) + QLatin1Char(')'));
    }
    return urls;
}

void ProjectSettings::slotExportToText()
{
    const QString savePath = QFileDialog::getSaveFileName(this, QString(), project_folder->url().toLocalFile(), QStringLiteral("text/plain"));
    if (savePath.isEmpty()) {
        return;
    }
    QString text;
    text.append(i18n("Project folder: %1",  project_folder->url().toLocalFile()) + '\n');
    text.append(i18n("Project profile: %1",  m_pw->selectedProfile()) + '\n');
    text.append(i18n("Total clips: %1 (%2 used in timeline).", files_count->text(), used_count->text()) + "\n\n");
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        if (files_list->topLevelItem(i)->childCount() > 0) {
            text.append('\n' + files_list->topLevelItem(i)->text(0) + ":\n\n");
            for (int j = 0; j < files_list->topLevelItem(i)->childCount(); ++j) {
                text.append(files_list->topLevelItem(i)->child(j)->text(0) + '\n');
            }
        }
    }
    QTemporaryFile tmpfile;
    if (!tmpfile.open()) {
        qCWarning(KDENLIVE_LOG) << "/////  CANNOT CREATE TMP FILE in: " << tmpfile.fileName();
        return;
    }
    QFile xmlf(tmpfile.fileName());
    if (!xmlf.open(QIODevice::WriteOnly)) {
        return;
    }
    xmlf.write(text.toUtf8());
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
    proxyparams->setPlainText(params.section(QLatin1Char(';'), 0, 0));
}

void ProjectSettings::slotUpdatePreviewParams()
{
    QString params = preview_profile->itemData(preview_profile->currentIndex()).toString();
    previewparams->setPlainText(params.section(QLatin1Char(';'), 0, 0));
}

const QMap<QString, QString> ProjectSettings::metadata() const
{
    QMap<QString, QString> metadata;
    for (int i = 0; i < metadata_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = metadata_list->topLevelItem(i);
        if (!item->text(1).simplified().isEmpty()) {
            // Insert metadata entry
            QString key = item->data(0, Qt::UserRole).toString();
            if (key.isEmpty()) {
                key = QStringLiteral("meta.attr.") + item->text(0).simplified() + QStringLiteral(".markup");
            }
            QString value = item->text(1);
            metadata.insert(key, value);
        }
    }
    return metadata;
}

void ProjectSettings::slotAddMetadataField()
{
    QString metaField = QInputDialog::getText(this, i18n("Metadata"), i18n("Metadata"));
    if (metaField.isEmpty()) {
        return;
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(metadata_list, QStringList() << metaField);
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void ProjectSettings::slotDeleteMetadataField()
{
    QTreeWidgetItem *item = metadata_list->currentItem();
    if (item) {
        delete item;
    }
}

void ProjectSettings::slotManageEncodingProfile()
{
    QPointer<EncodingProfilesDialog> d = new EncodingProfilesDialog(0);
    d->exec();
    delete d;
    loadProxyProfiles();
}

void ProjectSettings::slotManagePreviewProfile()
{
    QPointer<EncodingProfilesDialog> d = new EncodingProfilesDialog(1);
    d->exec();
    delete d;
    loadPreviewProfiles();
}

void ProjectSettings::loadProxyProfiles()
{
    // load proxy profiles
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "proxy");
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> k(values);
    int ix = -1;
    proxy_profile->clear();
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            QString params = k.value().section(QLatin1Char(';'), 0, 0);
            QString extension = k.value().section(QLatin1Char(';'), 1, 1);
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
        proxy_profile->addItem(i18n("Current Settings"), QString(m_proxyparameters + QLatin1Char(';') + m_proxyextension));
    }
    proxy_profile->setCurrentIndex(ix);
    slotUpdateProxyParams();
}

void ProjectSettings::loadPreviewProfiles()
{
    // load proxy profiles
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "timelinepreview");
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> k(values);
    int ix = -1;
    preview_profile->clear();
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            QString params = k.value().section(QLatin1Char(';'), 0, 0);
            QString extension = k.value().section(QLatin1Char(';'), 1, 1);
            if (ix == -1 && (params == m_previewparams && extension == m_previewextension)) {
                // this is the current profile
                ix = preview_profile->count();
            }
            preview_profile->addItem(k.key(), k.value());
        }
    }
    if (ix == -1) {
        // Current project proxy settings not found
        ix = preview_profile->count();
        if (m_previewparams.isEmpty() && m_previewextension.isEmpty()) {
            // Leave empty, will be automatically detected
            preview_profile->addItem(i18n("Auto"));
        } else {
            preview_profile->addItem(i18n("Current Settings"), QString(m_previewparams + QLatin1Char(';') + m_previewextension));
        }
    }
    preview_profile->setCurrentIndex(ix);
    slotUpdatePreviewParams();
}

const QString ProjectSettings::storageFolder() const
{
    if (custom_folder->isChecked()) {
        return project_folder->url().toLocalFile();
    }
    return QString();
}
