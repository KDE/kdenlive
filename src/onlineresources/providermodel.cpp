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

#include "providermodel.hpp"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"

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

ProviderModel::ProviderModel(const QString &path)
    : m_path(path)
    , m_invalid(false)
{

    QFile file(path);
    QJsonParseError jsonError;

    if (!file.exists()) {
        qCWarning(KDENLIVE_LOG) << "WARNING, COULD NOT FIND PROVIDER " << path << ".";
        m_invalid = true;
    } else {
        file.open(QFile::ReadOnly);
        m_doc = QJsonDocument::fromJson(file.readAll(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError) {
            m_invalid = true;
            // There was an error parsing data
            KMessageBox::error(nullptr, jsonError.errorString(), i18n("Error Loading Data"));
            return;
        }
        if( m_doc["integration"].toString() == "buildin")  {
            m_integrationtype = INTEGRATIONTYPE::BUILDIN;
        } else if( m_doc["integration"].toString() == "browser")  {
            m_integrationtype = INTEGRATIONTYPE::BROWSER;
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

        //KMessageBox::informationList (nullptr,"Resource Provider found",values, "Debug");
        /*if(m_doc["name"].toString() == "Pexels Photos") {
            QString data = "{ \"total_results\": 10000,\"page\": 1,\"per_page\": 1,\"photos\": [{\"id\": 3573351,\"width\": 3066,\"height\": 3968,\"url\": \"https://www.pexels.com/photo/trees-during-day-3573351/\",\"photographer\": \"Lukas Rodriguez\", \"photographer_url\": \"https://www.pexels.com/@lukas-rodriguez-1845331\",\"photographer_id\": 1845331,\"avg_color\": \"#374824\",\"src\": {\"original\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png\", \"large2x\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&dpr=2&h=650&w=940\", \"large\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&h=650&w=940\", \"medium\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&h=350\", \"small\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&h=130\", \"portrait\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&fit=crop&h=1200&w=800\", \"landscape\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&fit=crop&h=627&w=1200\", \"tiny\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&dpr=1&fit=crop&h=200&w=280\" },\"liked\": false}],\"next_page\": \"https://api.pexels.com/v1/search/?page=2&per_page=1&query=nature\" }";
            parseSearchResponse(data.toUtf8());
        }*/

        if(m_doc["name"].toString() == "Pexels Video") {
            //QString data = "{ \"total_results\": 10000,\"page\": 1,\"per_page\": 1,\"photos\": [{\"id\": 3573351,\"width\": 3066,\"height\": 3968,\"url\": \"https://www.pexels.com/photo/trees-during-day-3573351/\",\"photographer\": \"Lukas Rodriguez\", \"photographer_url\": \"https://www.pexels.com/@lukas-rodriguez-1845331\",\"photographer_id\": 1845331,\"avg_color\": \"#374824\",\"src\": {\"original\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png\", \"large2x\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&dpr=2&h=650&w=940\", \"large\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&h=650&w=940\", \"medium\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&h=350\", \"small\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&h=130\", \"portrait\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&fit=crop&h=1200&w=800\", \"landscape\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&fit=crop&h=627&w=1200\", \"tiny\": \"https://images.pexels.com/photos/3573351/pexels-photo-3573351.png?auto=compress&cs=tinysrgb&dpr=1&fit=crop&h=200&w=280\" },\"liked\": false}],\"next_page\": \"https://api.pexels.com/v1/search/?page=2&per_page=1&query=nature\" }";
            //parseSearchResponse(data);
        }
    } else {
        KMessageBox::sorry(nullptr,"The provider config at " + path + " is invalid.");
    }
}

void ProviderModel::validate() {

    m_invalid = true;
    if(m_doc.isNull() || m_doc.isEmpty() || !m_doc.isObject()) {
        qDebug() << "Invalid document";
        return;
    }

    if(!m_doc["name"].isString() ) {
        qDebug() << "Missing key name of type string ";
        return;
    }

    if(m_integrationtype != INTEGRATIONTYPE::BROWSER) {
        if(!m_doc["api"].isObject() || !m_doc["api"].toObject()["search"].isObject()) {
            qDebug() << "Missing api of type object or key search of type object";
            return;
        }
    } else {
        if(!m_doc["homepage"].isString() ) {
            qDebug() << "Missing key homepage of type string ";
            return;
        }
    }

    m_invalid = false;
}

bool ProviderModel::is_valid() const {
    return !m_invalid;
}

QString ProviderModel::name() const
{
    return m_name;
}

QString ProviderModel::homepage() const
{
    return m_homepage;
}

ProviderModel::SERVICETYPE ProviderModel::type() const
{
    return m_type;
}

ProviderModel::INTEGRATIONTYPE ProviderModel::integratonType() const
{
    return m_integrationtype;
}

QString ProviderModel::attribution() const
{
    return m_attribution;
}

bool ProviderModel::downloadOAuth2() const
{
    return m_doc["downloadOAuth2"].toBool(false);
}

QUrl ProviderModel::getSearchUrl(const QString &searchText, const int page) {

    QUrl url(m_apiroot);
    const QJsonObject req = m_search["req"].toObject();

    url.setPath(url.path().append(req["path"].toString()));

    auto parseValue = [&](QString value) {
        value = value.replace("%query%", searchText);
        value = value.replace("%pagenum%", QString::number(page));
        value = value.replace("%perpage%", QString::number(m_perPage));
        value = value.replace("%shortlocale%", "en-US"); //TODO
        value = value.replace("%clientkey%", m_clientkey);

        return value;
    };

    QUrlQuery query;

    for (const auto param : req["params"].toArray()) {
        query.addQueryItem(param.toObject()["key"].toString(), parseValue(param.toObject()["value"].toString()));
    }


    url.setQuery(query);

    return url;
}

void ProviderModel::slotStartSearch(const QString &searchText, int page)
{
    QUrl uri = getSearchUrl(searchText, page);
    //  qCDebug(KDENLIVE_LOG)<<uri;

    auto parseValue = [&](QString value) {
        value = value.replace("%shortlocale%", "en-US"); //TODO
        value = value.replace("%clientkey%", m_clientkey);

        return value;
    };

    if(m_search["req"].toObject()["method"].toString() == "GET") {

        QNetworkAccessManager *manager = new QNetworkAccessManager(this);

        QNetworkRequest request(uri);

        if(m_search["req"].toObject()["header"].isArray()) {
            for (const auto &header: m_search["req"].toObject()["header"].toArray()) {
                request.setRawHeader(header.toObject()["key"].toString().toUtf8(), parseValue(header.toObject()["value"].toString()).toUtf8());
            }
        }
        QNetworkReply *reply = manager->get(request);

        connect(reply, &QNetworkReply::finished, [=]() {
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

std::pair<QList<ResourceItemInfo>, const int> ProviderModel::parseSearchResponse(const QByteArray &data) {
    QJsonObject keys = m_search["res"].toObject();
    QList<ResourceItemInfo> list;
    int pageCount = 0;
    if(keys["format"].toString() == "json") {
        QJsonDocument res = QJsonDocument::fromJson(data);
        auto parse = [&](QJsonObject item, QString key) {

            QJsonObject tmpKeys = keys;
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
            // TODO "$" means template, store template instead of using as key
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

            // "%" means template, store template instead of using as key
            if(parseKey.startsWith("%")) {
                return tmpKeys[key];
            }

            return item[parseKey];
        };

        auto parseString = [&](QJsonObject item, QString key) {
            QJsonValue val = parse(item, key);
            if(!val.isString()) {
                return QString();
            }
            QString result = val.toString();

            if(result.startsWith("$")) {
                QStringList sections = result.split("{");
                for (auto &section : sections) {
                    section.remove("{");
                    section.remove(section.indexOf("}"), section.length());
                    result.replace("{" + section + "}", item[section].isDouble() ? QString::number(item[section].toDouble()) : item[section].toString());
                }
                result.remove("$");
            }

            return result;
        };

        QJsonArray items;
        if(keys["list"].toString("").isEmpty()) {
            items = res.array();
        } else {
            items = parse(res.object(),"list").toArray();
        }

        pageCount = parse(res.object(), "resultCount").toInt() / m_perPage;

        for (const auto &item : qAsConst(items)) {
            ResourceItemInfo onlineItem;
            onlineItem.author = parseString(item.toObject(), "author");
            onlineItem.authorUrl = parseString(item.toObject(), "authorUrl");
            onlineItem.name = parseString(item.toObject(), "name");
            onlineItem.filetype = parseString(item.toObject(), "filetype");
            onlineItem.description = parseString(item.toObject(), "description");
            onlineItem.id = (parse(item.toObject(), "id").isString() ? parseString(item.toObject(), "id") : QString::number(parse(item.toObject(), "id").toInt()));
            onlineItem.infoUrl = parseString(item.toObject(), "url");
            onlineItem.license = parseString(item.toObject(), "license");
            onlineItem.attributionText = parseString(item.toObject(), "attributionText");
            onlineItem.imageUrl = parseString(item.toObject(), "imageUrl");
            onlineItem.previewUrl = parseString(item.toObject(), "previewUrl");
            onlineItem.width = parse(item.toObject(), "width").toInt();
            onlineItem.height = parse(item.toObject(), "height").toInt();
            onlineItem.duration = parse(item.toObject(), "duration").isDouble() ? (int) parse(item.toObject(), "duration").toDouble() : parse(item.toObject(), "duration").toInt();

            if(keys["downloadUrls"].isObject()) {
                for (const auto urlItem : parse(item.toObject(), "downloadUrls.key").toArray()) {
                    onlineItem.downloadUrls << parseString(urlItem.toObject(), "downloadUrls.url");
                    onlineItem.downloadLabels << parseString(urlItem.toObject(), "downloadUrls.name");
                }
                if (onlineItem.previewUrl.isEmpty()) {
                    onlineItem.previewUrl = onlineItem.downloadUrls.first();
                }
            } else if(keys["downloadUrl"].isString()){
                onlineItem.downloadUrl = parseString(item.toObject(), "downloadUrl");
                if (onlineItem.previewUrl.isEmpty()) {
                    onlineItem.previewUrl = onlineItem.downloadUrl;
                }
            }
            /*qDebug() << " < < < < < < < < < < < < < < < ";
            qDebug() << " < Name: " << onlineItem.name;
            qDebug() << " < Description: " << onlineItem.description;
            qDebug() << " < ID: " << onlineItem.id;
            qDebug() << " < URL: " << onlineItem.infoUrl;
            qDebug() << " < License: " << onlineItem.license;
            qDebug() << " < Attribution: " << onlineItem.attributionText;
            qDebug() << " < Author: " << onlineItem.author;
            qDebug() << " < Author URL: " << onlineItem.authorUrl;
            qDebug() << " < Size: " << onlineItem.width << " x " << onlineItem.height;
            qDebug() << " < Duration: " << onlineItem.duration;
            qDebug() << " < Image URL: " << onlineItem.imageUrl;
            qDebug() << " < Preview URL: " << onlineItem.previewUrl;
            qDebug() << " < Download URL: " << onlineItem.downloadUrl;*/

            list << onlineItem;
        }
    } else {
        qCWarning(KDENLIVE_LOG) << "WARNING: unknown response format: " << keys["format"];
    }
    return std::pair<QList<ResourceItemInfo>, const int> (list, pageCount);
}

QUrl ProviderModel::getFilesUrl(const QString &id) {

    QUrl url(m_apiroot);
    if(!m_download["req"].isObject()) {
        return QUrl();
    }
    const QJsonObject req = m_download["req"].toObject();

    auto parseValue = [&](QString value) {
        value = value.replace("%id%", id);
        value = value.replace("%clientkey%", m_clientkey);

        return value;
    };

    url.setPath(url.path().append(parseValue(req["path"].toString())));

    QUrlQuery query;

    for (const auto param : req["params"].toArray()) {
        query.addQueryItem(param.toObject()["key"].toString(), parseValue(param.toObject()["value"].toString()));
    }

    url.setQuery(query);

    return url;
}

void ProviderModel::slotFetchFiles(const QString &id) {

    QUrl uri = getFilesUrl(id);

    if(uri.isEmpty()) {
        return;
    }

    auto parseValue = [&](QString value) {
        value = value.replace("%shortlocale%", "en-US"); //TODO
        value = value.replace("%clientkey%", m_clientkey);

        return value;
    };

    if(m_download["req"].toObject()["method"].toString() == "GET") {

        QNetworkAccessManager *manager = new QNetworkAccessManager(this);

        QNetworkRequest request(uri);

        if(m_download["req"].toObject()["header"].isArray()) {
            for (const auto &header: m_search["req"].toObject()["header"].toArray()) {
                request.setRawHeader(header.toObject()["key"].toString().toUtf8(), parseValue(header.toObject()["value"].toString()).toUtf8());
            }
        }
        QNetworkReply *reply = manager->get(request);

        connect(reply, &QNetworkReply::finished, [=]() {
            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray response = reply->readAll();
                std::pair<QStringList, QStringList> result = parseFilesResponse(response, id);
                emit fetchedFiles(result.first, result.second);
                reply->deleteLater();
            }
            else // handle error
            {
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

std::pair<QStringList, QStringList> ProviderModel::parseFilesResponse(const QByteArray &data, const QString &id) {
    QJsonObject keys = m_download["res"].toObject();
    QStringList urls;
    QStringList labels;

    if(keys["format"].toString() == "json") {
        QJsonObject res = QJsonDocument::fromJson(data).object();
        auto parse = [&](QJsonObject item, QString key) {

            QJsonObject tmpKeys = keys;
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
            // TODO "$" means template, store template instead of using as key
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

            // "%" means template, store template instead of using as key
            if(parseKey.startsWith("%") || parseKey.startsWith("&")) {
                return tmpKeys[key];
            }
            return item[parseKey];
        };

        auto parseString = [&](QJsonObject item, QString key, QString parentKey = QString()) {
            QJsonValue val = parse(item, key);
            if(!val.isString()) {
                return QString();
            }
            QString result = val.toString("");

            if(result.startsWith("$")) {
                result = result.replace("%id%", id);
                QStringList sections = result.split("{");
                for (auto &section : sections) {
                    section.remove("{");
                    section.remove(section.indexOf("}"), section.length());
                    if(section.startsWith("&")) {
                        result.replace("{" + section + "}", parentKey);
                    } else {
                        result.replace("{" + section + "}", item[section].isDouble() ? QString::number(item[section].toDouble()) : item[section].toString());
                    }
                }
                result.remove("$");
            }
            return result;
        };


        if(keys["downloadUrls"].isObject()) {
            if(keys["downloadUrls"].toObject()["isObject"].toBool(false)) {
                QJsonObject list = parse(res, "downloadUrls.key").toObject();
                for (const auto key : list.keys()) {
                    QJsonObject urlItem = list[key].toObject();
                    QString format = parseString(urlItem, "downloadUrls.format", key);
                    //This ugly check is only for the complicated archive.org api to avoid a long file list for videos caused by thumbs and metafiles
                    if(m_type == ProviderModel::VIDEO && m_homepage == "https://archive.org" && format != QLatin1String("Animated GIF") && format != QLatin1String("Metadata")
                            && format != QLatin1String("Archive BitTorrent") && format != QLatin1String("Thumbnail") && format != QLatin1String("JSON")
                            && format != QLatin1String("JPEG") && format != QLatin1String("JPEG Thumb") && format != QLatin1String("PNG")
                            && format != QLatin1String("Video Index")) {
                        urls << parseString(urlItem, "downloadUrls.url", key);
                        labels << parseString(urlItem, "downloadUrls.name", key);
                    }


                }
            } else {
                for (const auto urlItem : parse(res, "downloadUrls.key").toArray()) {
                    urls << parseString(urlItem.toObject(), "downloadUrls.url");
                    labels << parseString(urlItem.toObject(), "downloadUrls.name");
                }
            }

        } else if(keys["downloadUrl"].isString()){
            urls << parseString(res, "downloadUrl");
        }

    } else {
        qCWarning(KDENLIVE_LOG) << "WARNING: unknown response format: " << keys["format"];
    }
    return std::pair<QStringList, QStringList> (urls, labels);
}
