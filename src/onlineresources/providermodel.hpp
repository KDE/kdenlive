/***************************************************************************
 *   Copyright (C) 2021 by Julius Künzel                                   *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.*
 ***************************************************************************/

#ifndef PROVIDERMODEL_H
#define PROVIDERMODEL_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <kio/jobclasses.h>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QPixmap>

struct ResourceItemInfo
{
    QString fileType;
    QString name;
    QString description;
    QString id;
    QString infoUrl;
    QString license;
    QString attributionText;
    QString author;
    QString authorUrl;
    int width;
    int height;
    int duration;
    QString downloadUrl;
    QString filetype;
    QStringList downloadUrls;
    QStringList downloadLabels;
    QString imageUrl;
    QString previewUrl;
    int filesize;
};

class ProviderModel : public QObject
{
    Q_OBJECT
public:
    enum SERVICETYPE { UNKNOWN = 0, AUDIO = 1, VIDEO = 2, IMAGE = 3};
    enum INTEGRATIONTYPE { BUILDIN = 1, BROWSER = 2};
    ProviderModel() = delete;
    ProviderModel(const QString &path);

    bool is_valid() const;
    QString name() const;
    QString homepage() const;
    ProviderModel::SERVICETYPE type() const;
    ProviderModel::INTEGRATIONTYPE integratonType() const;
    QString attribution() const;
    bool downloadOAuth2() const;

public slots:
    void slotStartSearch(const QString &searchText, int page);
    void slotFetchFiles(const QString &id);
    //void slotShowResults(QNetworkReply *reply);

protected:
    QString m_path;
    QString m_name;
    QString m_homepage;
    INTEGRATIONTYPE m_integrationtype;
    SERVICETYPE m_type;
    QString m_clientkey;
    QString m_attribution;
    bool m_invalid;
    QJsonDocument m_doc;
    QString m_apiroot;
    QJsonObject m_search;
    QJsonObject m_download;

private:
    void validate();
    QUrl getSearchUrl(const QString &searchText, const int page = 1);
    QUrl getFilesUrl(const QString &id);
    std::pair<QList<ResourceItemInfo>, const int> parseSearchResponse(const QByteArray &res);
    std::pair<QStringList, QStringList> parseFilesResponse(const QByteArray &res, const QString &id);
    QTemporaryFile *m_tmpThumbFile;
    const int m_perPage = 15;

signals:
    void searchDone(QList<ResourceItemInfo> &list, const int pageCount);
    void searchError(const QString &msg = QString());
    void fetchedFiles(QStringList, QStringList, const QString &token = QString());

};

#endif // PROVIDERMODEL_H
