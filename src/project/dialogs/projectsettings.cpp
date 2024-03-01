/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "projectsettings.h"

#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/customcamcorderdialog.h"
#include "dialogs/profilesdialog.h"
#include "dialogs/wizard.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mltcontroller/clipcontroller.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/dialogs/profilewidget.h"
#include "project/dialogs/temporarydata.h"
#include "titler/titlewidget.h"
#include "xml/xml.hpp"

#include "kdenlive_debug.h"
#include "utils/KMessageBox_KdenliveCompat.h"
#include <KIO/FileCopyJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <kio/directorysizejob.h>

#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTemporaryFile>

class NoEditDelegate : public QStyledItemDelegate
{
public:
    NoEditDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(parent);
        Q_UNUSED(option);
        Q_UNUSED(index);
        return nullptr;
    }
};

ProjectSettings::ProjectSettings(KdenliveDoc *doc, QMap<QString, QString> metadata, QStringList lumas, int videotracks, int audiotracks, int audiochannels,
                                 const QString & /*projectPath*/, bool readOnlyTracks, bool savedProject, QWidget *parent)
    : QDialog(parent)
    , m_savedProject(savedProject)
    , m_lumas(std::move(lumas))
    , m_newProject(doc == nullptr)
{
    setupUi(this);
    tabWidget->setTabBarAutoHide(true);
    auto *vbox = new QVBoxLayout;
    vbox->setContentsMargins(0, 0, 0, 0);
    m_pw = new ProfileWidget(this);
    vbox->addWidget(m_pw);
    profile_box->setLayout(vbox);
    profile_box->setTitle(i18n("Select the profile (preset) of the project"));
    file_message->hide();

    default_folder->setText(i18n("Default: %1", QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));

    list_search->setTreeWidget(files_list);
    project_folder->setMode(KFile::Directory);

    m_buttonOk = buttonBox->button(QDialogButtonBox::Ok);
    // buttonOk->setEnabled(false);
    audio_thumbs->setChecked(KdenliveSettings::audiothumbnails());
    video_thumbs->setChecked(KdenliveSettings::videothumbnails());
    audio_tracks->setValue(audiotracks);
    video_tracks->setValue(videotracks);
    qDebug() << "::::: GOT PROJECT AUDIO CHANNELS: " << audiochannels << "\nBBBBBBBBBBBBBBBBBBB";
    if (audiochannels == 4) {
        audio_channels->setCurrentIndex(1);
    } else if (audiochannels == 6) {
        audio_channels->setCurrentIndex(2);
    }
    connect(generate_proxy, &QAbstractButton::toggled, proxy_minsize, &QWidget::setEnabled);
    connect(checkProxy, &QToolButton::clicked, pCore.get(), &Core::testProxies);
    connect(generate_imageproxy, &QAbstractButton::toggled, proxy_imageminsize, &QWidget::setEnabled);
    connect(generate_imageproxy, &QAbstractButton::toggled, image_label, &QWidget::setEnabled);
    connect(generate_imageproxy, &QAbstractButton::toggled, proxy_imagesize, &QWidget::setEnabled);
    connect(video_tracks, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]() {
        if (video_tracks->value() + audio_tracks->value() <= 0) {
            video_tracks->setValue(1);
        }
    });
    connect(audio_tracks, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]() {
        if (video_tracks->value() + audio_tracks->value() <= 0) {
            audio_tracks->setValue(1);
        }
    });

    connect(external_proxy, &QCheckBox::toggled, this, &ProjectSettings::slotExternalProxyChanged);
    connect(external_proxy, &QCheckBox::toggled, external_proxy_profile, &QComboBox::setEnabled);
    connect(external_proxy_profile, &QComboBox::currentTextChanged, this, &ProjectSettings::slotExternalProxyProfileChanged);
    slotExternalProxyChanged(external_proxy->checkState());
    connect(manage_external, &QToolButton::clicked, this, &ProjectSettings::configureExternalProxies);

    QString currentProf;
    if (doc) {
        currentProf = pCore->getCurrentProfile()->path();
        proxy_box->setChecked(doc->useProxy());
        generate_proxy->setChecked(doc->getDocumentProperty(QStringLiteral("generateproxy")).toInt() != 0);
        proxy_minsize->setValue(doc->getDocumentProperty(QStringLiteral("proxyminsize")).toInt());
        m_proxyparameters = doc->getDocumentProperty(QStringLiteral("proxyparams"));
        m_initialExternalProxyProfile = doc->getDocumentProperty(QStringLiteral("externalproxyparams"));
        generate_imageproxy->setChecked(doc->getDocumentProperty(QStringLiteral("generateimageproxy")).toInt() != 0);
        proxy_imageminsize->setValue(doc->getDocumentProperty(QStringLiteral("proxyimageminsize")).toInt());
        proxy_imagesize->setValue(doc->getDocumentProperty(QStringLiteral("proxyimagesize")).toInt());
        proxy_resize->setValue(doc->getDocumentProperty(QStringLiteral("proxyresize")).toInt());
        m_proxyextension = doc->getDocumentProperty(QStringLiteral("proxyextension"));
        external_proxy->setChecked(doc->getDocumentProperty(QStringLiteral("enableexternalproxy")).toInt() != 0);
        m_previewparams = doc->getDocumentProperty(QStringLiteral("previewparameters"));
        m_previewextension = doc->getDocumentProperty(QStringLiteral("previewextension"));
        QString storageFolder = doc->getDocumentProperty(QStringLiteral("storagefolder"));
        if (doc->projectTempFolder() == (QFileInfo(doc->url().toLocalFile()).absolutePath() + QStringLiteral("/cachefiles"))) {
            same_folder->setChecked(true);
        } else if (!storageFolder.isEmpty()) {
            custom_folder->setChecked(true);
        } else {
            default_folder->setChecked(true);
        }
        project_folder->setUrl(QUrl::fromLocalFile(doc->projectTempFolder()));
        auto *cacheWidget = new TemporaryData(doc, true, this);
        cacheWidget->buttonBox->hide();
        connect(cacheWidget, &TemporaryData::disableProxies, this, &ProjectSettings::disableProxies);
        connect(cacheWidget, &TemporaryData::disablePreview, this, &ProjectSettings::disablePreview);
        tabWidget->addTab(cacheWidget, i18n("Cache Data"));
    } else {
        currentProf = KdenliveSettings::default_profile();
        proxy_box->setChecked(KdenliveSettings::enableproxy());
        external_proxy->setChecked(KdenliveSettings::externalproxy());
        qDebug() << "//// INITIAL REPORT; ENABLE EXT PROCY: " << KdenliveSettings::externalproxy() << "\n++++++++";
        m_initialExternalProxyProfile = KdenliveSettings::externalProxyProfile();
        generate_proxy->setChecked(KdenliveSettings::generateproxy());
        proxy_minsize->setValue(KdenliveSettings::proxyminsize());
        m_proxyparameters = KdenliveSettings::proxyparams();
        generate_imageproxy->setChecked(KdenliveSettings::generateimageproxy());
        proxy_imageminsize->setValue(KdenliveSettings::proxyimageminsize());
        proxy_resize->setValue(KdenliveSettings::proxyscale());
        m_proxyextension = KdenliveSettings::proxyextension();
        m_previewparams = KdenliveSettings::previewparams();
        m_previewextension = KdenliveSettings::previewextension();
        if (!KdenliveSettings::defaultprojectfolder().isEmpty()) {
            project_folder->setUrl(QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder()));
        } else {
            project_folder->setUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        }
        if (KdenliveSettings::customprojectfolder()) {
            custom_folder->setChecked(true);
        } else if (KdenliveSettings::sameprojectfolder()) {
            same_folder->setChecked(true);
        } else {
            default_folder->setChecked(true);
        }
    }
    external_proxy_profile->setEnabled(external_proxy->isChecked());

    // Select profile
    m_pw->loadProfile(currentProf);

    proxy_minsize->setEnabled(generate_proxy->isChecked());
    proxy_imageminsize->setEnabled(generate_imageproxy->isChecked());
    QString currentProfileParams;
    if (!m_previewparams.isEmpty() || !m_previewextension.isEmpty()) {
        currentProfileParams = QString("%1;%2").arg(m_previewparams, m_previewextension);
    }
    m_tlPreviewProfiles = new EncodingTimelinePreviewProfilesChooser(this, true, currentProfileParams);
    preview_profile_box->addWidget(m_tlPreviewProfiles);
    connect(m_pw, &ProfileWidget::profileChanged, this, [this]() { m_tlPreviewProfiles->filterPreviewProfiles(m_pw->selectedProfile()); });
    m_tlPreviewProfiles->filterPreviewProfiles(currentProf);
    loadProxyProfiles();
    loadExternalProxyProfiles();

    // Proxy GUI stuff
    proxy_showprofileinfo->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    proxy_showprofileinfo->setToolTip(i18n("Show default profile parameters"));
    proxy_manageprofile->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    proxy_manageprofile->setToolTip(i18n("Manage proxy profiles"));
    checkProxy->setIcon(QIcon::fromTheme(QStringLiteral("run-build")));
    checkProxy->setToolTip(i18n("Compare proxy profiles efficiency"));

    connect(proxy_manageprofile, &QAbstractButton::clicked, this, &ProjectSettings::slotManageEncodingProfile);
    proxy_profile->setToolTip(i18n("Select default proxy profile"));

    connect(proxy_profile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectSettings::slotUpdateProxyParams);
    proxyparams->setVisible(false);
    proxyparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    connect(proxy_showprofileinfo, &QAbstractButton::clicked, proxyparams, &QWidget::setVisible);

    external_proxy_profile->setToolTip(i18n("Select camcorder profile"));

    if (readOnlyTracks) {
        video_tracks->setEnabled(false);
        audio_tracks->setEnabled(false);
        audio_channels->setEnabled(false);
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
    add_metadata->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    delete_metadata->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));

    // Guides categories
    QWidget *guidesPage = tabWidget->widget(2);
    m_guidesCategories = new GuideCategories(doc, this);
    QVBoxLayout *guidesLayout = new QVBoxLayout(guidesPage);
    guidesLayout->addWidget(m_guidesCategories);

    if (!m_newProject) {
        slotUpdateFiles();
        connect(delete_unused, &QAbstractButton::clicked, this, &ProjectSettings::slotDeleteUnused);
    } else {
        // Hide project files tab since its an empty new project
        tabWidget->removeTab(4);
    }
    connect(project_folder, &KUrlRequester::textChanged, this, &ProjectSettings::slotUpdateButton);
    connect(button_export, &QAbstractButton::clicked, this, &ProjectSettings::slotExportToText);
}

void ProjectSettings::slotExternalProxyChanged(bool enabled)
{

    l_relPathOrigToProxy->setVisible(enabled);
    le_relPathOrigToProxy->setVisible(enabled);
    l_prefix_proxy->setVisible(enabled);
    le_prefix_proxy->setVisible(enabled);
    l_suffix_proxy->setVisible(enabled);
    le_suffix_proxy->setVisible(enabled);
    l_relPathProxyToOrig->setVisible(enabled);
    le_relPathProxyToOrig->setVisible(enabled);
    l_prefix_clip->setVisible(enabled);
    le_prefix_clip->setVisible(enabled);
    l_suffix_clip->setVisible(enabled);
    le_suffix_clip->setVisible(enabled);

    slotExternalProxyProfileChanged(external_proxy_profile->currentText());
}

void ProjectSettings::setExternalProxyProfileData(const QString &profileData)
{
    auto params = profileData.split(";");
    QString val1, val2, val3, val4, val5, val6;
    int count = 0;
    while (params.count() >= 6) {
        if (count > 0) {
            val1.append(QLatin1Char(';'));
            val2.append(QLatin1Char(';'));
            val3.append(QLatin1Char(';'));
            val4.append(QLatin1Char(';'));
            val5.append(QLatin1Char(';'));
            val6.append(QLatin1Char(';'));
        }
        val1.append(params.at(0));
        val2.append(params.at(1));
        val3.append(params.at(2));
        val4.append(params.at(3));
        val5.append(params.at(4));
        val6.append(params.at(5));
        params = params.mid(6);
        count++;
    }
    le_relPathOrigToProxy->setText(val1);
    le_prefix_proxy->setText(val2);
    le_suffix_proxy->setText(val3);
    le_relPathProxyToOrig->setText(val4);
    le_prefix_clip->setText(val5);
    le_suffix_clip->setText(val6);
}

void ProjectSettings::configureExternalProxies()
{
    // We want to edit the profiles
    CustomCamcorderDialog cd;
    if (cd.exec() == QDialog::Accepted) {
        // reload profiles
        m_initialExternalProxyProfile = external_proxy_profile->currentData().toString();
        loadExternalProxyProfiles();
    }
}

void ProjectSettings::slotExternalProxyProfileChanged(const QString &)
{
    setExternalProxyProfileData(external_proxy_profile->currentData().toString());
}

void ProjectSettings::slotEditMetadata(QTreeWidgetItem *item, int)
{
    metadata_list->editItem(item, 1);
}

void ProjectSettings::slotDeleteUnused()
{
    QStringList toDelete;
    QStringList idsToDelete;
    QList<std::shared_ptr<ProjectClip>> clipList = pCore->projectItemModel()->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        if (!clip->isIncludedInTimeline()) {
            idsToDelete << clip->clipId();
            ClipType::ProducerType type = clip->clipType();
            if (type != ClipType::Color && type != ClipType::Text && type != ClipType::TextTemplate) {
                QUrl url = QUrl::fromLocalFile(clip->getOriginalUrl());
                if (url.isValid() && !toDelete.contains(url.path()) && QFile::exists(url.path())) {
                    toDelete << url.path();
                }
            }
        }
    }
    // make sure our urls are not used in another clip
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        if (clip->isIncludedInTimeline()) {
            QUrl url(clip->getOriginalUrl());
            if (url.isValid() && toDelete.contains(url.path())) toDelete.removeAll(url.path());
        }
    }
    if (toDelete.count() == 0) {
        file_message->setText(i18n("No files to delete on your drive."));
        file_message->animatedShow();
        pCore->window()->slotCleanProject();
        slotUpdateFiles();
        return;
    }
    if (KMessageBox::warningTwoActionsList(
            this,
            i18n("This will remove the following files from your hard drive.\nThis action cannot be undone, only use if you know "
                 "what you are doing.\nAre you sure you want to continue?"),
            toDelete, i18n("Delete unused clips"), KStandardGuiItem::del(), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction)
        return;
    pCore->projectItemModel()->requestTrashClips(idsToDelete, toDelete);
    slotUpdateFiles();
}

void ProjectSettings::slotUpdateFiles(bool cacheOnly)
{
    qDebug() << "// UPDATING PROJECT FILES\n----------\n-----------";
    m_projectProxies.clear();
    m_projectThumbs.clear();
    if (cacheOnly) {
        return;
    }
    files_list->clear();

    // List all files that are used in the project. That also means:
    // images included in slideshow and titles, files in playlist clips
    // TODO: images used in luma transitions?

    // Setup categories
    QTreeWidgetItem *videos = new QTreeWidgetItem(files_list, QStringList() << i18n("Video clips"));
    videos->setIcon(0, QIcon::fromTheme(QStringLiteral("video-x-generic")));
    videos->setExpanded(true);
    QTreeWidgetItem *sounds = new QTreeWidgetItem(files_list, QStringList() << i18n("Audio clips"));
    sounds->setIcon(0, QIcon::fromTheme(QStringLiteral("audio-x-generic")));
    sounds->setExpanded(true);
    QTreeWidgetItem *images = new QTreeWidgetItem(files_list, QStringList() << i18n("Image clips"));
    images->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    images->setExpanded(true);
    QTreeWidgetItem *slideshows = new QTreeWidgetItem(files_list, QStringList() << i18n("Slideshow clips"));
    slideshows->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    slideshows->setExpanded(true);
    QTreeWidgetItem *texts = new QTreeWidgetItem(files_list, QStringList() << i18n("Text clips"));
    texts->setIcon(0, QIcon::fromTheme(QStringLiteral("text-plain")));
    texts->setExpanded(true);
    QTreeWidgetItem *playlists = new QTreeWidgetItem(files_list, QStringList() << i18n("Playlist clips"));
    playlists->setIcon(0, QIcon::fromTheme(QStringLiteral("video-mlt-playlist")));
    playlists->setExpanded(true);
    QTreeWidgetItem *others = new QTreeWidgetItem(files_list, QStringList() << i18n("Other clips"));
    others->setIcon(0, QIcon::fromTheme(QStringLiteral("unknown")));
    others->setExpanded(true);
    QTreeWidgetItem *subtitles = new QTreeWidgetItem(files_list, QStringList() << i18n("Subtitles"));
    subtitles->setIcon(0, QIcon::fromTheme(QStringLiteral("text-plain")));
    subtitles->setExpanded(true);
    int count = 0;
    QStringList allFonts;
    for (const QString &file : qAsConst(m_lumas)) {
        count++;
        new QTreeWidgetItem(images, QStringList() << file);
    }
    QList<std::shared_ptr<ProjectClip>> clipList = pCore->projectItemModel()->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        switch (clip->clipType()) {
        case ClipType::Color:
        case ClipType::Timeline:
            // ignore color and timeline clips in list, there is no real file
            break;
        case ClipType::SlideShow: {
            const QStringList subfiles = extractSlideshowUrls(clip->clipUrl());
            for (const QString &file : subfiles) {
                count++;
                new QTreeWidgetItem(slideshows, QStringList() << file);
            }
            break;
        }
        case ClipType::Text: {
            new QTreeWidgetItem(texts, QStringList() << clip->clipUrl());
            QString titleData = clip->getProducerProperty(QStringLiteral("xmldata"));
            const QStringList imagefiles = TitleWidget::extractImageList(titleData, pCore->currentDoc()->documentRoot());
            const QStringList fonts = TitleWidget::extractFontList(clip->getProducerProperty(QStringLiteral("xmldata")));
            for (const QString &file : imagefiles) {
                new QTreeWidgetItem(images, QStringList() << file);
            }
            allFonts << fonts;
            break;
        }
        case ClipType::Audio:
            new QTreeWidgetItem(sounds, QStringList() << clip->clipUrl());
            break;
        case ClipType::Image:
            new QTreeWidgetItem(images, QStringList() << clip->clipUrl());
            break;
        case ClipType::Playlist: {
            new QTreeWidgetItem(playlists, QStringList() << clip->clipUrl());
            const QStringList files = extractPlaylistUrls(clip->clipUrl());
            for (const QString &file : files) {
                new QTreeWidgetItem(others, QStringList() << file);
            }
            break;
        }
        case ClipType::Unknown:
            new QTreeWidgetItem(others, QStringList() << clip->clipUrl());
            break;
        default:
            new QTreeWidgetItem(videos, QStringList() << clip->clipUrl());
            break;
        }
    }
    // Subtitles
    QStringList subtitleFiles = pCore->currentDoc()->getAllSubtitlesPath(true);
    for (auto &path : subtitleFiles) {
        new QTreeWidgetItem(subtitles, QStringList() << path);
    }

    uint used = 0;
    uint unUsed = 0;
    qint64 usedSize = 0;
    qint64 unUsedSize = 0;
    pCore->bin()->getBinStats(&used, &unUsed, &usedSize, &unUsedSize);
    allFonts.removeDuplicates();
    // Hide unused categories
    for (int j = 0; j < files_list->topLevelItemCount(); ++j) {
        if (files_list->topLevelItem(j)->childCount() == 0) {
            files_list->topLevelItem(j)->setHidden(true);
        }
    }
    files_count->setText(QString::number(count));
    fonts_list->addItems(allFonts);
    if (allFonts.isEmpty()) {
        fonts_list->setHidden(true);
        label_fonts->setHidden(true);
    }
    used_count->setText(QString::number(used));
    used_size->setText(KIO::convertSize(static_cast<KIO::filesize_t>(usedSize)));
    unused_count->setText(QString::number(unUsed));
    unused_size->setText(KIO::convertSize(static_cast<KIO::filesize_t>(unUsedSize)));
    delete_unused->setEnabled(unUsed > 0);
}

const QString ProjectSettings::selectedPreview() const
{
    return QString("%1;%2").arg(m_tlPreviewProfiles->currentParams(), m_tlPreviewProfiles->currentExtension());
}

void ProjectSettings::accept()
{
    if (selectedProfile().isEmpty()) {
        KMessageBox::error(this, i18n("Please select a video profile"));
        return;
    }
    QString params = selectedPreview();
    if (!params.isEmpty()) {
        if (params.section(QLatin1Char(';'), 0, 0) != m_previewparams || params.section(QLatin1Char(';'), 1, 1) != m_previewextension) {
            // Timeline preview settings changed, warn if there are existing previews
            if (pCore->hasTimelinePreview() &&
                KMessageBox::warningContinueCancel(this,
                                                   i18n("You changed the timeline preview profile. This will remove all existing timeline previews for "
                                                        "this project.\n Are you sure you want to proceed?"),
                                                   i18n("Confirm profile change")) == KMessageBox::Cancel) {
                return;
            }
        }
    }
    if (!m_newProject && selectedProfile() != pCore->getCurrentProfile()->path()) {
        if (KMessageBox::warningContinueCancel(
                this,
                i18n("Changing the profile of your project cannot be undone.\nIt is recommended to save your project before attempting this operation "
                     "that might cause some corruption in transitions.\nAre you sure you want to proceed?"),
                i18n("Confirm profile change")) == KMessageBox::Cancel) {
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

const QStringList ProjectSettings::guidesCategories() const
{
    return m_guidesCategories->updatedGuides();
}

const QMap<int, int> ProjectSettings::remapGuidesCategories() const
{
    return m_guidesCategories->remapedGuides();
}

QString ProjectSettings::selectedProfile() const
{
    return m_pw->selectedProfile();
}

std::pair<int, int> ProjectSettings::tracks() const
{
    return {video_tracks->value(), audio_tracks->value()};
}

int ProjectSettings::audioChannels() const
{
    switch (audio_channels->currentIndex()) {
    case 1:
        return 4;
    case 2:
        return 6;
    default:
        return 2;
    }
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
    return proxy_box->isChecked();
}

bool ProjectSettings::useExternalProxy() const
{
    return external_proxy->isChecked();
}

bool ProjectSettings::generateProxy() const
{
    return generate_proxy->isChecked();
}

bool ProjectSettings::generateImageProxy() const
{
    return generate_imageproxy->isChecked();
}

bool ProjectSettings::docFolderAsStorageFolder() const
{
    return same_folder->isChecked();
}

int ProjectSettings::proxyMinSize() const
{
    return proxy_minsize->value();
}

int ProjectSettings::proxyImageMinSize() const
{
    return proxy_imageminsize->value();
}

int ProjectSettings::proxyImageSize() const
{
    return proxy_imagesize->value();
}

int ProjectSettings::proxyResize() const
{
    return proxy_resize->value();
}

QString ProjectSettings::externalProxyParams() const
{
    return external_proxy_profile->currentData().toString();
}

QString ProjectSettings::proxyParams() const
{
    QString params = proxy_profile->currentData().toString();
    return params.section(QLatin1Char(';'), 0, 0);
}

QString ProjectSettings::proxyExtension() const
{
    QString params = proxy_profile->currentData().toString();
    return params.section(QLatin1Char(';'), 1, 1);
}

QString ProjectSettings::previewParams() const
{
    return m_tlPreviewProfiles->currentParams();
}

QString ProjectSettings::previewExtension() const
{
    return m_tlPreviewProfiles->currentExtension();
}

// static
QStringList ProjectSettings::extractPlaylistUrls(const QString &path)
{
    QStringList urls;
    QDomDocument doc;

    if (!Xml::docContentFromFile(doc, path, false)) {
        return urls;
    }
    QString root = doc.documentElement().attribute(QStringLiteral("root"));
    if (!root.isEmpty() && !root.endsWith(QLatin1Char('/'))) {
        root.append(QLatin1Char('/'));
    }
    QDomNodeList chains = doc.elementsByTagName(QStringLiteral("chain"));
    for (int i = 0; i < chains.count(); ++i) {
        chains.item(i).toElement().setTagName(QStringLiteral("producer"));
    }
    QDomNodeList files = doc.elementsByTagName(QStringLiteral("producer"));
    for (int i = 0; i < files.count(); ++i) {
        QDomElement e = files.at(i).toElement();
        QString type = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        if (type != QLatin1String("colour") && type != QLatin1String("color")) {
            QString url = Xml::getXmlProperty(e, QStringLiteral("resource"));
            if (type == QLatin1String("timewarp")) {
                url = Xml::getXmlProperty(e, QStringLiteral("warp_resource"));
            } else if (type == QLatin1String("framebuffer")) {
                url = url.section(QLatin1Char('?'), 0, 0);
            }
            if (!url.isEmpty() && url != QLatin1String("<producer>")) {
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
                    // TODO: Do something to avoid infinite loops if 2 files reference themselves...
                    urls << extractPlaylistUrls(url);
                }
            }
        }
    }

    // TODO merge with similar logic in DocumentChecker

    // luma files for transitions
    files = doc.elementsByTagName(QStringLiteral("transition"));
    for (int i = 0; i < files.count(); ++i) {
        QDomElement e = files.at(i).toElement();
        QString url = Xml::getXmlProperty(e, QStringLiteral("resource"));
        if (url.isEmpty()) {
            url = Xml::getXmlProperty(e, QStringLiteral("luma"));
        }
        if (!url.isEmpty()) {
            if (QFileInfo(url).isRelative()) {
                url.prepend(root);
            }
            urls << url;
        }
    }

    // urls for vidstab stabilization data and LUTs
    files = doc.elementsByTagName(QStringLiteral("filter"));
    for (int i = 0; i < files.count(); ++i) {
        QDomElement e = files.at(i).toElement();
        QString url = Xml::getXmlProperty(e, QStringLiteral("filename"));
        if (url.isEmpty()) {
            url = Xml::getXmlProperty(e, QStringLiteral("av.file"));
        }
        if (!url.isEmpty()) {
            if (QFileInfo(url).isRelative()) {
                url.prepend(root);
            }
            urls << url;
        }
    }

    return urls;
}

// static
QStringList ProjectSettings::extractSlideshowUrls(const QString &url)
{
    QStringList urls;
    QString path = QFileInfo(url).absolutePath();
    QDir dir(path);
    if (url.contains(QStringLiteral(".all."))) {
        // this is a MIME slideshow, like *.jpeg
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
        static const QRegularExpression rx(QRegularExpression::anchoredPattern(regexp));
        int count = 0;
        const QStringList result = dir.entryList(QDir::Files);
        for (const QString &p : result) {
            if (rx.match(p).hasMatch()) {
                count++;
            }
        }
        urls.append(url + QStringLiteral(" (") + i18np("1 image found", "%1 images found", count) + QLatin1Char(')'));
    }
    return urls;
}

void ProjectSettings::slotExportToText()
{
    const QString savePath = QFileDialog::getSaveFileName(this, i18n("Save As"), QString(), i18n("Text File (*.txt)"));
    if (savePath.isEmpty()) {
        return;
    }

    QString text;
    text.append(i18n("Project folder: %1", project_folder->url().toLocalFile()) + '\n');
    text.append(i18n("Project profile: %1", m_pw->selectedProfile()) + '\n');
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
    QString params = proxy_profile->currentData().toString();
    proxyparams->setPlainText(params.section(QLatin1Char(';'), 0, 0));
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
    QString metaField = QInputDialog::getText(this, i18nc("@title:window", "Metadata"), i18n("Metadata"));
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
    QPointer<EncodingProfilesDialog> d = new EncodingProfilesDialog(EncodingProfilesManager::ProxyClips);
    d->exec();
    delete d;
    loadProxyProfiles();
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
    if (!KdenliveSettings::supportedHWCodecs().isEmpty()) {
        proxy_profile->addItem(QIcon::fromTheme(QStringLiteral("speedometer")), i18n("Automatic (%1)", Wizard::getHWCodecFriendlyName()));
    } else {
        proxy_profile->addItem(i18n("Automatic"));
    }
    const QStringList allHWCodecs = Wizard::codecs();
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            QString params = k.value().section(QLatin1Char(';'), 0, 0);
            QString extension = k.value().section(QLatin1Char(';'), 1, 1);
            if (ix == -1 && ((params == m_proxyparameters && extension == m_proxyextension))) {
                // this is the current profile
                ix = proxy_profile->count();
            }
            QString itemCodec;
            QStringList values = params.split(QLatin1Char('-'));
            for (auto &v : values) {
                if (v.startsWith(QLatin1String("vcodec ")) || v.startsWith(QLatin1String("codec:v ")) || v.startsWith(QLatin1String("c:v "))) {
                    itemCodec = v.section(QLatin1Char(' '), 1);
                    break;
                }
            }
            if (!itemCodec.isEmpty() && allHWCodecs.contains(itemCodec)) {
                if (KdenliveSettings::supportedHWCodecs().contains(itemCodec)) {
                    proxy_profile->addItem(QIcon::fromTheme(QStringLiteral("speedometer")), k.key(), k.value());
                }
                continue;
            }
            proxy_profile->addItem(k.key(), k.value());
        }
    }
    if (ix == -1) {
        // Current project proxy settings not found
        if (m_proxyparameters.isEmpty() && m_proxyextension.isEmpty()) {
            ix = 0;
        } else {
            ix = proxy_profile->count();
            proxy_profile->addItem(i18n("Current Settings"), QString(m_proxyparameters + QLatin1Char(';') + m_proxyextension));
        }
    }
    proxy_profile->setCurrentIndex(ix);
    slotUpdateProxyParams();
}

void ProjectSettings::loadExternalProxyProfiles()
{
    // load proxy profiles
    KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("externalproxies.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation), "proxy");
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> k(values);
    int ix = -1;
    external_proxy_profile->clear();
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            if (ix == -1 && k.value() == m_initialExternalProxyProfile) {
                // this is the current profile
                ix = external_proxy_profile->count();
            }
            if (k.value().contains(QLatin1Char(';'))) {
                external_proxy_profile->addItem(k.key(), k.value());
            }
        }
    }
    if (ix == -1 && !m_initialExternalProxyProfile.isEmpty()) {
        // Current project proxy settings not found
        ix = external_proxy_profile->count();
        external_proxy_profile->addItem(i18n("Current Settings"), m_initialExternalProxyProfile);
    }
    external_proxy_profile->setCurrentIndex(ix);
}

const QString ProjectSettings::storageFolder() const
{
    if (custom_folder->isChecked()) {
        return project_folder->url().toLocalFile();
    }
    return QString();
}
