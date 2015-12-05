/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
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

#include "resourcewidget.h"
#include "freesound.h"
#include "openclipart.h"
#include "archiveorg.h"
#include "kdenlivesettings.h"

#include <QPushButton>
#include <QListWidget>
#include <QAction>
#include <QMenu>
#include <QFileDialog>
#include <QNetworkConfigurationManager>
#include <QDebug>
#include <QFontDatabase>
#include <QTemporaryFile>

#include <KSharedConfig>
#include <klocalizedstring.h>
#include <kio/job.h>
#include <KIO/SimpleJob>
#include <KRun>
#include <KConfigGroup>
#include <KPixmapSequence>
#include <KPixmapSequenceOverlayPainter>
#include <KFileItem>
#include <KMessageBox>

ResourceWidget::ResourceWidget(const QString & folder, QWidget * parent) :
    QDialog(parent),
    m_folder(folder),
    m_currentService(NULL)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    m_tmpThumbFile = new QTemporaryFile;
    /*< is used to create unique temporary files safely.
      The file itself is created by calling open().
     *  The name of the temporary file is guaranteed to be unique
     * (i.e., you are guaranteed to not overwrite an existing file), and the file will subsequently be
     *  removed upon destruction of the QTemporaryFile object.
     * This is an important technique that avoids data corruption for applications that store data in temporary files.
     *  The file name is auto-generated with this form of the constuctor
     */

    service_list->addItem(i18n("Freesound Audio Library"), FREESOUND);
    service_list->addItem(i18n("Archive.org Video Library"), ARCHIVEORG);
    service_list->addItem(i18n("Open Clip Art Graphic Library"), OPENCLIPART);
    setWindowTitle(i18n("Search Online Resources"));
    QPalette p = palette();
    p.setBrush(QPalette::Base, p.window());
    info_browser->setPalette(p);
    connect(button_search, SIGNAL(clicked()), this, SLOT(slotStartSearch()));
    connect(search_results, SIGNAL(currentRowChanged(int)), this, SLOT(slotUpdateCurrentSound()));
    connect(button_preview, SIGNAL(clicked()), this, SLOT(slotPlaySound()));
    connect(button_import, SIGNAL(clicked()), this, SLOT(slotSaveItem()));
    connect(item_license, SIGNAL(leftClickedUrl(QString)), this, SLOT(slotOpenUrl(QString)));
    connect(service_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChangeService()));

    
    m_networkManager = new QNetworkConfigurationManager(this);
    if (!m_networkManager->isOnline()) {
        slotOnlineChanged(false);
    }
    connect(m_networkManager, SIGNAL(onlineStateChanged(bool)), this, SLOT(slotOnlineChanged(bool)));
    connect(page_next, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(page_prev, SIGNAL(clicked()), this, SLOT(slotPreviousPage()));
    connect(page_number, SIGNAL(valueChanged(int)), this, SLOT(slotStartSearch(int)));
    connect(info_browser, SIGNAL(anchorClicked(QUrl)), this, SLOT(slotOpenLink(QUrl)));
    
    m_autoPlay = new QAction(i18n("Auto Play"), this);
    m_autoPlay->setCheckable(true);
    QMenu *resourceMenu = new QMenu;
    resourceMenu->addAction(m_autoPlay);
    config_button->setMenu(resourceMenu);
    config_button->setIcon(QIcon::fromTheme(QStringLiteral("configure")));

    m_busyWidget = new KPixmapSequenceOverlayPainter(this);
    m_busyWidget->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_busyWidget->setWidget(search_results->viewport());
    
    sound_box->setEnabled(false);
    search_text->setFocus();
    slotChangeService();
    loadConfig();
}
/**
 * @brief ResourceWidget::~ResourceWidget
 */
ResourceWidget::~ResourceWidget()
{
    delete m_currentService;
    delete m_tmpThumbFile;
    saveConfig();
}
/**
 * @brief ResourceWidget::loadConfig
 */
void ResourceWidget::loadConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "ResourceWidget");
    QList<int> size;
    size << 100 << 400;
    splitter->setSizes(resourceConfig.readEntry( "mainSplitter", size));
}
/**
 * @brief ResourceWidget::saveConfig
 */
void ResourceWidget::saveConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "ResourceWidget");
    resourceConfig.writeEntry(QStringLiteral("mainsplitter"), splitter->size());
    config->sync();
}
/**
 * @brief ResourceWidget::slotStartSearch
 * @param page
 */
void ResourceWidget::slotStartSearch(int page)
{
    page_number->blockSignals(true);
    page_number->setValue(page);
    page_number->blockSignals(false);
    m_busyWidget->start();
    m_currentService->slotStartSearch(search_text->text(), page);
}
/**
 * @brief Fires when user selects a different item in the list of found items
 * This is not just for sounds. It fires for clip art and videos too.
 */
void ResourceWidget::slotUpdateCurrentSound()
{
    if (!m_autoPlay->isChecked()){
        m_currentService->stopItemPreview(NULL);
         button_preview->setText(i18n("Preview"));
    }
    item_license->clear();
    m_title.clear();
    m_image.clear();
    m_desc.clear();
    m_meta.clear();
    QListWidgetItem *item = search_results->currentItem();// get the item the user selected
    if (!item) {
        sound_box->setEnabled(false);// sound_box  is the group of objects in the Online resources window on the LHS with the
        // preview and import buttons and the details about the currently selected item.
        // if nothing is selected then we disable all these
        return;
    }
    m_currentInfo = m_currentService->displayItemDetails(item); // Not so much displaying the items details
                                                        // This getting the items details into m_currentInfo
    
    if (m_autoPlay->isChecked() && m_currentService->hasPreview)
        m_currentService->startItemPreview(item);
    sound_box->setEnabled(true);// enable the group with the preview and import buttons
    QString title = "<h3>" + m_currentInfo.itemName; // title is not just a title. It is a whole lot of HTML for displaying the
                                                    // the info for the current item.
                                                    // updateLayout() adds  m_image,m_desc and m_meta to the title to make up the html that displays on the widget
    if (!m_currentInfo.infoUrl.isEmpty())
        title += QStringLiteral(" (<a href=\"%1\">%2</a>)").arg(m_currentInfo.infoUrl).arg(i18nc("the url link pointing to a web page", "link"));
    title.append("</h3>");
    
    if (!m_currentInfo.authorUrl.isEmpty()) {
        title += QStringLiteral("<a href=\"%1\">").arg(m_currentInfo.authorUrl);
        if (!m_currentInfo.author.isEmpty())
            title.append(m_currentInfo.author);
        else title.append(i18n("Author"));
        title.append("</a><br />");
    }
    else if (!m_currentInfo.author.isEmpty())
        title.append(m_currentInfo.author + "<br />");
    else
        title.append("<br />");
    
    slotSetTitle(title);// updates the m_title var with the new HTML. Calls updateLayout()
    // that set upates the html in  info_browser
    if (!m_currentInfo.description.isEmpty()) slotSetDescription(m_currentInfo.description);
    if (!m_currentInfo.license.isEmpty()) parseLicense(m_currentInfo.license);
}

/**
 * @brief  Downloads thumbnail file from url and saves it to tmp directory - temporyFile
 *
 * The same temp file is recycled.
 * Loads the image into the ResourceWidget window.
 * Connected to signal AbstractService::GotThumb
 * */
void ResourceWidget::slotLoadThumb(const QString &url)
{
    QUrl img(url);
    if (img.isEmpty()) return;
    m_tmpThumbFile->close();
    if (m_tmpThumbFile->open()) {
        KIO::FileCopyJob *copyjob = KIO::file_copy(img, QUrl::fromLocalFile(m_tmpThumbFile->fileName()), -1, KIO::Overwrite);
        if (copyjob->exec()) {// This FileCopyJob is working 21Nov2015
            slotSetImage(m_tmpThumbFile->fileName());
        }
    }
}


/**
 * @brief Fires when meta info has been updated
 *
 * connected to gotMetaInfo(QMap) slot in each of the services classes (FreeSound, ArchiveOrg). Copies itemDownload
 * from metaInfo into m_currentInfo - used by FreeSound case because with new freesound API the
 * item download data is obtained from a secondary location and is populated into metaInfo
 */
void ResourceWidget::slotDisplayMetaInfo(const QMap<QString, QString> &metaInfo)
{
    if (metaInfo.contains(QStringLiteral("license"))) {
        parseLicense(metaInfo.value(QStringLiteral("license")));
    }
    if (metaInfo.contains(QStringLiteral("description"))) {
        slotSetDescription(metaInfo.value(QStringLiteral("description")));
    }
    if (metaInfo.contains(QLatin1String("itemDownload"))) {
     m_currentInfo.itemDownload=metaInfo.value(QLatin1String("itemDownload"));

    }
    if (metaInfo.contains(QLatin1String("itemPreview"))) {


        if (m_autoPlay->isChecked())
        {
               m_currentService->startItemPreview(search_results->currentItem());
                button_preview->setText(i18n("Stop"));
        }
    }

}

/**
 * @brief ResourceWidget::slotPlaySound
 * fires when button_preview is clicked. This button is clicked again to stop the preview
 */
void ResourceWidget::slotPlaySound()
{
    QString caption;
    QString sStop =i18n("Stop");
    QString sPreview = i18n("Preview");
    if (!m_currentService)
        return;
    caption= button_preview->text();
    if (caption.contains( sPreview))
    {
        const bool started = m_currentService->startItemPreview(search_results->currentItem());
        if (started)
            button_preview->setText(i18n("Stop"));
    }
    else
    {
        m_currentService->stopItemPreview(search_results->currentItem());

        button_preview->setText(i18n("Preview"));
    }
}

/**
 * @brief Fires when import button on the ResourceWidget is clicked and also called by slotOpenLink()
 *
 * Opens a dialog for user to choose a save location. Starts a file download job. Connects the job to slotGotFile()
 */
void ResourceWidget::slotSaveItem(const QString &originalUrl)
{
    QListWidgetItem *item = search_results->currentItem();
    if (!item) return;
    QString path = m_folder;
    QString ext;
    if (!path.endsWith('/')) path.append('/');
    if (!originalUrl.isEmpty()) {
        path.append(QUrl(originalUrl).fileName());
        ext = "*." + QUrl(originalUrl).fileName().section('.', -1);
        m_currentInfo.itemDownload = originalUrl;
    }
    else {
        path.append(m_currentService->getDefaultDownloadName(item));

        if(m_currentService->serviceType==FREESOUND)
        {  // intermim fix for FREESOUND - getting highquality .mp3 preview for files when download them
            ext ="Audio (*.mp3);;All Files(*.*)";
             //ext ="Images (*.png *.xpm *.jpg);;Text files (*.txt);;XML files (*.xml);;Audio(*.mp3)";
        }
        else if(m_currentService->serviceType==OPENCLIPART)
        {
            ext = "Images (" + m_currentService->getExtension(search_results->currentItem()) +");;All Files(*.*)";
        }
        else
        {
             ext = "Video (" + m_currentService->getExtension(search_results->currentItem()) +");;All Files(*.*)";
        }
    }
    QString saveUrlstring = QFileDialog::getSaveFileName(this, QString(), path, ext);
    QUrl srcUrl(m_currentInfo.itemDownload);

    QUrl saveUrl=QUrl::fromLocalFile ( saveUrlstring);

    if (saveUrl.isEmpty() ) //QFile::exists(srcUrl.path()) is always false because this does not work on files remotely hosted on http
        //So only check if the save  url is empty (ie if user cancels the save.
        //If the URL has no file at the end we trap this error in slotGotFile.
        return;

    if (QFile::exists(saveUrlstring))
    {
        int ret = QMessageBox::warning(this, i18n("File Exists"),
                                       i18n("Do you want to overwrite the existing file?."),
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);
        if (ret==QMessageBox::No)
            return;
    }
    button_import->setEnabled(false); // disable buttons while download runs. enabled in slotGotFile
    KIO::FileCopyJob * getJob = KIO::file_copy(srcUrl, QUrl(saveUrl), -1, KIO::Overwrite);
    KFileItem info(srcUrl);
    getJob->setSourceSize(info.size());
    getJob->setProperty("license", item_license->text());
    getJob->setProperty("licenseurl", item_license->url());
    getJob->setProperty("originurl", m_currentInfo.itemDownload);
    if (!m_currentInfo.authorUrl.isEmpty()) getJob->setProperty("author", m_currentInfo.authorUrl);
    else if (!m_currentInfo.author.isEmpty()) getJob->setProperty("author", m_currentInfo.author);
    connect(getJob, SIGNAL(result(KJob*)), this, SLOT(slotGotFile(KJob*)));

    getJob->start();
}

/**
 * @brief ResourceWidget::slotGotFile - fires when the file copy job started by  ResourceWidget::slotSaveItem() completes
 * emits addClip which causes clip to be added to the project bin.
 * @param job
 */
void ResourceWidget::slotGotFile(KJob *job)

{
    QString errTxt;
    button_import->setEnabled(true);
    if (job->error() )
    {

        errTxt  =job->errorString();
        KMessageBox::sorry(this, errTxt, i18n("Error Loading Data"));
        qDebug()<<"//file import job errored: "<<errTxt;
        return;
    }
    else
    {
        KIO::FileCopyJob* copyJob = static_cast<KIO::FileCopyJob*>( job );
        const QUrl filePath = copyJob->destUrl();

        KMessageBox::information(this, "Resource saved to " + filePath.toString(), i18n("Data Imported"));
        emit addClip(filePath);
    }
}


/**
 * @brief ResourceWidget::slotOpenUrl. Opens the file on the URL using the associated application via a KRun object
 *
 *
 * called by slotOpenLink() so this will open .html in the users associated browser
 * @param url
 */
void ResourceWidget::slotOpenUrl(const QString &url)
{
    new KRun(QUrl(url), this);
}
/**
 * @brief ResourceWidget::slotChangeService - fires when user changes what online resource they want to search agains via the dropdown list

  Also fires when widget first opens
*/
void ResourceWidget::slotChangeService()
{

    delete m_currentService;
    m_currentService = NULL;
    SERVICETYPE service = (SERVICETYPE) service_list->itemData(service_list->currentIndex()).toInt();
    if (service == FREESOUND) {
        m_currentService = new FreeSound(search_results);
    } else if (service == OPENCLIPART) {
        m_currentService = new OpenClipArt(search_results);
    } else if (service == ARCHIVEORG) {
        m_currentService = new ArchiveOrg(search_results);
    } else return;

    connect(m_currentService, SIGNAL(gotMetaInfo(QString)), this, SLOT(slotSetMetadata(QString)));
    connect(m_currentService, SIGNAL(gotMetaInfo(QMap<QString,QString>)), this, SLOT(slotDisplayMetaInfo(QMap<QString,QString>)));
    connect(m_currentService, SIGNAL(maxPages(int)), this, SLOT(slotSetMaximum(int)));
    connect(m_currentService, SIGNAL(searchInfo(QString)), search_info, SLOT(setText(QString)));
    connect(m_currentService, SIGNAL(gotThumb(QString)), this, SLOT(slotLoadThumb(QString)));
    connect(m_currentService, SIGNAL(searchDone()), m_busyWidget, SLOT(stop()));
    connect (m_currentService,SIGNAL(previewFinished()),this, SLOT(slotPreviewFinished()));
    
    button_preview->setVisible(m_currentService->hasPreview);
    button_import->setVisible(!m_currentService->inlineDownload);
    search_info->setText(QString());
    if (!search_text->text().isEmpty())
        slotStartSearch();
}
/**
 * @brief ResourceWidget::slotSetMaximum
 * @param max
 */
void ResourceWidget::slotSetMaximum(int max)
{
    page_number->setMaximum(max);
}
/**
 * @brief ResourceWidget::slotOnlineChanged
 * @param online
 */
void ResourceWidget::slotOnlineChanged(bool online)
{
    
    button_search->setEnabled(online);
    search_info->setText(online ? QString() : i18n("You need to be online\n for searching"));
}
/**
 * @brief ResourceWidget::slotNextPage
 */
void ResourceWidget::slotNextPage()
{
    const int ix = page_number->value();
    if (search_results->count() > 0)
        page_number->setValue(ix + 1);
}
/**
 * @brief ResourceWidget::slotPreviousPage
 */
void ResourceWidget::slotPreviousPage()
{
    const int ix = page_number->value();
    if (ix > 1)
        page_number->setValue(ix - 1);
}
/**
 * @brief ResourceWidget::parseLicense provides a name for the licence based on the license URL
 * called by slotDisplayMetaInfo and by slotUpdateCurrentSound
 * @param licenseUrl
 */
void ResourceWidget::parseLicense(const QString &licenseUrl)
{
    QString licenseName;
    if (licenseUrl.contains(QStringLiteral("/sampling+/")))
        licenseName = QStringLiteral("Sampling+");
    else if (licenseUrl.contains(QStringLiteral("/by/")))
        licenseName = QStringLiteral("Attribution");
    else if (licenseUrl.contains(QStringLiteral("/by-nd/")))
        licenseName = QStringLiteral("Attribution-NoDerivs");
    else if (licenseUrl.contains(QStringLiteral("/by-nc-sa/")))
        licenseName = QStringLiteral("Attribution-NonCommercial-ShareAlike");
    else if (licenseUrl.contains(QStringLiteral("/by-sa/")))
        licenseName = QStringLiteral("Attribution-ShareAlike");
    else if (licenseUrl.contains(QStringLiteral("/by-nc/")))
        licenseName = QStringLiteral("Attribution-NonCommercial");
    else if (licenseUrl.contains(QStringLiteral("/by-nc-nd/")))
        licenseName = QStringLiteral("Attribution-NonCommercial-NoDerivs");
    else if (licenseUrl.contains(QStringLiteral("/publicdomain/zero/")))
        licenseName = QStringLiteral("Creative Commons 0");
    else if (licenseUrl.endsWith(QLatin1String("/publicdomain")) || licenseUrl.contains(QLatin1String("openclipart.org/share")))
        licenseName = QStringLiteral("Public Domain");
    else licenseName = i18n("Unknown");
    item_license->setText(licenseName);
    item_license->setUrl(licenseUrl);
}


/**
 * @brief ResourceWidget::slotOpenLink. Fires when Url in the resource wizard is clicked
 * @param url
 * Connected to anchorClicked(). If the url ends in _import it downloads the file at the end of the url via slotSaveItem()
 * Otherwise it opens the URL via slotOpenUrl()
 */
void ResourceWidget::slotOpenLink(const QUrl &url)
{
    QString path = url.toEncoded();
    if (path.endsWith(QLatin1String("_import"))) {
        path.chop(7);
        // import file in Kdenlive
        slotSaveItem(path);
    }
    else {
        slotOpenUrl(path);
    }
}
/**
 * @brief ResourceWidget::slotSetDescription
 * @param desc
 */
void ResourceWidget::slotSetDescription(const QString &desc)
{
    if(m_desc != desc) {
        m_desc = desc;
        updateLayout();
    }
}
/**
 * @brief ResourceWidget::slotSetMetadata
 * @param desc
 * This is called when gotMetaInfo(Qstring) signal fires. That signal is passing html in the parameter
 * This function is updating the html (m_meta) in the ResourceWidget and then calls  updateLayout()
 * which updates actual widget
 */
void ResourceWidget::slotSetMetadata(const QString &desc)
{
    if (m_meta != desc) {
        m_meta = desc;
        updateLayout();
    }
}
/**
 * @brief ResourceWidget::slotSetImage
 * @param desc
 */
void ResourceWidget::slotSetImage(const QString &desc)
{
    m_image = QStringLiteral("<img src=\"%1\" style=\"max-height:150px\" max-width=\"%2px\" />").arg(desc).arg((int) (info_browser->width() * 0.9));
    updateLayout();
}
/** @brief updates the display with infomation on the seleted item
 *
 * Called by ResourceWidget::slotUpdateCurrentSound()
*/
void ResourceWidget::slotSetTitle(const QString &desc)
{
    if (m_title != desc) {
        m_title = desc;
        updateLayout();
    }
}
/**
 * @brief ResourceWidget::updateLayout
 * This concats the html in m_title, m_image,m_desc and m_meta and sets the resulting
 * html markup into the content of the ResourceWidget \n
 * Called by slotSetTitle() , slotSetMetadata() ,slotSetDescription() ,slotSetImage()
 */
void ResourceWidget::updateLayout()
{
    QString content = m_title;
    if (!m_image.isEmpty())
        content.append(m_image + "<br clear=\"all\" />");
    if (!m_desc.isEmpty())
        content.append(m_desc);
    if (!m_meta.isEmpty())
        content.append(m_meta);
    info_browser->setHtml(content);
}
/**
 * @brief ResourceWidget::slotPreviewFinished
 * connected to FreeSound previewFinished signal
 */
void  ResourceWidget::slotPreviewFinished()
{
    button_preview->setText(i18n("Preview"));
}


