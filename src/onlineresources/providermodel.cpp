/***************************************************************************
 *   Copyright (C) 2021 by Julius Künzel (jk.kdedev@smartlab.uber.space)   *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "providermodel.hpp"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonValue>
#include <QJsonArray>
#include <QUrlQuery>
#include <KMessageBox>
#include <klocalizedstring.h>
#include <kio/storedtransferjob.h>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDesktopServices>

ProviderModel::ProviderModel(const QString &path)
    : m_path(path)
    , m_invalid(false)
{

    QFile file(path);
    QJsonParseError jsonError;

    if (!file.exists()) {
        qCWarning(KDENLIVE_LOG) << "WARNING, can not find provider configuration file at" << path << ".";
        m_invalid = true;
    } else {
        file.open(QFile::ReadOnly);
        m_doc = QJsonDocument::fromJson(file.readAll(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError) {
            m_invalid = true;
            // There was an error parsing data
            KMessageBox::error(nullptr, jsonError.errorString(), i18nc("@title:window", "Error Loading Data"));
            return;
        }
        validate();
    }

    if(!m_invalid) {
        m_apiroot = m_doc["api"].toObject()["root"].toString();
        m_search = m_doc["api"].toObject()["search"].toObject();
        m_download = m_doc["api"].toObject()["downloadUrls"].toObject();
        m_name = m_doc["name"].toString();
        m_clientkey = m_doc["clientkey"].toString();
        m_attribution = m_doc["attributionHtml"].toString();
        m_homepage = m_doc["homepage"].toString();

        #ifndef DOXYGEN_SHOULD_SKIP_THIS // don't make this any more public than it is.
        if(!m_clientkey.isEmpty()) {
            //all these keys are registered with online-resources@kdenlive.org
            m_clientkey.replace("%freesound_apikey%","aJuPDxHP7vQlmaPSmvqyca6YwNdP0tPaUnvmtjIn");
            m_clientkey.replace("%pexels_apikey%","563492ad6f91700001000001c2c34d4986e5421eb353e370ae5a89d0");
            m_clientkey.replace("%pixabay_apikey%","20228828-57acfa09b69e06ae394d206af");
        }
        #endif

        if( m_doc["type"].toString() == "music")  {
            m_type = SERVICETYPE::AUDIO;
        } else if( m_doc["type"].toString() == "sound")  {
            m_type = SERVICETYPE::AUDIO;
        } else if( m_doc["type"].toString() == "video")  {
            m_type = SERVICETYPE::VIDEO;
        } else if( m_doc["type"].toString() == "image")  {
            m_type = SERVICETYPE::IMAGE;
        } else {
            m_type = SERVICETYPE::UNKNOWN;
        }

        if(downloadOAuth2() == true) {
            QJsonObject ouath2Info= m_doc["api"].toObject()["oauth2"].toObject();
            auto replyHandler = new QOAuthHttpServerReplyHandler(1337, this);
            m_oauth2.setReplyHandler(replyHandler);
            m_oauth2.setAuthorizationUrl(QUrl(ouath2Info["authorizationUrl"].toString()));
            m_oauth2.setAccessTokenUrl(QUrl(ouath2Info["accessTokenUrl"].toString()));
            m_oauth2.setClientIdentifier(ouath2Info["clientId"].toString());
            m_oauth2.setClientIdentifierSharedKey(m_clientkey);
#if QT_VERSION >= QT_VERSION_CHECK(5,12,0)
            connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::refreshTokenChanged, this, [&](const QString &refreshToken){
#else
            connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::tokenChanged, this, [&](const QString &refreshToken){
#endif
                KSharedConfigPtr config = KSharedConfig::openConfig();
                KConfigGroup authGroup(config, "OAuth2Authentication" + m_name);
                authGroup.writeEntry(QStringLiteral("refresh_token"), refreshToken);
            });

            m_oauth2.setModifyParametersFunction([&](QAbstractOAuth::Stage stage, QVariantMap *parameters) {
                if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
                    if(m_oauth2.scope().isEmpty()) {
                        parameters->remove("scope");
                    }
                }
                if (stage == QAbstractOAuth::Stage::RefreshingAccessToken) {
                    parameters->insert("client_id", m_oauth2.clientIdentifier());
                    parameters->insert("client_secret", m_oauth2.clientIdentifierSharedKey());
                }

            });

            connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::statusChanged, this, [=](QAbstractOAuth::Status status) {
                if (status == QAbstractOAuth::Status::Granted ) {
                    emit authenticated(m_oauth2.token());
                } else if (status == QAbstractOAuth::Status::NotAuthenticated) {
                    KMessageBox::error(nullptr, "DEBUG: NotAuthenticated");
                }
            });
            connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::error, this, [=](const QString &error, const QString &errorDescription) {
                qCWarning(KDENLIVE_LOG) << "Error in authorization flow. " << error << " " << errorDescription;
                emit authenticated(QString());
            });
            connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, &QDesktopServices::openUrl);
        }
    } else {
        qCWarning(KDENLIVE_LOG) << "The provider config file at " << path << " is invalid. ";
    }
}

void ProviderModel::authorize() {
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup authGroup(config, "OAuth2Authentication" + m_name);

    QString strRefreshTokenFromSettings = authGroup.readEntry(QStringLiteral("refresh_token"));

    if(m_oauth2.token().isEmpty()) {
        if (!strRefreshTokenFromSettings.isEmpty()) {
            m_oauth2.setRefreshToken(strRefreshTokenFromSettings);
            m_oauth2.refreshAccessToken();
        } else {
            m_oauth2.grant();
        }
    }  else {
        if(m_oauth2.expirationAt() < QDateTime::currentDateTime()) {
            emit authenticated(m_oauth2.token());
        } else {
            m_oauth2.refreshAccessToken();
        }

    }
}

void ProviderModel::refreshAccessToken() {
    m_oauth2.refreshAccessToken();
}

/**
 * @brief ProviderModel::validate
 * Check if config has all required fields. Result can be gotten with is_valid()
 */
void ProviderModel::validate() {

    m_invalid = true;
    if(m_doc.isNull() || m_doc.isEmpty() || !m_doc.isObject()) {
        qCWarning(KDENLIVE_LOG) << "Root object missing or invalid";
        return;
    }

    if(!m_doc["integration"].isString() || m_doc["integration"].toString() != "buildin") {
        qCWarning(KDENLIVE_LOG) << "Currently only integration type \"buildin\" is supported";
        return;
    }

    if(!m_doc["name"].isString() ) {
        qCWarning(KDENLIVE_LOG) << "Missing key name of type string ";
        return;
    }

    if(!m_doc["homepage"].isString() ) {
        qCWarning(KDENLIVE_LOG) << "Missing key homepage of type string ";
        return;
    }

    if(!m_doc["type"].isString() ) {
        qCWarning(KDENLIVE_LOG) << "Missing key type of type string ";
        return;
    }

    if(!m_doc["api"].isObject() || !m_doc["api"].toObject()["search"].isObject()) {
        qCWarning(KDENLIVE_LOG)  << "Missing api of type object or key search of type object";
        return;
    }
    if(downloadOAuth2()) {
        if(!m_doc["api"].toObject()["oauth2"].isObject()) {
            qCWarning(KDENLIVE_LOG) << "Missing OAuth2 configuration (required)";
            return;
        }
        if(m_doc["api"].toObject()["oauth2"].toObject()["authorizationUrl"].toString().isEmpty()) {
            qCWarning(KDENLIVE_LOG) << "Missing authorizationUrl for OAuth2";
            return;
        }
        if(m_doc["api"].toObject()["oauth2"].toObject()["accessTokenUrl"].toString().isEmpty()) {
            qCWarning(KDENLIVE_LOG) << "Missing accessTokenUrl for OAuth2";
            return;
        }
        if(m_doc["api"].toObject()["oauth2"].toObject()["clientId"].toString().isEmpty()) {
            qCWarning(KDENLIVE_LOG) << "Missing clientId for OAuth2";
            return;
        }
    }

    m_invalid = false;
}

bool ProviderModel::is_valid() const {
    return !m_invalid;
}

QString ProviderModel::name() const {
    return m_name;
}

QString ProviderModel::homepage() const {
    return m_homepage;
}

ProviderModel::SERVICETYPE ProviderModel::type() const {
    return m_type;
}

QString ProviderModel::attribution() const {
    return m_attribution;
}

bool ProviderModel::downloadOAuth2() const {
    return m_doc["downloadOAuth2"].toBool(false);
}

bool ProviderModel::requiresLogin() const {
    if(downloadOAuth2()) {
        KSharedConfigPtr config = KSharedConfig::openConfig();
        KConfigGroup authGroup(config, "OAuth2Authentication" + m_name);
        authGroup.exists();

        return !authGroup.exists() || authGroup.readEntry(QStringLiteral("refresh_token")).isEmpty();
    }
    return false;
}

/**
 * @brief ProviderModel::objectGetValue
 * @param item Object containing the value
 * @param key General key of value to get
 * @return value
 * Gets a value of item identified by key. The key is translated to the key the provider uses (configured in the providers config file)
 * E.g. the provider uses "photographer" as key for the author and another provider uses "user".
 * With this function you can simply use "author" as key no matter of the providers specific key.
 * In addition this function takes care of modifiers like "$" for placeholders, etc. but does not parse them (use objectGetString for this purpose)
 */

QJsonValue ProviderModel::objectGetValue(QJsonObject item, QString key) {
    QJsonObject tmpKeys = m_search["res"].toObject();
    if(key.contains(".")) {
        QStringList subkeys = key.split(".");

        for (const auto &subkey : qAsConst(subkeys)) {
            if(subkeys.indexOf(subkey) == subkeys.indexOf(subkeys.last())) {
                key = subkey;
            } else {
                tmpKeys = tmpKeys[subkey].toObject();
            }
        }
    }

    QString parseKey = tmpKeys[key].toString();
    // "$" means template, store template string instead of using as key
    if(parseKey.startsWith("$")) {
        return tmpKeys[key];
    }

    // "." in key means value is in a subobject
    if(parseKey.contains(".")) {
        QStringList subkeys = tmpKeys[key].toString().split(".");

        for (const auto &subkey : qAsConst(subkeys)) {
            if(subkeys.indexOf(subkey) == subkeys.indexOf(subkeys.last())) {
                parseKey = subkey;
            } else {
                item = item[subkey].toObject();
            }

        }
    }

    // "%" means placeholder, store placeholder instead of using as key
    if(parseKey.startsWith("%")) {
        return tmpKeys[key];
    }

    return item[parseKey];
}

/**
 * @brief ProviderModel::objectGetString
 * @param item Object containing the value
 * @param key General key of value to get
 * @param id The id is used for to replace the palceholder "%id%" (optional)
 * @param parentKey Key of the parent (json) object. Used for to replace the palceholder "&" (optional)
 * @return result string
 * Same as objectGetValue but more specific only for strings. In addition this function parses template strings and palceholders.
 */

QString ProviderModel::objectGetString(QJsonObject item, QString key, const QString &id, const QString &parentKey) {
    QJsonValue val = objectGetValue(item, key);
    if(!val.isString()) {
        return QString();
    }
    QString result = val.toString();

    if(result.startsWith("$")) {
        result = result.replace("%id%", id);
        QStringList sections = result.split("{");
        for (auto &section : sections) {
            section.remove("{");
            section.remove(section.indexOf("}"), section.length());

            // "&" is a placeholder for the parent key
            if(section.startsWith("&")) {
                result.replace("{" + section + "}", parentKey);
            } else {
                result.replace("{" + section + "}", item[section].isDouble() ? QString::number(item[section].toDouble()) : item[section].toString());
            }
        }
        result.remove("$");
    }

    return result;
}

QString ProviderModel::replacePlaceholders(QString string, const QString query, const int page, const QString id) {
    string = string.replace("%query%", query);
    string = string.replace("%pagenum%", QString::number(page));
    string = string.replace("%perpage%", QString::number(m_perPage));
    string = string.replace("%shortlocale%", "en-US"); //TODO
    string = string.replace("%clientkey%", m_clientkey);
    string = string.replace("%id%", id);

    return string;
}

/**
 * @brief ProviderModel::getFilesUrl
 * @param searchText The search query
 * @param page The page to request
 * Get the url to search for items
 */
QUrl ProviderModel::getSearchUrl(const QString &searchText, const int page) {

    QUrl url(m_apiroot);
    const QJsonObject req = m_search["req"].toObject();
    QUrlQuery query;
    url.setPath(url.path().append(req["path"].toString()));

    for (const auto param : req["params"].toArray()) {
        query.addQueryItem(param.toObject()["key"].toString(), replacePlaceholders(param.toObject()["value"].toString(), searchText, page));
    }
    url.setQuery(query);

    return url;
}

/**
 * @brief ProviderModel::slotFetchFiles
 * @param searchText The search query
 * @param page The page to request
 * Fetch metadata about the available files, if they are not included in the search response (e.g. archive.org)
 */
void ProviderModel::slotStartSearch(const QString &searchText, const int page)
{
    QUrl uri = getSearchUrl(searchText, page);

    if(m_search["req"].toObject()["method"].toString() == "GET") {

        auto *manager = new QNetworkAccessManager(this);

        QNetworkRequest request(uri);

        if(m_search["req"].toObject()["header"].isArray()) {
            for (const auto &header: m_search["req"].toObject()["header"].toArray()) {
                request.setRawHeader(header.toObject()["key"].toString().toUtf8(), replacePlaceholders(header.toObject()["value"].toString(), searchText, page).toUtf8());
            }
        }
        QNetworkReply *reply = manager->get(request);

        connect(reply, &QNetworkReply::finished, this, [=]() {
            if(reply->error() == QNetworkReply::NoError) {
                QByteArray response = reply->readAll();
                std::pair<QList<ResourceItemInfo>, const int> result = parseSearchResponse(response);
                emit searchDone(result.first, result.second);

            } else {
              emit searchError(QStringLiteral("HTTP ") + reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString());
              qCDebug(KDENLIVE_LOG) << reply->errorString();
            }
            reply->deleteLater();
        });

        connect(reply, &QNetworkReply::sslErrors, this, [=]() {
            emit searchError(QStringLiteral("HTTP ") + reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString());
            qCDebug(KDENLIVE_LOG) << reply->errorString();
        });

    } else {
        qCDebug(KDENLIVE_LOG) << "Only GET is implemented yet";
    }
}

/**
 * @brief ProviderModel::parseFilesResponse
 * @param data Response data of the api request
 * @return pair of  of QList containing ResourceItemInfo and int reflecting the number of found pages
 * Parse the response data of a search request, usually after slotStartSearch
 */
std::pair<QList<ResourceItemInfo>, const int> ProviderModel::parseSearchResponse(const QByteArray &data) {
    QJsonObject keys = m_search["res"].toObject();
    QList<ResourceItemInfo> list;
    int pageCount = 0;
    if(keys["format"].toString() == "json") {
        QJsonDocument res = QJsonDocument::fromJson(data);

        QJsonArray items;
        if(keys["list"].toString("").isEmpty()) {
            items = res.array();
        } else {
            items = objectGetValue(res.object(), "list").toArray();
        }

        pageCount = objectGetValue(res.object(), "resultCount").toInt() / m_perPage;

        for (const auto &item : qAsConst(items)) {
            ResourceItemInfo onlineItem;
            onlineItem.author = objectGetString(item.toObject(), "author");
            onlineItem.authorUrl = objectGetString(item.toObject(), "authorUrl");
            onlineItem.name = objectGetString(item.toObject(), "name");
            onlineItem.filetype = objectGetString(item.toObject(), "filetype");
            onlineItem.description = objectGetString(item.toObject(), "description");
            onlineItem.id = (objectGetValue(item.toObject(), "id").isString() ? objectGetString(item.toObject(), "id") : QString::number(objectGetValue(item.toObject(), "id").toInt()));
            onlineItem.infoUrl = objectGetString(item.toObject(), "url");
            onlineItem.license = objectGetString(item.toObject(), "licenseUrl");
            onlineItem.imageUrl = objectGetString(item.toObject(), "imageUrl");
            onlineItem.previewUrl = objectGetString(item.toObject(), "previewUrl");
            onlineItem.width = objectGetValue(item.toObject(), "width").toInt();
            onlineItem.height = objectGetValue(item.toObject(), "height").toInt();
            onlineItem.duration = objectGetValue(item.toObject(), "duration").isDouble() ? int(objectGetValue(item.toObject(), "duration").toDouble()) : objectGetValue(item.toObject(), "duration").toInt();

            if(keys["downloadUrls"].isObject()) {
                for (const auto urlItem : objectGetValue(item.toObject(), "downloadUrls.key").toArray()) {
                    onlineItem.downloadUrls << objectGetString(urlItem.toObject(), "downloadUrls.url");
                    onlineItem.downloadLabels << objectGetString(urlItem.toObject(), "downloadUrls.name");
                }
                if (onlineItem.previewUrl.isEmpty()) {
                    onlineItem.previewUrl = onlineItem.downloadUrls.first();
                }
            } else if(keys["downloadUrl"].isString()){
                onlineItem.downloadUrl = objectGetString(item.toObject(), "downloadUrl");
                if (onlineItem.previewUrl.isEmpty()) {
                    onlineItem.previewUrl = onlineItem.downloadUrl;
                }
            }

            list << onlineItem;
        }
    } else {
        qCWarning(KDENLIVE_LOG) << "WARNING: unknown response format: " << keys["format"];
    }
    return std::pair<QList<ResourceItemInfo>, const int> (list, pageCount);
}

/**
 * @brief ProviderModel::getFilesUrl
 * @param id The providers id of the item the data should be fetched for
 * @return the url
 * Get the url to fetch metadata about the available files.
 */
QUrl ProviderModel::getFilesUrl(const QString &id) {

    QUrl url(m_apiroot);
    if(!m_download["req"].isObject()) {
        return QUrl();
    }
    const QJsonObject req = m_download["req"].toObject();
    QUrlQuery query;
    url.setPath(url.path().append(replacePlaceholders(req["path"].toString(), QString(), 0, id)));

    for (const auto param : req["params"].toArray()) {
        query.addQueryItem(param.toObject()["key"].toString(), replacePlaceholders(param.toObject()["value"].toString(), QString(), 0, id));
    }

    url.setQuery(query);

    return url;
}

/**
 * @brief ProviderModel::slotFetchFiles
 * @param id The providers id of the item the date should be fetched for
 * Fetch metadata about the available files, if they are not included in the search response (e.g. archive.org)
 */
void ProviderModel::slotFetchFiles(const QString &id) {

    QUrl uri = getFilesUrl(id);

    if(uri.isEmpty()) {
        return;
    }

    if(m_download["req"].toObject()["method"].toString() == "GET") {

        auto *manager = new QNetworkAccessManager(this);

        QNetworkRequest request(uri);

        if(m_download["req"].toObject()["header"].isArray()) {
            for (const auto &header: m_search["req"].toObject()["header"].toArray()) {
                request.setRawHeader(header.toObject()["key"].toString().toUtf8(), replacePlaceholders(header.toObject()["value"].toString()).toUtf8());
            }
        }
        QNetworkReply *reply = manager->get(request);

        connect(reply, &QNetworkReply::finished, this, [=]() {
            if(reply->error() == QNetworkReply::NoError) {
                QByteArray response = reply->readAll();
                std::pair<QStringList, QStringList> result = parseFilesResponse(response, id);
                emit fetchedFiles(result.first, result.second);
                reply->deleteLater();
            }
            else {
              emit fetchedFiles(QStringList(),QStringList());
              qCDebug(KDENLIVE_LOG) << reply->errorString();
            }
        });

        connect(reply, &QNetworkReply::sslErrors, this, [=]() {
            emit fetchedFiles(QStringList(), QStringList());
            qCDebug(KDENLIVE_LOG) << reply->errorString();
        });

    } else {
        qCDebug(KDENLIVE_LOG) << "Only GET is implemented yet";
    }
}

/**
 * @brief ProviderModel::parseFilesResponse
 * @param data Response data of the api request
 * @param id The providers id of the item the date should be fetched for
 * @return pair of two QStringList First list contains urls to files, second list contains labels describing the files
 * Parse the response data of a fetch files request, usually after slotFetchFiles
 */
std::pair<QStringList, QStringList> ProviderModel::parseFilesResponse(const QByteArray &data, const QString &id) {
    QJsonObject keys = m_download["res"].toObject();
    QStringList urls;
    QStringList labels;

    if(keys["format"].toString() == "json") {
        QJsonObject res = QJsonDocument::fromJson(data).object();

        if(keys["downloadUrls"].isObject()) {
            if(keys["downloadUrls"].toObject()["isObject"].toBool(false)) {
                QJsonObject list = objectGetValue(res, "downloadUrls.key").toObject();
                for (const auto &key : list.keys()) {
                    QJsonObject urlItem = list[key].toObject();
                    QString format = objectGetString(urlItem, "downloadUrls.format", id, key);
                    //This ugly check is only for the complicated archive.org api to avoid a long file list for videos caused by thumbs for each frame and metafiles
                    if(m_type == ProviderModel::VIDEO && m_homepage == "https://archive.org" && format != QLatin1String("Animated GIF") && format != QLatin1String("Metadata")
                            && format != QLatin1String("Archive BitTorrent") && format != QLatin1String("Thumbnail") && format != QLatin1String("JSON")
                            && format != QLatin1String("JPEG") && format != QLatin1String("JPEG Thumb") && format != QLatin1String("PNG")
                            && format != QLatin1String("Video Index")) {
                        urls << objectGetString(urlItem, "downloadUrls.url", id, key);
                        labels << objectGetString(urlItem, "downloadUrls.name", id, key);
                    }
                }
            } else {
                for (const auto urlItem : objectGetValue(res, "downloadUrls.key").toArray()) {
                    urls << objectGetString(urlItem.toObject(), "downloadUrls.url", id);
                    labels << objectGetString(urlItem.toObject(), "downloadUrls.name", id);
                }
            }

        } else if(keys["downloadUrl"].isString()){
            urls << objectGetString(res, "downloadUrl", id);
        }

    } else {
        qCWarning(KDENLIVE_LOG) << "WARNING fetch files: unknown response format: " << keys["format"];
    }
    return std::pair<QStringList, QStringList> (urls, labels);
}
