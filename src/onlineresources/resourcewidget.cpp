/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "resourcewidget.hpp"
#include "core.h"
#include "kdenlivesettings.h"

#include "utils/KMessageBox_KdenliveCompat.h"
#include <KFileItem>
#include <KIO/FileCopyJob>
#include <KIO/Global>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentDirs>
#include <KSelectAction>
#include <KSqueezedTextLabel>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDatabase>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QToolBar>
#include <QtConcurrent>

#include <kcompletion_version.h>

ResourceWidget::ResourceWidget(QWidget *parent)
    : QWidget(parent)
    , m_showloadingWarning(true)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    m_tmpThumbFile = new QTemporaryFile(this);

    int iconHeight = int(QFontInfo(font()).pixelSize() * 3.5);
    m_iconSize = QSize(int(iconHeight * pCore->getCurrentDar()), iconHeight);

    slider_zoom->setRange(0, 15);
    connect(slider_zoom, &QAbstractSlider::valueChanged, this, &ResourceWidget::slotSetIconSize);
    connect(button_zoomin, &QToolButton::clicked, this, [&]() { slider_zoom->setValue(qMin(slider_zoom->value() + 1, slider_zoom->maximum())); });
    connect(button_zoomout, &QToolButton::clicked, this, [&]() { slider_zoom->setValue(qMax(slider_zoom->value() - 1, slider_zoom->minimum())); });

    message_line->hide();

    for (const QPair<QString, QString> &provider : ProvidersRepository::get()->getAllProviers()) {
        QIcon icon;
        switch (ProvidersRepository::get()->getProvider(provider.second)->type()) {
        case ProviderModel::AUDIO:
            icon = QIcon::fromTheme(QStringLiteral("player-volume"));
            break;
        case ProviderModel::VIDEO:
            icon = QIcon::fromTheme(QStringLiteral("camera-video"));
            break;
        case ProviderModel::IMAGE:
            icon = QIcon::fromTheme(QStringLiteral("camera-photo"));
            break;
        default:
            icon = QIcon();
        }
        service_list->addItem(icon, provider.first, provider.second);
    }
    connect(service_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ResourceWidget::slotChangeProvider);
    loadConfig();
    connect(provider_info, &KUrlLabel::leftClickedUrl, this, [&]() { slotOpenUrl(provider_info->url()); });
    connect(label_license, &KUrlLabel::leftClickedUrl, this, [&]() { slotOpenUrl(label_license->url()); });
    connect(search_text, &KLineEdit::returnKeyPressed, this, &ResourceWidget::slotStartSearch);
    connect(search_results, &QListWidget::currentRowChanged, this, &ResourceWidget::slotUpdateCurrentItem);
    connect(this, &ResourceWidget::gotPixmap, this, &ResourceWidget::slotShowPixmap);
    connect(button_preview, &QAbstractButton::clicked, this, [&]() {
        if (!m_currentProvider) {
            return;
        }

        slotPreviewItem();
    });

    connect(button_import, &QAbstractButton::clicked, this, [&]() {
        if (m_currentProvider->get()->downloadOAuth2()) {
            if (m_currentProvider->get()->requiresLogin()) {
                KMessageBox::information(this, i18n("Login is required to download this item.\nYou will be redirected to the login page now."));
            }
            m_currentProvider->get()->authorize();
        } else {
            if (m_currentItem->data(singleDownloadRole).toBool()) {
                if (m_currentItem->data(downloadRole).toString().isEmpty()) {
                    m_currentProvider->get()->slotFetchFiles(m_currentItem->data(idRole).toString());
                    return;
                } else {
                    slotSaveItem();
                }
            } else {
                slotChooseVersion(m_currentItem->data(downloadRole).toStringList(), m_currentItem->data(downloadLabelRole).toStringList());
            }
        }
    });

    page_number->setEnabled(false);

    connect(page_number, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ResourceWidget::slotStartSearch);
    adjustSize();
}

ResourceWidget::~ResourceWidget()
{
    saveConfig();
    delete m_tmpThumbFile;
}

/**
 * @brief ResourceWidget::saveConfig
 * Load current provider and zoom value from config file
 */
void ResourceWidget::loadConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "OnlineResources");
    slider_zoom->setValue(resourceConfig.readEntry("zoom", 7));
    if (resourceConfig.readEntry("provider", service_list->itemText(0)).isEmpty()) {
        service_list->setCurrentIndex(0);
    } else {
        service_list->setCurrentItem(resourceConfig.readEntry("provider", service_list->itemText(0)));
    }
    slotChangeProvider();
}

/**
 * @brief ResourceWidget::saveConfig
 * Save current provider and zoom value to config file
 */
void ResourceWidget::saveConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "OnlineResources");
    resourceConfig.writeEntry(QStringLiteral("provider"), service_list->currentText());
    resourceConfig.writeEntry(QStringLiteral("zoom"), slider_zoom->value());
    config->sync();
}

/**
 * @brief ResourceWidget::blockUI
 * @param block
 * Block or unblock the online resource ui
 */
void ResourceWidget::blockUI(bool block)
{
    buildin_box->setEnabled(!block);
    search_text->setEnabled(!block);
    service_list->setEnabled(!block);
    setCursor(block ? Qt::WaitCursor : Qt::ArrowCursor);
}

/**
 * @brief ResourceWidget::slotChangeProvider
 * Set m_currentProvider to the current selected provider of the service_list and update ui
 */
void ResourceWidget::slotChangeProvider()
{
    if (m_currentProvider != nullptr) {
        m_currentProvider->get()->disconnect(this);
    }

    details_box->setEnabled(false);
    button_import->setEnabled(false);
    button_preview->setEnabled(false);
    info_browser->clear();
    search_results->clear();
    page_number->blockSignals(true);
    page_number->setValue(1);
    page_number->setMaximum(1);
    page_number->blockSignals(false);

    if (service_list->currentData().toString().isEmpty()) {
        provider_info->clear();
        buildin_box->setEnabled(false);
        return;
    } else {
        buildin_box->setEnabled(true);
        message_line->hide();
    }

    m_currentProvider = &ProvidersRepository::get()->getProvider(service_list->currentData().toString());

    provider_info->setText(i18n("Media provided by %1", m_currentProvider->get()->name()));
    provider_info->setUrl(m_currentProvider->get()->homepage());
    connect(m_currentProvider->get(), &ProviderModel::searchDone, this, &ResourceWidget::slotSearchFinished);
    connect(m_currentProvider->get(), &ProviderModel::searchError, this, &ResourceWidget::slotDisplayError);

    connect(m_currentProvider->get(), &ProviderModel::fetchedFiles, this, &ResourceWidget::slotChooseVersion);
    connect(m_currentProvider->get(), &ProviderModel::authenticated, this, &ResourceWidget::slotAccessTokenReceived);

    // automatically kick of a search if we have search text and we switch services.
    if (!search_text->text().isEmpty()) {
        slotStartSearch();
    }
}

/**
 * @brief ResourceWidget::slotOpenUrl
 * @param url link to open in external browser
 * Open a url in a external browser
 */
void ResourceWidget::slotOpenUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

/**
 * @brief ResourceWidget::slotStartSearch
 * Calls slotStartSearch on the object for the currently selected service.
 */
void ResourceWidget::slotStartSearch()
{
    message_line->setText(i18nc("@info:status", "Search pending…"));
    message_line->setMessageType(KMessageWidget::Information);
    message_line->show();

    blockUI(true);
    details_box->setEnabled(false);
    button_import->setEnabled(false);
    button_preview->setEnabled(false);
    info_browser->clear();
    search_results->clear();
    m_currentProvider->get()->slotStartSearch(search_text->text(), page_number->value());
}

/**
 * @brief ResourceWidget::slotDisplayError
 * @param message the error text
 * Displays the items of list in the search_results ListView
 */
void ResourceWidget::slotDisplayError(const QString &message)
{
    message_line->setText(i18n("Search failed! %1", message));
    message_line->setMessageType(KMessageWidget::Error);
    message_line->show();
    page_number->setEnabled(false);
    service_list->setEnabled(true);
    buildin_box->setEnabled(true);
    setCursor(Qt::ArrowCursor);
}

/**
 * @brief ResourceWidget::slotSearchFinished
 * @param list list of the found items
 * @param pageCount number of found pages
 * Displays the items of list in the search_results ListView
 */
void ResourceWidget::slotSearchFinished(const QList<ResourceItemInfo> &list, int pageCount)
{
    QMutexLocker lock(&m_imageLock);
    m_imagesUrl.clear();
    if (list.isEmpty()) {
        message_line->setText(i18nc("@info", "No items found."));
        message_line->setMessageType(KMessageWidget::Error);
        message_line->show();
        blockUI(false);
        return;
    }

    message_line->setMessageType(KMessageWidget::Information);
    message_line->show();
    int count = 0;
    for (const ResourceItemInfo &item : qAsConst(list)) {
        message_line->setText(i18nc("@info:progress", "Parsing item %1 of %2…", count, list.count()));
        // if item has no name use "Created by Author", if item even has no author use "Unnamed"
        QListWidgetItem *listItem = new QListWidgetItem(
            item.name.isEmpty() ? (item.author.isEmpty() ? i18n("Unnamed") : i18nc("Created by author name", "Created by %1", item.author)) : item.name);
        if (!item.imageUrl.isEmpty()) {
            m_imagesUrl << item.imageUrl;
        }

        listItem->setData(idRole, item.id);
        listItem->setData(nameRole, item.name);
        listItem->setData(filetypeRole, item.filetype);
        listItem->setData(descriptionRole, item.description);
        listItem->setData(imageRole, item.imageUrl);
        listItem->setData(previewRole, item.previewUrl);
        listItem->setData(authorUrl, item.authorUrl);
        listItem->setData(authorRole, item.author);
        listItem->setData(widthRole, item.width);
        listItem->setData(heightRole, item.height);
        listItem->setData(durationRole, item.duration);
        listItem->setData(urlRole, item.infoUrl);
        listItem->setData(licenseRole, item.license);
        if (item.downloadUrl.isEmpty() && item.downloadUrls.length() > 0) {
            listItem->setData(singleDownloadRole, false);
            listItem->setData(downloadRole, item.downloadUrls);
            listItem->setData(downloadLabelRole, item.downloadLabels);
        } else {
            listItem->setData(singleDownloadRole, true);
            listItem->setData(downloadRole, item.downloadUrl);
        }
        search_results->addItem(listItem);
        count++;
    }
    m_imagesUrl.removeDuplicates();
    message_line->hide();
    page_number->setMaximum(pageCount);
    page_number->setEnabled(true);
    blockUI(false);
    lock.unlock();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(this, &ResourceWidget::slotLoadImages);
#else
    (void)QtConcurrent::run(&ResourceWidget::slotLoadImages, this);
#endif
}

void ResourceWidget::slotShowPixmap(const QString &url, const QPixmap &pixmap)
{
    for (int i = 0; i < search_results->count(); i++) {
        auto item = search_results->item(i);
        if (item->data(imageRole).toString() == url) {
            item->setIcon(pixmap);
            break;
        }
    }
}

void ResourceWidget::slotLoadImages()
{
    if (!m_imageLock.tryLock()) {
        // Another request is loading, wait
        return;
    }
    if (m_imagesUrl.isEmpty()) {
        // No images to load
        m_imageLock.unlock();
        return;
    }
    while (!m_imagesUrl.isEmpty()) {
        const QString url = m_imagesUrl.takeFirst();
        m_imageLock.unlock();
        QUrl img(url);
        m_tmpThumbFile->close();
        if (m_tmpThumbFile->open()) {
            KIO::FileCopyJob *copyjob = KIO::file_copy(img, QUrl::fromLocalFile(m_tmpThumbFile->fileName()), -1, KIO::HideProgressInfo | KIO::Overwrite);
            if (copyjob->exec()) {
                QPixmap pic(m_tmpThumbFile->fileName());
                Q_EMIT gotPixmap(url, pic);
            }
        }
        if (m_imagesUrl.isEmpty()) {
            break;
        }
        if (!m_imageLock.tryLock()) {
            break;
        }
    }
}

/**
 * @brief ResourceWidget::slotUpdateCurrentItem
 * Set m_currentItem to the current selected item of the search_results ListView and
 * show its details within the details_box
 */
void ResourceWidget::slotUpdateCurrentItem()
{
    details_box->setEnabled(false);
    button_import->setEnabled(false);
    button_preview->setEnabled(false);

    // get the item the user selected
    m_currentItem = search_results->currentItem();
    if (!m_currentItem) {
        return;
    }

    if (m_currentProvider->get()->type() != ProviderModel::IMAGE && !m_currentItem->data(previewRole).toString().isEmpty()) {
        button_preview->show();
    } else {
        button_preview->hide();
    }

    QString details = "<h3>" + m_currentItem->text();

    if (!m_currentItem->data(urlRole).toString().isEmpty()) {
        details += QStringLiteral(" <a href=\"%1\">%2</a>").arg(m_currentItem->data(urlRole).toString(), i18nc("the url link pointing to a web page", "link"));
    }

    details.append(QStringLiteral("</h3>"));

    if (!m_currentItem->data(authorUrl).toString().isEmpty()) {
        details += i18n("Created by <a href=\"%1\">", m_currentItem->data(authorUrl).toString());
        if (!m_currentItem->data(authorRole).toString().isEmpty()) {
            details.append(m_currentItem->data(authorRole).toString());
        } else {
            details.append(i18n("Author"));
        }
        details.append(QStringLiteral("</a><br />"));
    } else if (!m_currentItem->data(authorRole).toString().isEmpty()) {
        details.append(i18n("Created by %1", m_currentItem->data(authorRole).toString()) + QStringLiteral("<br />"));
    } else {
        details.append(QStringLiteral("<br />"));
    }

    if (m_currentProvider->get()->type() != ProviderModel::AUDIO && m_currentItem->data(widthRole).toInt() != 0) {
        details.append(i18n("Size: %1 x %2", m_currentItem->data(widthRole).toInt(), m_currentItem->data(heightRole).toInt()) + QStringLiteral("<br />"));
    }
    if (m_currentItem->data(durationRole).toInt() != 0) {
        details.append(i18n("Duration: %1 sec", m_currentItem->data(durationRole).toInt()) + QStringLiteral("<br />"));
    }
    details.append(m_currentItem->data(descriptionRole).toString());
    info_browser->setHtml(details);

    label_license->setText(licenseNameFromUrl(m_currentItem->data(licenseRole).toString(), true));
    label_license->setTipText(licenseNameFromUrl(m_currentItem->data(licenseRole).toString(), false));
    label_license->setUseTips(true);
    label_license->setUrl(m_currentItem->data(licenseRole).toString());

    details_box->setEnabled(true);
    button_import->setEnabled(true);
    button_preview->setEnabled(true);
}

/**
 * @brief ResourceWidget::licenseNameFromUrl
 * @param licenseUrl
 * @param shortName Whether the long name like "Attribution-NonCommercial-ShareAlike 3.0" or the short name like "CC BY-NC-SA 3.0" should be returned
 * @return the license name "Unnamed License" if url is not known.
 */
QString ResourceWidget::licenseNameFromUrl(const QString &licenseUrl, const bool shortName)
{
    QString licenseName;
    QString licenseShortName;

    if (licenseUrl.contains("creativecommons.org")) {
        if (licenseUrl.contains(QStringLiteral("/sampling+/"))) {
            licenseName = i18nc("Creative Commons License", "CC Sampling+");
        } else if (licenseUrl.contains(QStringLiteral("/by/"))) {
            licenseName = i18nc("Creative Commons License", "Creative Commons Attribution");
            licenseShortName = i18nc("Creative Commons License (short)", "CC BY");
        } else if (licenseUrl.contains(QStringLiteral("/by-nd/"))) {
            licenseName = i18nc("Creative Commons License", "Creative Commons Attribution-NoDerivs");
            licenseShortName = i18nc("Creative Commons License (short)", "CC BY-ND");
        } else if (licenseUrl.contains(QStringLiteral("/by-nc-sa/"))) {
            licenseName = i18nc("Creative Commons License", "Creative Commons Attribution-NonCommercial-ShareAlike");
            licenseShortName = i18nc("Creative Commons License (short)", "CC BY-NC-SA");
        } else if (licenseUrl.contains(QStringLiteral("/by-sa/"))) {
            licenseName = i18nc("Creative Commons License", "Creative Commons Attribution-ShareAlike");
            licenseShortName = i18nc("Creative Commons License (short)", "CC BY-SA");
        } else if (licenseUrl.contains(QStringLiteral("/by-nc/"))) {
            licenseName = i18nc("Creative Commons License", "Creative Commons Attribution-NonCommercial");
            licenseShortName = i18nc("Creative Commons License (short)", "CC BY-NC");
        } else if (licenseUrl.contains(QStringLiteral("/by-nc-nd/"))) {
            licenseName = i18nc("Creative Commons License", "Creative Commons Attribution-NonCommercial-NoDerivs");
            licenseShortName = i18nc("Creative Commons License (short)", "CC BY-NC-ND");
        } else if (licenseUrl.contains(QLatin1String("/publicdomain/zero/"))) {
            licenseName = i18nc("Creative Commons License", "Creative Commons 0");
            licenseShortName = i18nc("Creative Commons License (short)", "CC 0");
        } else if (licenseUrl.endsWith(QLatin1String("/publicdomain")) || licenseUrl.contains(QLatin1String("openclipart.org/share"))) {
            licenseName = i18nc("License", "Public Domain");
        } else {
            licenseShortName = i18nc("Short for: Unknown Creative Commons License", "Unknown CC License");
            licenseName = i18n("Unknown Creative Commons License");
        }

        if (licenseUrl.contains(QStringLiteral("/1.0"))) {
            licenseName.append(QStringLiteral(" 1.0"));
            licenseShortName.append(QStringLiteral(" 1.0"));
        } else if (licenseUrl.contains(QStringLiteral("/2.0"))) {
            licenseName.append(QStringLiteral(" 2.0"));
            licenseShortName.append(QStringLiteral(" 2.0"));
        } else if (licenseUrl.contains(QStringLiteral("/2.5"))) {
            licenseName.append(QStringLiteral(" 2.5"));
            licenseShortName.append(QStringLiteral(" 2.5"));
        } else if (licenseUrl.contains(QStringLiteral("/3.0"))) {
            licenseName.append(QStringLiteral(" 3.0"));
            licenseShortName.append(QStringLiteral(" 3.0"));
        } else if (licenseUrl.contains(QStringLiteral("/4.0"))) {
            licenseName.append(QStringLiteral(" 4.0"));
            licenseShortName.append(QStringLiteral(" 4.0"));
        }
    } else if (licenseUrl.contains("pexels.com/license/")) {
        licenseName = i18n("Pexels License");
    } else if (licenseUrl.contains("pixabay.com/service/license/")) {
        licenseName = i18n("Pixabay License");
        ;
    } else {
        licenseName = i18n("Unknown License");
    }

    if (shortName && !licenseShortName.isEmpty()) {
        return licenseShortName;
    } else {
        return licenseName;
    }
}

/**
 * @brief ResourceWidget::slotSetIconSize
 * @param size
 * Set the icon size for the search_results ListView
 */
void ResourceWidget::slotSetIconSize(int size)
{
    if (!search_results) {
        return;
    }
    QSize zoom = m_iconSize;
    zoom = zoom * (size / 4.0);
    search_results->setIconSize(zoom);
}

/**
 * @brief ResourceWidget::slotPreviewItem
 * Emits the previewClip signal if m_currentItem is valid to display a preview in the Clip Monitor
 */
void ResourceWidget::slotPreviewItem()
{
    if (!m_currentItem) {
        return;
    }
    blockUI(true);
    const QString path = m_currentItem->data(previewRole).toString();
    if (m_showloadingWarning && !QUrl::fromUserInput(path).isLocalFile()) {
        message_line->setText(i18n("It maybe takes a while until the preview is loaded"));
        message_line->setMessageType(KMessageWidget::Warning);
        message_line->show();
        QTimer::singleShot(6000, message_line, &KMessageWidget::animatedHide);
        repaint();
        // Only show this warning once
        m_showloadingWarning = false;
    }
    Q_EMIT previewClip(path, i18n("Online Resources Preview"));
    blockUI(false);
}

/**
 * @brief ResourceWidget::slotChooseVersion
 * @param urls list of download urls pointing to the certain file version
 * @param labels list of labels for the certain file version (needs to have the same order than urls)
 * @param accessToken access token to pass through to slotSaveItem
 * Displays a dialog to let the user choose a file version (e.g. filetype, quality) if there are multiple versions
 * available
 */
void ResourceWidget::slotChooseVersion(const QStringList &urls, const QStringList &labels, const QString &accessToken)
{
    if (urls.isEmpty() || labels.isEmpty()) {
        return;
    }
    if (urls.length() == 1) {
        slotSaveItem(urls.first(), accessToken);
        return;
    }
    bool ok;
    QString name = QInputDialog::getItem(this, i18nc("@title:window", "Choose File Version"), i18n("Please choose the version you want to download"), labels, 0,
                                         false, &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    slotSaveItem(urls.at(labels.indexOf(name)), accessToken);
}

/**
 * @brief ResourceWidget::slotSaveItem
 * @param originalUrl url pointing to the download file
 * @param accessToken the access token (optional)
 * Opens a dialog for user to choose a save location and start the download of the file
 */
void ResourceWidget::slotSaveItem(const QString &originalUrl, const QString &accessToken)
{
    QUrl saveUrl;

    if (!m_currentItem) {
        return;
    }

    QUrl srcUrl(originalUrl.isEmpty() ? m_currentItem->data(downloadRole).toString() : originalUrl);
    if (srcUrl.isEmpty()) {
        return;
    }

    QString path = KRecentDirs::dir(QStringLiteral(":KdenliveOnlineResourceFolder"));
    QString ext;

    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    if (!path.endsWith(QLatin1Char('/'))) {
        path.append(QLatin1Char('/'));
    }
    if (!srcUrl.fileName().isEmpty()) {
        path.append(srcUrl.fileName());
        ext = "*." + srcUrl.fileName().section(QLatin1Char('.'), -1);
    } else if (!m_currentItem->data(filetypeRole).toString().isEmpty()) {
        ext = "*." + m_currentItem->data(filetypeRole).toString();
    } else {
        if (m_currentProvider->get()->type() == ProviderModel::AUDIO) {
            ext = i18n("Audio") + QStringLiteral(" (*.wav *.mp3 *.ogg *.aif *.aiff *.m4a *.flac)") + QStringLiteral(";;");
        } else if (m_currentProvider->get()->type() == ProviderModel::VIDEO) {
            ext = i18n("Video") + QStringLiteral(" (*.mp4 *.webm *.mpg *.mov *.avi *.mkv)") + QStringLiteral(";;");
        } else if (m_currentProvider->get()->type() == ProviderModel::ProviderModel::IMAGE) {
            ext = i18n("Images") + QStringLiteral(" (*.png *.jpg *.jpeg *.svg") + QStringLiteral(";;");
        }
        ext.append(i18n("All Files") + QStringLiteral(" (*)"));
    }
    if (path.endsWith(QLatin1Char('/'))) {
        if (m_currentItem->data(nameRole).toString().isEmpty()) {
            path.append(i18n("Unnamed"));
        } else {
            path.append(m_currentItem->data(nameRole).toString());
        }
        path.append(srcUrl.fileName().section(QLatin1Char('.'), -1));
    }

    QString attribution;
    if (KMessageBox::questionTwoActions(this,
                                        i18n("Be aware that the usage of the resource is maybe restricted by license terms or law!\n"
                                             "Do you want to add license attribution to your Project Notes?"),
                                        QString(), KStandardGuiItem::add(), KGuiItem(i18nc("@action:button", "Continue without")),
                                        i18n("Remember this decision")) == KMessageBox::PrimaryAction) {
        attribution = i18nc("item name, item url, author name, license name, license url",
                            "This video uses \"%1\" (%2) by \"%3\" licensed under %4. To view a copy of this license, visit %5",
                            m_currentItem->data(nameRole).toString().isEmpty() ? i18n("Unnamed") : m_currentItem->data(nameRole).toString(),
                            m_currentItem->data(urlRole).toString(), m_currentItem->data(authorRole).toString(),
                            ResourceWidget::licenseNameFromUrl(m_currentItem->data(licenseRole).toString(), true), m_currentItem->data(licenseRole).toString());
        attribution.append(QStringLiteral("<br/> "));
    }

    QString saveUrlstring = QFileDialog::getSaveFileName(this, QString(), path, ext);

    // if user cancels save
    if (saveUrlstring.isEmpty()) {
        return;
    }

    saveUrl = QUrl::fromLocalFile(saveUrlstring);

    KIO::FileCopyJob *getJob = KIO::file_copy(srcUrl, saveUrl, -1, KIO::Overwrite);
    if (!accessToken.isEmpty()) {
        getJob->addMetaData("customHTTPHeader", QStringLiteral("Authorization: Bearer %1").arg(accessToken));
    }
    KFileItem info(srcUrl);
    getJob->setSourceSize(info.size());
    getJob->setProperty("attribution", attribution);
    getJob->setProperty("usedOAuth2", !accessToken.isEmpty());

    connect(getJob, &KJob::result, this, &ResourceWidget::slotGotFile);
    getJob->start();
}

/**
 * @brief ResourceWidget::slotGotFile
 * @param job
 * Finish the download by emitting addClip and if necessary addLicenseInfo
 * Enables the import button
 */
void ResourceWidget::slotGotFile(KJob *job)

{
    if (job->error() != 0) {
        const QString errTxt = job->errorString();
        if (job->property("usedOAuth2").toBool()) {
            KMessageBox::error(this, i18n("%1 Try again.", errTxt), i18n("Error Loading Data"));
        } else {
            KMessageBox::error(this, errTxt, i18n("Error Loading Data"));
        }
        qCDebug(KDENLIVE_LOG) << "//file import job errored: " << errTxt;
        return;
    }
    auto *copyJob = static_cast<KIO::FileCopyJob *>(job);
    const QUrl filePath = copyJob->destUrl();
    KRecentDirs::add(QStringLiteral(":KdenliveOnlineResourceFolder"), filePath.adjusted(QUrl::RemoveFilename).toLocalFile());

    KMessageBox::information(this, i18n("Resource saved to %1", filePath.toLocalFile()), i18n("Data Imported"));
    Q_EMIT addClip(filePath, QString());

    if (!copyJob->property("attribution").toString().isEmpty()) {
        Q_EMIT addLicenseInfo(copyJob->property("attribution").toString());
    }
}

/**
 * @brief ResourceWidget::slotAccessTokenReceived
 * @param accessToken - the access token obtained from the provider
 * Calls slotSaveItem or slotChooseVersion for auth protected files
 */
void ResourceWidget::slotAccessTokenReceived(const QString &accessToken)
{
    if (!accessToken.isEmpty()) {
        if (m_currentItem->data(singleDownloadRole).toBool()) {
            if (m_currentItem->data(downloadRole).toString().isEmpty()) {
                m_currentProvider->get()->slotFetchFiles(m_currentItem->data(idRole).toString());
                return;
            } else {
                slotSaveItem(QString(), accessToken);
            }
        } else {
            slotChooseVersion(m_currentItem->data(downloadRole).toStringList(), m_currentItem->data(downloadLabelRole).toStringList(), accessToken);
        }

    } else {
        KMessageBox::error(this, i18n("Try importing again to obtain a new connection"),
                           i18n("Error Getting Access Token from %1.", m_currentProvider->get()->name()));
    }
}
