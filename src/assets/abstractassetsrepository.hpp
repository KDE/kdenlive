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

#ifndef ASSETSREPOSITORY_H
#define ASSETSREPOSITORY_H

#include "definitions.h"
#include <QSet>
#include <memory>
#include <mlt++/Mlt.h>
#include <mutex>
#include <unordered_map>

/** @brief This class is the base class for assets (transitions or effets) repositories
 */

template <typename AssetType> class AbstractAssetsRepository
{

public:
    AbstractAssetsRepository();
    virtual ~AbstractAssetsRepository() = default;

    /* @brief Returns true if a given asset exists
     */
    bool exists(const QString &assetId) const;

    /* @brief Returns a vector of pair (asset id, asset name)
     */
    QVector<QPair<QString, QString>> getNames() const;

    /* @brief Return type of asset */
    AssetType getType(const QString &assetId) const;
    
    /* @brief Return type of asset */
    bool isUnique(const QString &assetId) const;

    /* @brief Return name of asset */
    Q_INVOKABLE QString getName(const QString &assetId) const;

    /* @brief Return description of asset */
    QString getDescription(const QString &assetId) const;

    /* @brief Returns a DomElement representing the asset's properties */
    QDomElement getXml(const QString &assetId) const;

protected:
    struct Info
    {
        QString id;    // identifier of the asset
        QString mltId; //"tag" of the asset, that is the name of the mlt service
        QString name, description, author, version_str;
        int version{};
        QDomElement xml;
        AssetType type;
    };

    // Reads the asset list from file and populates appropriate structure
    void parseAssetList(const QString &filePath, QSet<QString> &destination);

    void init();
    virtual Mlt::Properties *retrieveListFromMlt() const = 0;

    /* @brief Parse some info from a mlt structure
       @param res Datastructure to fill
       @return true on success
    */
    bool parseInfoFromMlt(const QString &assetId, Info &res);

    /* @brief Returns the metadata associated with the given asset*/
    virtual Mlt::Properties *getMetadata(const QString &assetId) const = 0;

    /* @brief Parse one asset from its XML content
       @param res data structure to fill
       @return true of success
     */
    bool parseInfoFromXml(const QDomElement &currentAsset, Info &res) const;

    /* @brief Figure what is the type of the asset based on its metadata and store it in res*/
    virtual void parseType(QScopedPointer<Mlt::Properties> &metadata, Info &res) = 0;

    /* @brief Retrieves additional info about asset from a custom XML file
       The resulting assets are stored in customAssets
     */
    virtual void parseCustomAssetFile(const QString &file_name, std::unordered_map<QString, Info> &customAssets) const = 0;

    /* @brief Returns the path to custom XML description of the assets*/
    virtual QStringList assetDirs() const = 0;

    /* @brief Returns the path to the assets' blacklist*/
    virtual QString assetBlackListPath() const = 0;

    /* @brief Returns the path to the assets' preferred list*/
    virtual QString assetPreferredListPath() const = 0;

    std::unordered_map<QString, Info> m_assets;

    QSet<QString> m_blacklist;

    QSet<QString> m_preferred_list;
};

#include "abstractassetsrepository.ipp"

#endif
