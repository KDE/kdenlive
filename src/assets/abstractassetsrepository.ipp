/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "xml/xml.hpp"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

template <typename AssetType> AbstractAssetsRepository<AssetType>::AbstractAssetsRepository() = default;

template <typename AssetType> void AbstractAssetsRepository<AssetType>::init()
{
// Warning: Mlt::Factory::init() resets the locale to the default system value, make sure we keep correct locale
#ifndef Q_OS_MAC
    setlocale(LC_NUMERIC, nullptr);
#else
    setlocale(LC_NUMERIC_MASK, nullptr);
#endif

    // Parse effects blacklist
    parseBlackList(assetBlackListPath());

    // Retrieve the list of MLT's available assets.
    QScopedPointer<Mlt::Properties> assets(retrieveListFromMlt());
    int max = assets->count();
    QString sox = QStringLiteral("sox.");
    for (int i = 0; i < max; ++i) {
        Info info;
        QString name = assets->get_name(i);
        info.id = name;
        if (name.startsWith(sox)) {
            // sox effects are not usage directly (parameters not available)
            continue;
        }
        // qDebug() << "trying to parse " <<name <<" blacklist="<<m_blacklist.contains(name);
        if (!m_blacklist.contains(name) && parseInfoFromMlt(name, info)) {
            m_assets[name] = info;
        } else {
            if (m_blacklist.contains(name)) {
                qDebug() << name << "is blacklisted";
            } else {
                qDebug() << "WARNING : Fails to parse " << name;
            }
        }
    }

    // We now parse custom effect xml

    // Set the directories to look into for effects.
    QStringList asset_dirs = assetDirs();

    /* Parsing of custom xml works as follows: we parse all custom files.
       Each of them contains a tag, which is the corresponding mlt asset, and an id that is the name of the asset. Note that several custom files can correspond
       to the same tag, and in that case they must have different ids. We do the parsing in a map from ids to parse info, and then we add them to the asset
       list, while discarding the bare version of each tag (the one with no file associated)
    */
    std::unordered_map<QString, Info> customAssets;
    for (const auto &dir : asset_dirs) {
        QDir current_dir(dir);
        QStringList filter;
        filter << QStringLiteral("*.xml");
        QStringList fileList = current_dir.entryList(filter, QDir::Files);
        for (const auto &file : fileList) {
            QString path = current_dir.absoluteFilePath(file);
            parseCustomAssetFile(path, customAssets);
        }
    }

    // We add the custom assets
    for (const auto &custom : customAssets) {
        // Custom assets should override default ones
        m_assets[custom.first] = custom.second;
        /*if (m_assets.count(custom.second.mltId) > 0) {
            m_assets.erase(custom.second.mltId);
        }
        if (m_assets.count(custom.first) == 0) {
            m_assets[custom.first] = custom.second;
        } else {
            qDebug() << "Error: conflicting asset name " << custom.first;
        }*/
    }
}

template <typename AssetType> void AbstractAssetsRepository<AssetType>::parseBlackList(const QString &path)
{
    QFile blacklist_file(path);
    if (blacklist_file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&blacklist_file);
        QString line;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (!line.isEmpty() && !line.startsWith('#')) {
                m_blacklist.insert(line);
            }
        }
        blacklist_file.close();
    }
}

template <typename AssetType> bool AbstractAssetsRepository<AssetType>::parseInfoFromMlt(const QString &assetId, Info &res)
{
    QScopedPointer<Mlt::Properties> metadata(getMetadata(assetId));
    if (metadata && metadata->is_valid()) {
        if (metadata->get("title") && metadata->get("identifier") && strlen(metadata->get("title")) > 0) {
            QString id = metadata->get("identifier");
            res.name = metadata->get("title");
            res.name[0] = res.name[0].toUpper();
            res.description = metadata->get("description");
            res.description.append(QString(" (%1)").arg(id));
            res.author = metadata->get("creator");
            res.version_str = metadata->get("version");
            res.version = ceil(100 * metadata->get_double("version"));
            res.id = res.mltId = assetId;
            parseType(metadata, res);
            // Create params
            QDomDocument doc;
            QDomElement eff = doc.createElement(QStringLiteral("effect"));
            eff.setAttribute(QStringLiteral("tag"), id);
            eff.setAttribute(QStringLiteral("id"), id);
            ////qCDebug(KDENLIVE_LOG)<<"Effect: "<<id;

            Mlt::Properties param_props((mlt_properties)metadata->get_data("parameters"));
            for (int j = 0; param_props.is_valid() && j < param_props.count(); ++j) {
                QDomElement params = doc.createElement(QStringLiteral("parameter"));

                Mlt::Properties paramdesc((mlt_properties)param_props.get_data(param_props.get_name(j)));
                params.setAttribute(QStringLiteral("name"), paramdesc.get("identifier"));
                if (params.attribute(QStringLiteral("name")) == QLatin1String("argument")) {
                    // This parameter has to be given as attribute when using command line, do not show it in Kdenlive
                    continue;
                }

                if (paramdesc.get("readonly") && (strcmp(paramdesc.get("readonly"), "yes") == 0)) {
                    // Do not expose readonly parameters
                    continue;
                }

                if (paramdesc.get("maximum")) {
                    params.setAttribute(QStringLiteral("max"), paramdesc.get("maximum"));
                }
                if (paramdesc.get("minimum")) {
                    params.setAttribute(QStringLiteral("min"), paramdesc.get("minimum"));
                }

                QString paramType = paramdesc.get("type");
                if (paramType == QLatin1String("integer")) {
                    if (params.attribute(QStringLiteral("min")) == QLatin1String("0") && params.attribute(QStringLiteral("max")) == QLatin1String("1")) {
                        params.setAttribute(QStringLiteral("type"), QStringLiteral("bool"));
                    } else {
                        params.setAttribute(QStringLiteral("type"), QStringLiteral("constant"));
                    }
                } else if (paramType == QLatin1String("float")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("constant"));
                    // param type is float, set default decimals to 3
                    params.setAttribute(QStringLiteral("decimals"), QStringLiteral("3"));
                } else if (paramType == QLatin1String("boolean")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("bool"));
                } else if (paramType == QLatin1String("geometry")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("geometry"));
                } else if (paramType == QLatin1String("string")) {
                    // string parameter are not really supported, so if we have a default value, enforce it
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("fixed"));
                    if (paramdesc.get("default")) {
                        QString stringDefault = paramdesc.get("default");
                        stringDefault.remove(QLatin1Char('\''));
                        params.setAttribute(QStringLiteral("value"), stringDefault);
                    } else {
                        // String parameter without default, skip it completely
                        continue;
                    }
                } else {
                    params.setAttribute(QStringLiteral("type"), paramType);
                    if (!QString(paramdesc.get("format")).isEmpty()) {
                        params.setAttribute(QStringLiteral("format"), paramdesc.get("format"));
                    }
                }
                if (!params.hasAttribute(QStringLiteral("value"))) {
                    if (paramdesc.get("default")) {
                        params.setAttribute(QStringLiteral("default"), paramdesc.get("default"));
                    }
                    if (paramdesc.get("value")) {
                        params.setAttribute(QStringLiteral("value"), paramdesc.get("value"));
                    } else {
                        params.setAttribute(QStringLiteral("value"), paramdesc.get("default"));
                    }
                }
                QString paramName = paramdesc.get("title");
                if (!paramName.isEmpty()) {
                    QDomElement pname = doc.createElement(QStringLiteral("name"));
                    pname.appendChild(doc.createTextNode(paramName));
                    params.appendChild(pname);
                }
                if (paramdesc.get("description")) {
                    QDomElement comment = doc.createElement(QStringLiteral("comment"));
                    comment.appendChild(doc.createTextNode(paramdesc.get("description")));
                    params.appendChild(comment);
                }

                eff.appendChild(params);
            }
            doc.appendChild(eff);
            res.xml = eff;
            return true;
        }
    }
    return false;
}

template <typename AssetType> bool AbstractAssetsRepository<AssetType>::exists(const QString &assetId) const
{
    return m_assets.count(assetId) > 0;
}

template <typename AssetType> QVector<QPair<QString, QString>> AbstractAssetsRepository<AssetType>::getNames() const
{
    QVector<QPair<QString, QString>> res;
    res.reserve((int)m_assets.size());
    for (const auto &asset : m_assets) {
        res.push_back({asset.first, asset.second.name});
    }
    std::sort(res.begin(), res.end(), [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) { return a.second < b.second; });
    return res;
}

template <typename AssetType> AssetType AbstractAssetsRepository<AssetType>::getType(const QString &assetId) const
{
    Q_ASSERT(m_assets.count(assetId) > 0);
    return m_assets.at(assetId).type;
}

template <typename AssetType> QString AbstractAssetsRepository<AssetType>::getName(const QString &assetId) const
{
    Q_ASSERT(m_assets.count(assetId) > 0);
    return m_assets.at(assetId).name;
}

template <typename AssetType> QString AbstractAssetsRepository<AssetType>::getDescription(const QString &assetId) const
{
    Q_ASSERT(m_assets.count(assetId) > 0);
    return m_assets.at(assetId).description;
}

template <typename AssetType> bool AbstractAssetsRepository<AssetType>::parseInfoFromXml(const QDomElement &currentAsset, Info &res) const
{
    QString tag = currentAsset.attribute(QStringLiteral("tag"), QString());

    QString id = currentAsset.attribute(QStringLiteral("id"), QString());
    if (id.isEmpty()) {
        id = tag;
    }

    if (!exists(tag)) {
        qDebug() << "++++++ Unknown asset : " << tag;
        return false;
    }

    // Check if there is a maximal version set
    if (currentAsset.hasAttribute(QStringLiteral("version"))) {
        // a specific version of the filter is required
        if (m_assets.at(tag).version < (int)(100 * currentAsset.attribute(QStringLiteral("version")).toDouble())) {
            return false;
        }
    }

    res = m_assets.at(tag);
    res.id = id;
    res.mltId = tag;

    // Update description if the xml provide one
    QString description = Xml::getSubTagContent(currentAsset, QStringLiteral("description"));
    if (!description.isEmpty()) {
        res.description = description;
        res.description.append(QString(" (%1)").arg(tag));
    }

    // Update name if the xml provide one
    QString name = Xml::getSubTagContent(currentAsset, QStringLiteral("name"));
    if (!name.isEmpty()) {
        res.name = name;
    }
    return true;
}

template <typename AssetType> QDomElement AbstractAssetsRepository<AssetType>::getXml(const QString &assetId) const
{
    if (m_assets.count(assetId) == 0) {
        qDebug() << "Error : Requesting info on unknown transition " << assetId;
        return QDomElement();
    }
    return m_assets.at(assetId).xml.cloneNode().toElement();
}
