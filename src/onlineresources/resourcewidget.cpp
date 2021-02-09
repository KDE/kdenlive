#include "resourcewidget.hpp"
#include "core.h"
#include "kdenlivesettings.h"

#include <klocalizedstring.h>
//#include <QGridLayout>
#include <KSqueezedTextLabel>
#include <QFileDialog>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMenu>
#include <QProgressDialog>
#include <QToolBar>
#include <QComboBox>
//#include <QPixmap>
//#include <QNetworkAccessManager>
#include <KFileItem>
#include <KMessageBox>
#include <KRecentDirs>
#include <KRun>
#include <KSelectAction>

#ifdef QT5_USE_WEBENGINE
#include "qt-oauth-lib/oauth2.h"
#endif

ResourceWidget::ResourceWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentProvider(nullptr)
    , m_currentItem(nullptr)
    , m_pOAuth2(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    m_tmpThumbFile = new QTemporaryFile;

    int iconHeight = QFontInfo(font()).pixelSize() * 3.5;
    m_iconSize = QSize(iconHeight * pCore->getCurrentDar(), iconHeight);

    // Zoom slider
    //QWidget *container = new QWidget(this);
    //auto *lay = new QHBoxLayout;
    //m_slider = new QSlider(Qt::Horizontal, this);
    //slider_zoom->setMaximumWidth(100);
    //slider_zoom->setMinimumWidth(40);
    slider_zoom->setRange(0, 15);
    connect(slider_zoom, &QAbstractSlider::valueChanged, this, &ResourceWidget::slotSetIconSize);
    //auto *tb1 = new QToolButton(this);
    //tb1->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    connect(button_zoomin, &QToolButton::clicked, this, [&]() { slider_zoom->setValue(qMin(slider_zoom->value() + 1, slider_zoom->maximum())); });
    //auto *tb2 = new QToolButton(this);
    //tb2->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    connect(button_zoomout, &QToolButton::clicked, this, [&]() { slider_zoom->setValue(qMax(slider_zoom->value() - 1, slider_zoom->minimum())); });
    //lay->addWidget(tb2);
    //lay->addWidget(m_slider);
    //lay->addWidget(tb1);
    //container->setLayout(lay);
    //auto *widgetslider = new QWidgetAction(this);
    //widgetslider->setDefaultWidget(container);

    // Settings menu
    //QMenu *settingsMenu = new QMenu(i18n("Settings"), this);
    // View type
    /*KSelectAction *listType = new KSelectAction(QIcon::fromTheme(QStringLiteral("view-list-tree")), i18n("View Mode"), this);
    //pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode"), listType);
    QAction *treeViewAction = listType->addAction(QIcon::fromTheme(QStringLiteral("view-list-tree")), i18n("Tree View"));
    listType->addAction(treeViewAction);
    treeViewAction->setData(BinTreeView);
    if (m_listType == treeViewAction->data().toInt()) {
        listType->setCurrentAction(treeViewAction);
    }
    //pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode_tree"), treeViewAction);

    QAction *iconViewAction = listType->addAction(QIcon::fromTheme(QStringLiteral("view-list-icons")), i18n("Icon View"));
    iconViewAction->setData(BinIconView);
    if (m_listType == iconViewAction->data().toInt()) {
        listType->setCurrentAction(iconViewAction);
    }
    listType->setToolBarMode(KSelectAction::MenuMode);
    connect(listType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &Bin::slotInitView);
    settingsMenu->addAction(listType);*/
    //QMenu *sliderMenu = new QMenu(i18n("Zoom"), this);
    //sliderMenu->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    //sliderMenu->addAction(widgetslider);
    //settingsMenu->addMenu(sliderMenu);
    //settingsMenu->addMenu(sort);

    //button->setToolTip(i18n("Options"));
    //button_menu->setMenu(settingsMenu);
    //button_menu->setPopupMode(QToolButton::InstantPopup);

    message_line->close();

    for (QPair<QString, QString> provider : ProvidersRepository::get()->getAllProviers()) {
        service_list->addItem(provider.first, provider.second);
    }
    connect(service_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChangeProvider()));
    connect(provider_info, SIGNAL(leftClickedUrl(const QString&)), this, SLOT(slotOpenUrl(const QString&)));
    connect(button_search, &QAbstractButton::clicked, this, &ResourceWidget::slotStartSearch);
    connect(search_text, SIGNAL(returnPressed()), this, SLOT(slotStartSearch()));
    connect(search_results, &QListWidget::currentRowChanged, this, &ResourceWidget::slotUpdateCurrentItem);
    connect(button_preview, &QAbstractButton::clicked, this, [&](){
            if (!m_currentProvider) {
                qDebug() << "No current Provider -> no Preview.";
                return;
            }

            slotPreviewItem();
        });

    connect(button_import, &QAbstractButton::clicked, this, [&](){
        if(m_currentProvider->get()->downloadOAuth2()) {
#ifdef QT5_USE_WEBENGINE
            KMessageBox::error(this, "This download requires authenfication with OAuth2. Please install QtWebEngine and rebuild kdenlive to use this feature.");
            m_pOAuth2->obtainAccessToken();
#else
            KMessageBox::error(this, "This download requires authenfication with OAuth2. Please install QtWebEngine and rebuild kdenlive to use this feature.");
#endif
        } else {
            if(m_currentItem->data(singleDownloadRole).toBool()) {
                if(m_currentItem->data(downloadRole).toString().isEmpty()) {
                    m_currentProvider->get()->slotFetchFiles(m_currentItem->data(idRole).toString());
                    return;
                } else {
                    slotSaveItem();
                }
            } else {
                slotChooseVersion(m_currentItem->data(downloadRole).toStringList(),m_currentItem->data(downloadLabelRole).toStringList());
            }
        }

    });

    page_number->setEnabled(false);

    connect(page_number, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ResourceWidget::slotStartSearch);

    #ifdef QT5_USE_WEBENGINE
        m_pOAuth2 = new OAuth2(this);
        connect(m_pOAuth2, &OAuth2::accessTokenReceived, this, &ResourceWidget::slotAccessTokenReceived);
        connect(m_pOAuth2, &OAuth2::accessDenied, this, [&](){
            button_import->setEnabled(true);
            KMessageBox::sorry(this, i18n("Access Denied from Provider. Have you authorised the Kdenlive application on your account?"));

        });
        connect(m_pOAuth2, &OAuth2::UsePreview, this, [&](){
            slotSaveItem(m_currentItem->data(previewRole).toString());
        });
        connect(m_pOAuth2, &OAuth2::Canceled, this, [&](){
            button_import->setEnabled(true);
        });
    #endif
    //slotChangeProvider();
    loadConfig();
    adjustSize();
}

ResourceWidget::~ResourceWidget()
{
    saveConfig();
    delete m_currentProvider;
    delete m_tmpThumbFile;
    delete m_pOAuth2;
    //delete m_networkAccessManager;
}

void ResourceWidget::loadConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "OnlineResources");
    service_list->setCurrentItem(resourceConfig.readEntry("provider", service_list->itemText(0)));
    qDebug() << "Current Data: " << service_list->currentData();
    slider_zoom->setValue(resourceConfig.readEntry("zoom", 7));
}
/**
 * @brief ResourceWidget::saveConfig
 */
void ResourceWidget::saveConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "OnlineResources");
    resourceConfig.writeEntry(QStringLiteral("provider"), service_list->currentText());
    resourceConfig.writeEntry(QStringLiteral("zoom"), slider_zoom->value());
    config->sync();
}

void ResourceWidget::blockUI(bool block)
{
    browser->setEnabled(!block);
    buildin_box->setEnabled(!block);
    search_box->setEnabled(!block);
    service_list->setEnabled(!block);
    setCursor(block ? Qt::WaitCursor : Qt::ArrowCursor);
}

/*void ResourceWidget::setMonitor(Monitor *monitor)
{
    m_monitor = monitor;
    connect(m_monitor, &Monitor::addClipToProject, this, &Bin::slotAddClipToProject);
    connect(m_monitor, &Monitor::refreshCurrentClip, this, &Bin::slotOpenCurrent);
    connect(this, &Bin::openClip, [&](std::shared_ptr<ProjectClip> clip, int in, int out) {
        m_monitor->slotOpenClip(clip, in, out);
    });
}*/

bool ResourceWidget::eventFilter(QObject *obj, QEvent *event)
{
    //if (obj == search_results) {
        if (event->type() == QEvent::Wheel) {
            auto *e = static_cast<QWheelEvent *>(event);
            if ((e != nullptr) && e->modifiers() == Qt::ControlModifier) {
                wheelAccumulatedDelta += e->angleDelta().y();
                if (abs(wheelAccumulatedDelta) >= QWheelEvent::DefaultDeltasPerStep) {
                    slotZoomView(wheelAccumulatedDelta > 0);
                }
                // emit zoomView(e->delta() > 0);
                return true;
            }
        }
    //}

    return QWidget::eventFilter(obj, event);
}

void ResourceWidget::slotChangeProvider()
{
    if(m_currentProvider != nullptr) {
        m_currentProvider->get()->disconnect(this);
    }
    m_currentProvider = &ProvidersRepository::get()->getProvider(service_list->currentData().toString());

    if(m_currentProvider->get()->integratonType() == ProviderModel::BROWSER) {
        buildin_box->hide();
        search_box->hide();
        browser->show();
        browser->setUrl(QUrl(m_currentProvider->get()->homepage()));
    } else {
        browser->hide();
        buildin_box->show();
        search_box->show();

        provider_info->setText(i18n("Media provided by %1", m_currentProvider->get()->name()));
        provider_info->setUrl(m_currentProvider->get()->homepage());
        connect(m_currentProvider->get(), &ProviderModel::searchDone, this, &ResourceWidget::slotSearchFinished);
        connect(m_currentProvider->get(), &ProviderModel::searchError, this, [&](const QString &msg){
            message_line->setMessageType(KMessageWidget::Error);
            message_line->setText(i18n("Search failed! %1", msg));
            message_line->show();
            page_number->setEnabled(false);
            service_list->setEnabled(true);
            buildin_box->setEnabled(true);
            setCursor(Qt::ArrowCursor);
        });
        connect(m_currentProvider->get(), &ProviderModel::fetchedFiles, this, &ResourceWidget::slotChooseVersion);

        //provider_info
        details_box->setEnabled(false);
        info_browser->clear();
        search_results->clear();
        page_number->blockSignals(true);
        page_number->setValue(1);
        page_number->setMaximum(1);
        page_number->blockSignals(false);

        saveConfig();
        //search_text->setText(QString());
        if (!search_text->text().isEmpty()) {
            slotStartSearch(); // automatically kick of a search if we have search text and we switch services.
        }
    }
}

void ResourceWidget::slotOpenUrl(const QString &url)
{
    new KRun(QUrl(url), this);
}

/**
 * @brief ResourceWidget::slotStartSearch
 * @param page
 * connected to the button_search clicked signal in ResourceWidget constructor
 * also connected to the page_number value changed signal in ResourceWidget constructor
 * Calls slotStartSearch on the object for the currently selected service.
 */
void ResourceWidget::slotStartSearch()
{
    message_line->setMessageType(KMessageWidget::Information);
    message_line->show();
    message_line->setText(i18n("Search pending..."));

    blockUI(true);
    details_box->setEnabled(false);
    info_browser->clear();
    search_results->clear();
    m_currentProvider->get()->slotStartSearch(search_text->text(), page_number->value());
}

void ResourceWidget::slotSearchFinished(QList<ResourceItemInfo> &list, const int pageCount) {

    message_line->setMessageType(KMessageWidget::Information);
    message_line->show();
    int count = 0;
    for (const ResourceItemInfo &item: qAsConst(list)) {
        message_line->setText(i18n("Parsing item %1 of %2...", count, list.count()));
        //if item has no name use "Created by Author", if item even has no author use "Unnamed"
        QListWidgetItem *listItem = new QListWidgetItem(item.name.isEmpty() ? (item.author.isEmpty() ? i18n("Unnamed") : i18n("Created by %1", item.author)) : item.name);
        if(!item.imageUrl.isEmpty()) {
            QUrl img(item.imageUrl);
            if (img.isEmpty()) {
                return;
            }
            m_tmpThumbFile->close();
            if (m_tmpThumbFile->open()) {
                KIO::FileCopyJob *copyjob = KIO::file_copy(img, QUrl::fromLocalFile(m_tmpThumbFile->fileName()), -1, KIO::HideProgressInfo | KIO::Overwrite);
                if(copyjob->exec()) {
                    QPixmap pic(m_tmpThumbFile->fileName());
                    //GifLabel->setPixmap(pic); // pass a pointer as a parameter. Display the pic in our label
                    //slotSetImage(m_tmpThumbFile->fileName());
                    listItem->setIcon(pic);
                }
            }
        }

        listItem->setData(idRole,item.id);
        listItem->setData(nameRole, item.name);
        listItem->setData(filetypeRole, item.filetype);
        listItem->setData(descriptionRole,item.description);
        listItem->setData(imageRole,item.imageUrl);
        listItem->setData(previewRole, item.previewUrl);
        listItem->setData(authorUrl, item.authorUrl);
        listItem->setData(authorRole, item.author);
        listItem->setData(widthRole, item.width);
        listItem->setData(heightRole, item.height);
        listItem->setData(durationRole, item.duration);
        listItem->setData(urlRole, item.infoUrl);
        listItem->setData(licenseRole, item.license);
        if(item.downloadUrl.isEmpty() && item.downloadUrls.length() > 0) {
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
    page_number->setMaximum(pageCount);
    page_number->setEnabled(true);
    message_line->close();
    blockUI(false);
}

/**
 * @brief ResourceWidget::slotUpdateCurrentSound - Fires when user selects a different item in the list of found items

 * This is not just for sounds. It fires for clip art and videos too.
 */
void ResourceWidget::slotUpdateCurrentItem()
{
    details_box->setEnabled(false);

    m_currentItem = search_results->currentItem(); // get the item the user selected
    if (!m_currentItem) {
        return;
    }

    if(m_currentProvider->get()->type() != ProviderModel::IMAGE && !m_currentItem->data(previewRole).toString().isEmpty()) {
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

    if (!m_currentItem->data(licenseRole).toString().isEmpty()) {
        label_license->setText(m_currentItem->data(licenseRole).toString()); //TODO
    }

    details_box->setEnabled(true);
}

/**
 * @brief ResourceWidget::licenseNameFromUrl returns a name for the licence based on the license URL
 * @param licenseUrl
 * @param shortName wether the like "Attribution-NonCommercial-ShareAlike 3.0" or the short name like "CC BY-ND-SA 3.0" should be returned
 * @return the license name
 */
QString ResourceWidget::licenseNameFromUrl(const QString &licenseUrl, const bool shortName)
{
    QString licenseName;
    QString licenseShortName;

    // TODO translate them ?
    if (licenseUrl.contains(QStringLiteral("/sampling+/"))) {
        licenseName = QStringLiteral("Sampling+");
    } else if (licenseUrl.contains(QStringLiteral("/by/"))) {
        licenseName = QStringLiteral("Attribution");
        licenseShortName = QStringLiteral("CC BY");
    } else if (licenseUrl.contains(QStringLiteral("/by-nd/"))) {
        licenseName = QStringLiteral("Attribution-NoDerivs");
        licenseShortName = QStringLiteral("CC BY-ND");
    } else if (licenseUrl.contains(QStringLiteral("/by-nc-sa/"))) {
        licenseName = QStringLiteral("Attribution-NonCommercial-ShareAlike");
        licenseShortName = QStringLiteral("CC BY-ND-SA");
    } else if (licenseUrl.contains(QStringLiteral("/by-sa/"))) {
        licenseName = QStringLiteral("Attribution-ShareAlike");
        licenseShortName = QStringLiteral("CC BY-SA");
    } else if (licenseUrl.contains(QStringLiteral("/by-nc/"))) {
        licenseName = QStringLiteral("Attribution-NonCommercial");
        licenseShortName = QStringLiteral("CC BY-NC");
    } else if (licenseUrl.contains(QStringLiteral("/by-nc-nd/"))) {
        licenseName = QStringLiteral("Attribution-NonCommercial-NoDerivs");
        licenseShortName = QStringLiteral("CC BY-NC-ND");
    } else if (licenseUrl.contains(QLatin1String("/publicdomain/zero/"))) {
        licenseName = QStringLiteral("Creative Commons 0");
        licenseShortName = QStringLiteral("CC 0");
    } else if (licenseUrl.endsWith(QLatin1String("/publicdomain")) || licenseUrl.contains(QLatin1String("openclipart.org/share"))) {
        licenseName = QStringLiteral("Public Domain");
    } else {
        licenseName = i18n("Unknown");
        licenseShortName = i18n("Unknown");
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

    if(shortName) {
        return licenseShortName;
    } else {
        return licenseName;
    }
}

void ResourceWidget::slotSetIconSize(int size)
{
    if (!search_results) {
        return;
    }
    //KdenliveSettings::setBin_zoom(size); //TODO
    QSize zoom = m_iconSize;
    zoom = zoom * (size / 4.0);
    search_results->setIconSize(zoom);
}

void ResourceWidget::slotZoomView(bool zoomIn)
{
    wheelAccumulatedDelta = 0;
    if (search_results->count() == 0) {
        // Don't zoom on empty bin
        return;
    }
    int progress = (zoomIn) ? 1 : -1;
    slider_zoom->setValue(slider_zoom->value() + progress);
}

/**
 * @brief ResourceWidget::slotPlaySound
 * This slot starts the preview
 */
void ResourceWidget::slotPreviewItem()
{
    if (!m_currentItem) {
        return;
    }

    blockUI(true);
    emit previewClip(m_currentItem->data(previewRole).toString());
    blockUI(false);
}

void ResourceWidget::slotChooseVersion(const QStringList &urls, const QStringList &labels, const QString &accessToken) {
    if(urls.isEmpty() || labels.isEmpty()) {
        return;
    }
    if(urls.length() == 1) {
        slotSaveItem(urls.first(), accessToken);
        return;
    }
    bool ok;
    QString name = QInputDialog::getItem(this, i18n("Choose File Version"),QString(), labels, 0, false, &ok);
    if(!ok || name.isEmpty()) {
        return;
    }
    QString url = urls.at(labels.indexOf(name));
    slotSaveItem(url, accessToken);
}


/**
 * @brief Fires when import button on the ResourceWidget is clicked and also called by slotOpenLink()
 *
 * Opens a dialog for user to choose a save location.
 * If not freesound Starts a file download job and Connects the job to slotGotFile().
   If is freesound  creates an OAuth2 object to connect to freesounds oauth2 authentication.
  The  OAuth2 object is connected to a number of slots including  ResourceWidget::slotAccessTokenReceived
  Calls OAuth2::obtainAccessToken to kick off the freesound authentication
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
    QString sFileExt;

    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    if (!path.endsWith(QLatin1Char('/'))) {
        path.append(QLatin1Char('/'));
    }
    if (!srcUrl.fileName().isEmpty()) {
        path.append(srcUrl.fileName());
        ext = "*." + srcUrl.fileName().section(QLatin1Char('.'), -1);
    } else if(!m_currentItem->data(filetypeRole).toString().isEmpty()) {
        ext = "*." + m_currentItem->data(filetypeRole).toString();
    } else {
        if (m_currentProvider->get()->type() == ProviderModel::AUDIO) {
            ext = i18n("Audio") + QStringLiteral(" (*.wav *.mp3 *.ogg *.aif *.aiff *.m4a *.flac)") + QStringLiteral(";;");
        } else if (m_currentProvider->get()->type() == ProviderModel::VIDEO) {
            ext = i18n("Video") + QStringLiteral(" (*.mp4 *.webm *.mpg *.mov *.avi *.mkv)") + QStringLiteral(";;");
        } else if (m_currentProvider->get()->type() == ProviderModel:: ProviderModel::IMAGE) {
            ext = i18n("Images") + QStringLiteral(" (*.png *.jpg *.jpeg *.svg") + QStringLiteral(";;");
        }
        ext.append(i18n("All Files") + QStringLiteral(" (*.*)"));
    }
    if (path.endsWith(QLatin1Char('/'))) {
        if(m_currentItem->data(nameRole).toString().isEmpty()) {
            path.append(i18n("Unnamed"));
        } else {
            path.append(m_currentItem->data(nameRole).toString());
        }
        path.append(srcUrl.fileName().section(QLatin1Char('.'), -1));
    }

    QString attribution;
    if(KMessageBox::questionYesNo(this,
                                  i18n("Be aware that the usage of the resource is maybe limited by license terms or law!\n"
                                       "Do you want to add license attribution to your Project Notes?")) == KMessageBox::Yes) {
        attribution = i18n("This is a placeholder for the attribution");
    }

    QString saveUrlstring = QFileDialog::getSaveFileName(this, QString(), path, ext);

    if (saveUrlstring.isEmpty()) { // user canceled save
        return;
    }

    saveUrl = QUrl::fromLocalFile(saveUrlstring);

    button_import->setEnabled(false); // disable buttons while download runs. enabled in slotGotFile

    KIO::FileCopyJob *getJob = KIO::file_copy(srcUrl, saveUrl, -1, KIO::Overwrite);
    if(!accessToken.isEmpty()){
        getJob->addMetaData("customHTTPHeader", QByteArray("Authorization: Bearer ").append(accessToken.toUtf8()));
    }
    KFileItem info(srcUrl);
    getJob->setSourceSize(info.size());
    getJob->setProperty("attribution", attribution);
    getJob->setProperty("usedOAuth2", !accessToken.isEmpty());
    const QString licenseText = i18n("This video uses \"%1\" (%2) by \"%3\" licensed under %4. To view a copy of this license, visit %5",
                                     m_currentItem->data(filetypeRole).toString().isEmpty() ? i18n("Unnamed") : m_currentItem->data(nameRole).toString(),
                                     m_currentItem->data(urlRole).toString().isEmpty(),
                                     m_currentItem->data(authorRole).toString().isEmpty(),
                                     ResourceWidget::licenseNameFromUrl(m_currentItem->data(licenseRole).toString(), true),
                                     m_currentItem->data(licenseRole).toString().isEmpty());

    connect(getJob, &KJob::result, this, &ResourceWidget::slotGotFile);
    getJob->start();
    button_import->setEnabled(true);
}

/**
 * @brief ResourceWidget::slotGotFile - fires when the file copy job started by  ResourceWidget::slotSaveItem() completes
 * emits addClip which causes clip to be added to the project bin.
 * Enables the import button
 * @param job
 */
void ResourceWidget::slotGotFile(KJob *job)

{
    if (job->error() != 0) {
        const QString errTxt = job->errorString();
#ifdef QT5_USE_WEBENGINE
        if(job->property("usedOAuth2").toBool()) {
            m_pOAuth2->ForgetAccessToken();
            KMessageBox::sorry(this, errTxt + " " + i18n("Try again."), i18n("Error Loading Data"));
        } else {
#endif
            KMessageBox::sorry(this, errTxt, i18n("Error Loading Data"));
#ifdef QT5_USE_WEBENGINE
        }
#endif
        qCDebug(KDENLIVE_LOG) << "//file import job errored: " << errTxt;
        return;
    }
    auto *copyJob = static_cast<KIO::FileCopyJob *>(job);
    const QUrl filePath = copyJob->destUrl();
    KRecentDirs::add(QStringLiteral(":KdenliveOnlineResourceFolder"), filePath.adjusted(QUrl::RemoveFilename).toLocalFile());

    KMessageBox::information(this, i18n("Resource saved to %1", filePath.toLocalFile()), i18n("Data Imported"));
    emit addClip(filePath, QString());
    if(!copyJob->property("attribution").toString().isEmpty()) {
        emit addLicenseInfo(copyJob->property("attribution").toString());
    }
}

/**
 * @brief ResourceWidget::slotAccessTokenReceived
 * @param sAccessToken - the access token obtained from freesound website \n
 * Connected to OAuth2::accessTokenReceived signal in ResourceWidget constructor.
 * Fires when the OAuth2 object has obtained an access token. This slot then goes ahead
 * and starts the download of the requested file. ResourceWidget::DownloadRequestFinished will be
 * notified when that job finishes
 */
void ResourceWidget::slotAccessTokenReceived(const QString &accessToken)
{
    // qCDebug(KDENLIVE_LOG) << "slotAccessTokenReceived: " <<sAccessToken;
    if (!accessToken.isEmpty()) {
        if(m_currentItem->data(singleDownloadRole).toBool()) {
            if(m_currentItem->data(downloadRole).toString().isEmpty()) {
                m_currentProvider->get()->slotFetchFiles(m_currentItem->data(idRole).toString());
                return;
            } else {
                slotSaveItem(QString(), accessToken);
            }
        } else {
            slotChooseVersion(m_currentItem->data(downloadRole).toStringList(),m_currentItem->data(downloadLabelRole).toStringList(), accessToken);
        }

    } else {
        KMessageBox::error(this, i18n("Try importing again to obtain a new connection"), i18n("Error Getting Access Token from %1.",m_currentProvider->get()->name()));
    }
}
