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
#include <QString>
#include <QStandardPaths>
#include <QTextStream>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

template <typename AssetType> AbstractAssetsRepository<AssetType>::AbstractAssetsRepository()
{
}

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
            res.name = metadata->get("title");
            res.name[0] = res.name[0].toUpper();
            res.description = metadata->get("description");
            res.author = metadata->get("creator");
            res.version_str = metadata->get("version");
            res.version = metadata->get_double("version");
            res.id = res.mltId = assetId;
            parseType(metadata, res);
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

template <typename AssetType> bool AbstractAssetsRepository<AssetType>::isFavorite(const QString & /*assetId*/) const
{
    // TODO
    return true;
}

template <typename AssetType> bool AbstractAssetsRepository<AssetType>::parseInfoFromXml(const QDomElement &currentAsset, Info &res) const
{
    QLocale locale;

    // We first deal with locale
    if (currentAsset.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
        // set a locale for that effect
        locale = QLocale(currentAsset.attribute(QStringLiteral("LC_NUMERIC")));
    }
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

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
        if (locale.toDouble(currentAsset.attribute(QStringLiteral("version"))) > m_assets.at(tag).version) {
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
    return m_assets.at(assetId).xml;
}
