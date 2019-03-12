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

#ifndef ABSTRACTSERVICE_H
#define ABSTRACTSERVICE_H

#include <QListWidget>

const int imageRole = Qt::UserRole;
const int urlRole = Qt::UserRole + 1;
const int downloadRole = Qt::UserRole + 2;
const int durationRole = Qt::UserRole + 3;
const int previewRole = Qt::UserRole + 4;
const int authorRole = Qt::UserRole + 5;
const int authorUrl = Qt::UserRole + 6;
const int infoUrl = Qt::UserRole + 7;
const int infoData = Qt::UserRole + 8;
const int idRole = Qt::UserRole + 9;
const int licenseRole = Qt::UserRole + 10;
const int descriptionRole = Qt::UserRole + 11;

struct OnlineItemInfo
{
    QString itemPreview;
    QString itemName;
    QString itemDownload;
    QString itemId;
    QString infoUrl;
    QString license;
    QString author;
    QString authorUrl;
    QString description;
    QString fileType;
    QString HQpreview;
};

class AbstractService : public QObject
{
    Q_OBJECT

public:
    enum SERVICETYPE { NOSERVICE = 0, FREESOUND = 1, OPENCLIPART = 2, ARCHIVEORG = 3 };

    explicit AbstractService(QListWidget *listWidget, QObject *parent = nullptr);
    ~AbstractService() override;
    /** @brief Get file extension for currently selected item. */
    virtual QString getExtension(QListWidgetItem *item);
    /** @brief Get recommEnded download file name. */
    virtual QString getDefaultDownloadName(QListWidgetItem *item);
    /** @brief Does this service provide a preview (for example preview a sound. */
    bool hasPreview;
    /** @brief Does this service provide meta info about the item. */
    bool hasMetadata;
    /** @brief Should we show the "import" button or does this service provide download urls in info browser. */
    bool inlineDownload;
    /** @brief The type for this service. */
    SERVICETYPE serviceType;

public slots:
    virtual void slotStartSearch(const QString &searchText, int page = 0);
    virtual OnlineItemInfo displayItemDetails(QListWidgetItem *item);
    virtual bool startItemPreview(QListWidgetItem *item);
    virtual void stopItemPreview(QListWidgetItem *item);

protected:
    QListWidget *m_listWidget;

signals:
    void searchInfo(const QString &);
    void maxPages(int);
    /** @brief Emit meta info for current item in formatted html. */
    void gotMetaInfo(const QString &);
    /** @brief Emit some extra meta info (description, license). */
    void gotMetaInfo(const QMap<QString, QString> &info);
    /** @brief We have an url for current item's preview thumbnail. */
    void gotThumb(const QString &url);
    /** @brief The requested search query is finished. */
    void searchDone();
};

#endif
