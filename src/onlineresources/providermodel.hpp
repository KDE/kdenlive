/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QObject>
#include <QPixmap>
#include <QTemporaryFile>
#include <QtNetworkAuth>
#include <kio/jobclasses.h>

struct ResourceItemInfo
{
    QString fileType;
    QString name;
    QString description;
    QString id;
    QString infoUrl;
    QString license;
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
    // int filesize;
};

class ProviderModel : public QObject
{
    Q_OBJECT
public:
    enum SERVICETYPE { UNKNOWN = 0, AUDIO = 1, VIDEO = 2, IMAGE = 3 };
    ProviderModel() = delete;
    ProviderModel(const QString &path);

    void authorize();
    void refreshAccessToken();
    bool is_valid() const;
    QString name() const;
    QString homepage() const;
    ProviderModel::SERVICETYPE type() const;
    QString attribution() const;
    bool downloadOAuth2() const;
    bool requiresLogin() const;

public slots:
    void slotStartSearch(const QString &searchText, int page);
    void slotFetchFiles(const QString &id);
    // void slotShowResults(QNetworkReply *reply);

protected:
    QOAuth2AuthorizationCodeFlow m_oauth2;
    QString m_path;
    QString m_name;
    QString m_homepage;
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
    QJsonValue objectGetValue(QJsonObject item, QString key);
    QString objectGetString(QJsonObject item, const QString &key, const QString &id = QString(), const QString &parentKey = QString());
    QString replacePlaceholders(QString string, const QString &query = QString(), const int page = 0, const QString &id = QString());
    std::pair<QList<ResourceItemInfo>, const int> parseSearchResponse(const QByteArray &res);
    std::pair<QStringList, QStringList> parseFilesResponse(const QByteArray &res, const QString &id);
    QTemporaryFile *m_tmpThumbFile;
    const int m_perPage = 15;

signals:
    void searchDone(QList<ResourceItemInfo> &list, const int pageCount);
    void searchError(const QString &msg = QString());
    void fetchedFiles(QStringList, QStringList, const QString &token = QString());
    void authenticated(const QString &token);
    void usePreview();
    void authorizeWithBrowser(const QUrl &url);
};
