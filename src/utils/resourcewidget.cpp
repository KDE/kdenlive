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

#include <QPushButton>
#include <QSpinBox>
#include <QListWidget>
#include <QAction>
#include <QMenu>

#include <KDebug>
#include <kdeversion.h>
#include "kdenlivesettings.h"
#include <KGlobalSettings>
#include <KMessageBox>
#include <KFileDialog>
#include <kio/job.h>
#include <KIO/NetAccess>
#include <Solid/Networking>
#include <KRun>
#if KDE_IS_VERSION(4,4,0)
#include <KPixmapSequence>
#include <KPixmapSequenceOverlayPainter>
#endif
#include <KFileItem>

#ifdef USE_NEPOMUK
#if KDE_IS_VERSION(4,6,0)
#include <Nepomuk/Variant>
#include <Nepomuk/Resource>
#include <Nepomuk/ResourceManager>
#include <Nepomuk/Vocabulary/NIE>
#include <Nepomuk/Vocabulary/NCO>
#include <Nepomuk/Vocabulary/NDO>
#endif
#endif

ResourceWidget::ResourceWidget(const QString & folder, QWidget * parent) :
        QDialog(parent),
        m_folder(folder),
        m_currentService(NULL)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
#ifdef USE_QJSON
    service_list->addItem(i18n("Freesound Audio Library"), FREESOUND);
    service_list->addItem(i18n("Archive.org Video Library"), ARCHIVEORG);
#endif
    service_list->addItem(i18n("Open Clip Art Graphic Library"), OPENCLIPART);
    setWindowTitle(i18n("Search Online Resources"));
    info_browser->setStyleSheet(QString("QTextBrowser { background-color: transparent;}"));
    connect(button_search, SIGNAL(clicked()), this, SLOT(slotStartSearch()));
    connect(search_results, SIGNAL(currentRowChanged(int)), this, SLOT(slotUpdateCurrentSound()));
    connect(button_preview, SIGNAL(clicked()), this, SLOT(slotPlaySound()));
    connect(button_import, SIGNAL(clicked()), this, SLOT(slotSaveItem()));
    connect(item_license, SIGNAL(leftClickedUrl(const QString &)), this, SLOT(slotOpenUrl(const QString &)));
    connect(service_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChangeService()));
    if (Solid::Networking::status() == Solid::Networking::Unconnected) {
        slotOffline();
    }
    connect(Solid::Networking::notifier(), SIGNAL(shouldConnect()), this, SLOT(slotOnline()));
    connect(Solid::Networking::notifier(), SIGNAL(shouldDisconnect()), this, SLOT(slotOffline()));
    connect(page_next, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(page_prev, SIGNAL(clicked()), this, SLOT(slotPreviousPage()));
    connect(page_number, SIGNAL(valueChanged(int)), this, SLOT(slotStartSearch(int)));
    connect(info_browser, SIGNAL(anchorClicked(const QUrl &)), this, SLOT(slotOpenLink(const QUrl &)));
    
    m_autoPlay = new QAction(i18n("Auto Play"), this);
    m_autoPlay->setCheckable(true);
    QMenu *resourceMenu = new QMenu;
    resourceMenu->addAction(m_autoPlay);
    config_button->setMenu(resourceMenu);
    config_button->setIcon(KIcon("configure"));

#if KDE_IS_VERSION(4,4,0)
    m_busyWidget = new KPixmapSequenceOverlayPainter(this);
    m_busyWidget->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_busyWidget->setWidget(search_results->viewport());
#endif
    
    sound_box->setEnabled(false);
    search_text->setFocus();
#ifdef USE_NEPOMUK
#if KDE_IS_VERSION(4,6,0)    
    Nepomuk::ResourceManager::instance()->init();
#endif
#endif
    slotChangeService();
}

ResourceWidget::~ResourceWidget()
{
    if (m_currentService) delete m_currentService;
    KIO::NetAccess::removeTempFile(m_tmpThumbFile);
}

void ResourceWidget::slotStartSearch(int page)
{
    page_number->blockSignals(true);
    page_number->setValue(page);
    page_number->blockSignals(false);
#if KDE_IS_VERSION(4,4,0)
    m_busyWidget->start();
#endif
    m_currentService->slotStartSearch(search_text->text(), page);
}

void ResourceWidget::slotUpdateCurrentSound()
{
    KIO::NetAccess::removeTempFile(m_tmpThumbFile);
    if (!m_autoPlay->isChecked()) m_currentService->stopItemPreview(NULL);
    item_license->clear();
    m_title.clear();
    m_image.clear();
    m_desc.clear();
    m_meta.clear();
    QListWidgetItem *item = search_results->currentItem();
    if (!item) {
        sound_box->setEnabled(false);
        return;
    }
    m_currentInfo = m_currentService->displayItemDetails(item);
    
    if (m_autoPlay->isChecked() && m_currentService->hasPreview) m_currentService->startItemPreview(item);
    sound_box->setEnabled(true);
    QString title = "<h3>" + m_currentInfo.itemName;
    if (!m_currentInfo.infoUrl.isEmpty()) title += QString(" (<a href=\"%1\">%2</a>)").arg(m_currentInfo.infoUrl).arg(i18nc("the url link pointing to a web page", "link"));
    title.append("</h3>");
    
    if (!m_currentInfo.authorUrl.isEmpty()) {
        title += QString("<a href=\"%1\">").arg(m_currentInfo.authorUrl);
        if (!m_currentInfo.author.isEmpty())
            title.append(m_currentInfo.author);
        else title.append(i18n("Author"));
        title.append("</a><br />");
    }
    else if (!m_currentInfo.author.isEmpty())
        title.append(m_currentInfo.author + "<br />");
    else title.append("<br />");
    
    slotSetTitle(title);
    if (!m_currentInfo.description.isEmpty()) slotSetDescription(m_currentInfo.description);
    if (!m_currentInfo.license.isEmpty()) parseLicense(m_currentInfo.license);
}


void ResourceWidget::slotLoadThumb(const QString url)
{
    KUrl img(url);
    if (img.isEmpty()) return;
    if (KIO::NetAccess::exists(img, KIO::NetAccess::SourceSide, this)) {
        if (KIO::NetAccess::download(img, m_tmpThumbFile, this)) {
            slotSetImage(m_tmpThumbFile);
            /*QPixmap pix(tmpFile);
            
            int newHeight = pix.height() * item_image->width() / pix.width();
            if (newHeight > 200) {
                item_image->setScaledContents(false);
                //item_image->setFixedHeight(item_image->width());
            }
            else {
                item_image->setScaledContents(true);
                item_image->setFixedHeight(newHeight);
            }
            item_image->setPixmap(pix);*/
        }
    }
}


void ResourceWidget::slotDisplayMetaInfo(QMap <QString, QString> metaInfo)
{
    if (metaInfo.contains("license")) {
        parseLicense(metaInfo.value("license"));
    }
    if (metaInfo.contains("description")) {
        slotSetDescription(metaInfo.value("description"));
    }
}


void ResourceWidget::slotPlaySound()
{
    if (!m_currentService) return;
    bool started = m_currentService->startItemPreview(search_results->currentItem());
    if (started) button_preview->setText(i18n("Preview"));
    else button_preview->setText(i18n("Stop"));
}


void ResourceWidget::slotSaveItem(const QString originalUrl)
{
    //if (m_currentUrl.isEmpty()) return;
    QListWidgetItem *item = search_results->currentItem();
    if (!item) return;
    QString path = m_folder;
    QString ext;
    if (!path.endsWith('/')) path.append('/');
    if (!originalUrl.isEmpty()) {
        path.append(KUrl(originalUrl).fileName());
        ext = "*." + KUrl(originalUrl).fileName().section('.', -1);
        m_currentInfo.itemDownload = originalUrl;
    }
    else {
        path.append(m_currentService->getDefaultDownloadName(item));
        ext = m_currentService->getExtension(search_results->currentItem());
    }
    QString saveUrl = KFileDialog::getSaveFileName(KUrl(path), ext);
    KIO::UDSEntry entry;
    KUrl srcUrl(m_currentInfo.itemDownload);
    if (saveUrl.isEmpty() || !KIO::NetAccess::stat(srcUrl, entry, this)) return;
    KIO::FileCopyJob * getJob = KIO::file_copy(srcUrl, KUrl(saveUrl), -1, KIO::Overwrite);
    
    KFileItem info(entry, srcUrl); 
    getJob->setSourceSize(info.size());
    getJob->setProperty("license", item_license->text());
    getJob->setProperty("licenseurl", item_license->url());
    getJob->setProperty("originurl", m_currentInfo.itemDownload);
    if (!m_currentInfo.authorUrl.isEmpty()) getJob->setProperty("author", m_currentInfo.authorUrl);
    else if (!m_currentInfo.author.isEmpty()) getJob->setProperty("author", m_currentInfo.author);
    connect(getJob, SIGNAL(result(KJob*)), this, SLOT(slotGotFile(KJob*)));
    getJob->start();
}

void ResourceWidget::slotGotFile(KJob *job)
{
    if (job->error() != 0 ) return;
    KIO::FileCopyJob* copyJob = static_cast<KIO::FileCopyJob*>( job );
    const KUrl filePath = copyJob->destUrl();
#ifdef USE_NEPOMUK
#if KDE_IS_VERSION(4,6,0)
        Nepomuk::Resource res( filePath );
        res.setProperty( Nepomuk::Vocabulary::NIE::license(), (Nepomuk::Variant) job->property("license") );
        res.setProperty( Nepomuk::Vocabulary::NIE::licenseType(), (Nepomuk::Variant) job->property("licenseurl") );
        res.setProperty( Nepomuk::Vocabulary::NDO::copiedFrom(), (Nepomuk::Variant) job->property("originurl") );
        res.setProperty( Nepomuk::Vocabulary::NCO::creator(), (Nepomuk::Variant) job->property("author") );
        //res.setDescription(item_description->toPlainText());
        //res.setProperty( Soprano::Vocabulary::NAO::description(), 
#endif
#endif
        emit addClip(filePath, stringMap());
}

void ResourceWidget::slotOpenUrl(const QString &url)
{
    new KRun(KUrl(url), this);
}

void ResourceWidget::slotChangeService()
{
    if (m_currentService) {
        delete m_currentService;
        m_currentService = NULL;
    }
    SERVICETYPE service = (SERVICETYPE) service_list->itemData(service_list->currentIndex()).toInt();
    if (service == FREESOUND) {
        m_currentService = new FreeSound(search_results);
    }
    else if (service == OPENCLIPART) {
        m_currentService = new OpenClipArt(search_results);
    }
    else if (service == ARCHIVEORG) {
        m_currentService = new ArchiveOrg(search_results);
    }

    connect(m_currentService, SIGNAL(gotMetaInfo(const QString)), this, SLOT(slotSetMetadata(const QString)));
    connect(m_currentService, SIGNAL(gotMetaInfo(QMap <QString, QString>)), this, SLOT(slotDisplayMetaInfo(QMap <QString, QString>)));
    connect(m_currentService, SIGNAL(maxPages(int)), this, SLOT(slotSetMaximum(int)));
    connect(m_currentService, SIGNAL(searchInfo(QString)), search_info, SLOT(setText(QString)));
    connect(m_currentService, SIGNAL(gotThumb(const QString)), this, SLOT(slotLoadThumb(const QString)));
#if KDE_IS_VERSION(4,4,0)
    connect(m_currentService, SIGNAL(searchDone()), m_busyWidget, SLOT(stop()));
#endif
    
    button_preview->setVisible(m_currentService->hasPreview);
    button_import->setVisible(!m_currentService->inlineDownload);
    search_info->setText(QString());
    if (!search_text->text().isEmpty()) slotStartSearch();
}

void ResourceWidget::slotSetMaximum(int max)
{
    page_number->setMaximum(max);
}

void ResourceWidget::slotOnline()
{
    button_search->setEnabled(true);
    search_info->setText(QString());
}

void ResourceWidget::slotOffline()
{
    button_search->setEnabled(false);
    search_info->setText(i18n("You need to be online\n for searching"));
}

void ResourceWidget::slotNextPage()
{
    int ix = page_number->value();
    if (search_results->count() > 0) page_number->setValue(ix + 1);
}

void ResourceWidget::slotPreviousPage()
{
    int ix = page_number->value();
    if (ix > 1) page_number->setValue(ix - 1);
}

void ResourceWidget::parseLicense(const QString &licenseUrl)
{
    QString licenseName;
    if (licenseUrl.contains("/sampling+/"))
        licenseName = "Sampling+";
    else if (licenseUrl.contains("/by/"))
        licenseName = "Attribution";
    else if (licenseUrl.contains("/by-nd/"))
        licenseName = "Attribution-NoDerivs";
    else if (licenseUrl.contains("/by-nc-sa/"))
        licenseName = "Attribution-NonCommercial-ShareAlike";
    else if (licenseUrl.contains("/by-sa/"))
        licenseName = "Attribution-ShareAlike";
    else if (licenseUrl.contains("/by-nc/"))
        licenseName = "Attribution-NonCommercial";
    else if (licenseUrl.contains("/by-nc-nd/"))
        licenseName = "Attribution-NonCommercial-NoDerivs";
    else if (licenseUrl.contains("/publicdomain/zero/"))
        licenseName = "Creative Commons 0";
    else if (licenseUrl.endsWith("/publicdomain"))
        licenseName = "Public Domain";
    else licenseName = i18n("Unknown");
    item_license->setText(licenseName);
    item_license->setUrl(licenseUrl);
}


void ResourceWidget::slotOpenLink(const QUrl &url)
{
    QString path = url.toEncoded();
    if (path.endsWith("_import")) {
        path.chop(7);
        // import file in Kdenlive
        slotSaveItem(path);
    }
    else {
        slotOpenUrl(path);
    }
}

void ResourceWidget::slotSetDescription(const QString desc)
{
    m_desc = desc;
    updateLayout();
}

void ResourceWidget::slotSetMetadata(const QString desc)
{
    m_meta = desc;
    updateLayout();
}

void ResourceWidget::slotSetImage(const QString desc)
{
    m_image = QString("<img src=\"%1\" style=\"max-height:150px\" max-width=\"%2px\" />").arg(desc).arg((int) (info_browser->width() * 0.9));
    updateLayout();
}

void ResourceWidget::slotSetTitle(const QString desc)
{
    m_title = desc;
    updateLayout();
}

void ResourceWidget::updateLayout()
{
    QString content = m_title;
    if (!m_image.isEmpty()) content.append(m_image + "<br clear=\"all\" />");
    if (!m_desc.isEmpty()) content.append(m_desc);
    if (!m_meta.isEmpty()) content.append(m_meta);
    info_browser->setHtml(content);
}

