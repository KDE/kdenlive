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

#include <QPushButton>
#include <QSpinBox>
#include <QListWidget>
#include <QDomDocument>

#include <KDebug>
#include "kdenlivesettings.h"
#include <KGlobalSettings>
#include <KMessageBox>
#include <KFileDialog>
#include <kio/job.h>
#include <KIO/NetAccess>
#include <Solid/Networking>
#include <KRun>

#ifdef USE_NEPOMUK AND KDE_IS_VERSION(4,6,0)
#include <Nepomuk/Variant>
#include <Nepomuk/Resource>
#include <Nepomuk/ResourceManager>
#include <Soprano/Vocabulary/NAO>
#include <Nepomuk/Vocabulary/NIE>
#include <Nepomuk/Vocabulary/NDO>
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
#endif
    service_list->addItem(i18n("Open Clip Art Graphic Library"), OPENCLIPART);
    setWindowTitle(i18n("Search Online Resources"));
    info_widget->setStyleSheet(QString("QTreeWidget { background-color: transparent;}"));
    item_description->setStyleSheet(QString("KTextBrowser { background-color: transparent;}"));
    connect(button_search, SIGNAL(clicked()), this, SLOT(slotStartSearch()));
    connect(search_results, SIGNAL(currentRowChanged(int)), this, SLOT(slotUpdateCurrentSound()));
    connect(button_preview, SIGNAL(clicked()), this, SLOT(slotPlaySound()));
    connect(button_import, SIGNAL(clicked()), this, SLOT(slotSaveSound()));
    connect(sound_author, SIGNAL(leftClickedUrl(const QString &)), this, SLOT(slotOpenUrl(const QString &)));
    connect(item_license, SIGNAL(leftClickedUrl(const QString &)), this, SLOT(slotOpenUrl(const QString &)));
    connect(sound_name, SIGNAL(leftClickedUrl(const QString &)), this, SLOT(slotOpenUrl(const QString &)));
    connect(service_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChangeService()));
    sound_image->setFixedWidth(180);
    if (Solid::Networking::status() == Solid::Networking::Unconnected) {
        slotOffline();
    }
    connect(Solid::Networking::notifier(), SIGNAL(shouldConnect()), this, SLOT(slotOnline()));
    connect(Solid::Networking::notifier(), SIGNAL(shouldDisconnect()), this, SLOT(slotOffline()));
    connect(page_next, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(page_prev, SIGNAL(clicked()), this, SLOT(slotPreviousPage()));
    connect(page_number, SIGNAL(valueChanged(int)), this, SLOT(slotStartSearch(int)));
    sound_box->setEnabled(false);
    search_text->setFocus();
#ifdef USE_NEPOMUK AND KDE_IS_VERSION(4,6,0)    
    Nepomuk::ResourceManager::instance()->init();
#endif
    slotChangeService();
}

ResourceWidget::~ResourceWidget()
{
    if (m_currentService) delete m_currentService;
}

void ResourceWidget::slotStartSearch(int page)
{
    /*m_currentPreview.clear();
    m_currentUrl.clear();*/
    page_number->blockSignals(true);
    page_number->setValue(page);
    page_number->blockSignals(false);
    m_currentService->slotStartSearch(search_text->text(), page);
    /*QString uri;
    if (m_service == FREESOUND) {
        uri = "http://www.freesound.org/api/sounds/search/?q=";
        uri.append(search_text->text());
        if (page > 1) uri.append("&p=" + QString::number(page));
        uri.append("&api_key=a1772c8236e945a4bee30a64058dabf8");
    }
    else if (m_service == OPENCLIPART) {
        uri = "http://openclipart.org/api/search/?query=";
        uri.append(search_text->text());
        if (page > 1) uri.append("&page=" + QString::number(page));
    }*/
}

void ResourceWidget::slotUpdateCurrentSound()
{
    if (!sound_autoplay->isChecked()) m_currentService->stopItemPreview(NULL);
    info_widget->clear();
    item_description->clear();
    item_license->clear();
    QListWidgetItem *item = search_results->currentItem();
    if (!item) {
        sound_box->setEnabled(false);
        return;
    }
    m_currentInfo = m_currentService->displayItemDetails(item);
    /*m_currentPreview = item->data(previewRole).toString();
    m_currentUrl = item->data(downloadRole).toString();
    m_currentId = item->data(idRole).toInt();*/
    
    if (sound_autoplay->isChecked()) m_currentService->startItemPreview(item);
    sound_box->setEnabled(true);
    sound_name->setText(item->text());
    sound_name->setUrl(m_currentInfo.infoUrl);
    sound_author->setText(m_currentInfo.author);
    sound_author->setUrl(m_currentInfo.authorUrl);
    item_description->setHtml(m_currentInfo.description);
    

    KUrl img(m_currentInfo.imagePreview);
    if (img.isEmpty()) return;
    if (KIO::NetAccess::exists(img, KIO::NetAccess::SourceSide, this)) {
        QString tmpFile;
        if (KIO::NetAccess::download(img, tmpFile, this)) {
            QPixmap pix(tmpFile);
            int newHeight = pix.height() * sound_image->width() / pix.width();
            if (newHeight > 200) {
                sound_image->setScaledContents(false);
                //sound_image->setFixedHeight(sound_image->width());
            }
            else {
                sound_image->setScaledContents(true);
                sound_image->setFixedHeight(newHeight);
            }
            sound_image->setPixmap(pix);
            KIO::NetAccess::removeTempFile(tmpFile);
        }
    }
}


void ResourceWidget::slotDisplayMetaInfo(QMap <QString, QString> metaInfo)
{
    if (metaInfo.contains("description")) {
        item_description->setHtml(metaInfo.value("description"));
        metaInfo.remove("description");
    }
    if (metaInfo.contains("license")) {
        parseLicense(metaInfo.value("license"));
        metaInfo.remove("license");
    }
    QMap<QString, QString>::const_iterator i = metaInfo.constBegin();
    while (i != metaInfo.constEnd()) {
        new QTreeWidgetItem(info_widget, QStringList() << i.key() << i.value());
        ++i;
    }
    info_widget->resizeColumnToContents(0);
    info_widget->resizeColumnToContents(1);
}


void ResourceWidget::slotPlaySound()
{
    if (!m_currentService) return;
    bool started = m_currentService->startItemPreview(search_results->currentItem());
    if (started) button_preview->setText(i18n("Preview"));
    else button_preview->setText(i18n("Stop"));
}


void ResourceWidget::slotForcePlaySound(bool play)
{
    /*
    if (m_service != FREESOUND) return;
    m_previewProcess->close();
    if (m_currentPreview.isEmpty()) return;
    if (play)
        m_previewProcess->start("ffplay", QStringList() << m_currentPreview << "-nodisp");
    */
}

void ResourceWidget::slotPreviewStatusChanged(QProcess::ProcessState state)
{
    /*if (state == QProcess::NotRunning)
        button_preview->setText(i18n("Preview"));
    else 
        button_preview->setText(i18n("Stop"));*/
}

void ResourceWidget::slotSaveSound()
{
    //if (m_currentUrl.isEmpty()) return;
    QListWidgetItem *item = search_results->currentItem();
    if (!item) return;
    QString path = m_folder;
    if (!path.endsWith('/')) path.append('/');
    path.append(m_currentService->getDefaultDownloadName(item));
    QString ext = m_currentService->getExtension(search_results->currentItem());
    QString saveUrl = KFileDialog::getSaveFileName(KUrl(path), ext);
    if (saveUrl.isEmpty()) return;
    if (KIO::NetAccess::download(KUrl(m_currentInfo.itemDownload), saveUrl, this)) {
        const KUrl filePath = KUrl(saveUrl);
#ifdef USE_NEPOMUK AND KDE_IS_VERSION(4,6,0)
        Nepomuk::Resource res( filePath );
        res.setProperty( Nepomuk::Vocabulary::NIE::license(), (Nepomuk::Variant) item_license->text() );
        res.setProperty( Nepomuk::Vocabulary::NIE::licenseType(), (Nepomuk::Variant) item_license->url() );
        res.setProperty( Nepomuk::Vocabulary::NDO::copiedFrom(), sound_name->url() );
        //res.setDescription(item_description->toPlainText());
        //res.setProperty( Soprano::Vocabulary::NAO::description(), 
#endif
        emit addClip(KUrl(saveUrl), QString());//, sound_name->url());
    }
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
    connect(m_currentService, SIGNAL(gotMetaInfo(QMap <QString, QString>)), this, SLOT(slotDisplayMetaInfo(QMap <QString, QString>)));
    connect(m_currentService, SIGNAL(maxPages(int)), page_number, SLOT(setMaximum(int)));
    connect(m_currentService, SIGNAL(searchInfo(QString)), search_info, SLOT(setText(QString)));
    
    button_preview->setVisible(m_currentService->hasPreview);
    sound_autoplay->setVisible(m_currentService->hasPreview);
    search_info->setText(QString());
    info_widget->setVisible(m_currentService->hasMetadata);
    if (!search_text->text().isEmpty()) slotStartSearch();
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
    item_license->setText(i18n("License: %1", licenseName));
    item_license->setUrl(licenseUrl);
}

