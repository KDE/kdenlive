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

#ifndef RESOURCEWIDGET_H
#define RESOURCEWIDGET_H

#include "abstractservice.h"
#include "definitions.h"
#include "ui_freesound_ui.h"

#include <QDialog>
#include <QNetworkReply>
#include <kio/jobclasses.h>

class QAction;
class QNetworkConfigurationManager;
class QTemporaryFile;
class QMovie;
class OAuth2;

/**
  \brief This is the window that appears from Project>Online Resources

 * In the Online Resources window the user can choose from different online resources such as Freesound Audio Library, Archive.org Video
 * Library and Open clip Art Graphic Library.
 * Depending on which of these is selected  the resourcewidget will instantiate a FreeSound, ArchiveOrg or OpenClipArt
 * class that will deal with the searching and parsing of the results for the different on line resource libraries

  */
class ResourceWidget : public QDialog, public Ui::FreeSound_UI
{
    Q_OBJECT

public:
    explicit ResourceWidget(QString folder, QWidget *parent = nullptr);
    ~ResourceWidget() override;

private slots:
    void slotStartSearch(int page = 0);
    /**
     * @brief Fires when user selects a different item in the list of found items
     *
     * This is not just for sounds. It fires for clip art and videos too.
     */
    void slotUpdateCurrentSound();
    void slotPlaySound();
    void slotDisplayMetaInfo(const QMap<QString, QString> &metaInfo);
    void slotSaveItem(const QString &originalUrl = QString());
    void slotOpenUrl(const QString &url);
    void slotChangeService();
    void slotOnlineChanged(bool online);
    void slotNextPage();
    void slotPreviousPage();
    void slotOpenLink(const QUrl &url);
    void slotLoadThumb(const QString &url);
    /** @brief A file download is finished */
    void slotGotFile(KJob *job);
    void slotSetMetadata(const QString &metadata);
    void slotSetDescription(const QString &desc);
    void slotSetImage(const QString &desc);
    void slotSetTitle(const QString &title);
    void slotSetMaximum(int max);
    void slotPreviewFinished();
    void slotFreesoundAccessDenied();
    void slotReadyRead();
    void DownloadRequestFinished(QNetworkReply *reply);
    void slotAccessTokenReceived(const QString &sAccessToken);
    void slotFreesoundUseHQPreview();
    void slotFreesoundCanceled();
    void slotSearchFinished();
    void slotLoadPreview(const QString &url);
    void slotLoadAnimatedGif(KJob *job);

private:
    OAuth2 *m_pOAuth2;
    QNetworkConfigurationManager *m_networkManager;
    QNetworkAccessManager *m_networkAccessManager;
    void loadConfig();
    void saveConfig();
    void parseLicense(const QString &);
    QString GetSaveFileNameAndPathS(const QString &path, const QString &ext);
    QString m_folder;
    QString m_saveLocation;
    AbstractService *m_currentService;
    OnlineItemInfo m_currentInfo;
    QAction *m_autoPlay;
    QTemporaryFile *m_tmpThumbFile;
    QString m_title;
    QString m_desc;
    QString m_meta;
    QMovie *m_movie;
    void updateLayout();
    void DoFileDownload(const QUrl &srcUrl, const QUrl &saveUrl);

signals:
    void addClip(const QUrl &, const QString &);
};

#endif
