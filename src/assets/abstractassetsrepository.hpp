/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QSet>
#include <memory>
#include <mlt++/Mlt.h>
#include <mutex>
#include <unordered_map>

/** @class AbstractAssetsRepository
    @brief This class is the base class for assets (transitions or effets) repositories
 */
template <typename AssetType> class AbstractAssetsRepository
{

public:
    AbstractAssetsRepository();
    virtual ~AbstractAssetsRepository() = default;

    /** @brief Returns true if a given asset exists
     */
    bool exists(const QString &assetId) const;

    /** @brief Returns a vector of pair (asset id, asset name)
     */
    QVector<QPair<QString, QString>> getNames() const;

    /** @brief Return type of asset */
    AssetType getType(const QString &assetId) const;
    bool isIncludedInList(const QString &assetId) const;

    void getAttributes(const QString &assetId, bool *isPreferred, bool *isIncluded, bool *supportsTenBit);

    /** @brief Return type of asset */
    bool isUnique(const QString &assetId) const;

    /** @brief Return name of asset */
    Q_INVOKABLE QString getName(const QString &assetId) const;

    /** @brief Return description of asset */
    QString getDescription(const QString &assetId) const;
    /** @brief Return version of asset */
    int getVersion(const QString &assetId) const;

    /** @brief Returns a DomElement representing the asset's properties */
    QDomElement getXml(const QString &assetId) const;

protected:
    struct Info
    {
        QString id;    // identifier of the asset
        QString mltId; //"tag" of the asset, that is the name of the mlt service
        QString name, description, author, version_str;
        int version{};
        bool included{false};
        QMap<QString, QVariant> features;
        QDomElement xml;
        AssetType type;
    };

    // Reads the asset list from file and populates appropriate structure
    void parseAssetList(const QStringList &filePaths, QSet<QString> &destination);

    void init();
    virtual Mlt::Properties *retrieveListFromMlt() const = 0;

    /** @brief Parse some info from a mlt structure
       @param res Datastructure to fill
       @return true on success
    */
    bool parseInfoFromMlt(const QString &assetId, Info &res);

    /** @brief Returns the metadata associated with the given asset*/
    virtual Mlt::Properties *getMetadata(const QString &assetId) const = 0;

    /** @brief Parse one asset from its XML content
       @param res data structure to fill
       @return true of success
     */
    bool parseInfoFromXml(const QDomElement &assetXml, Info &res) const;

    /** @brief Define default features for assets
       @param res data structure to fill
     */
    void setDefaultFeatures(Info &res) const;

    /** @brief Figure what is the type of the asset based on its metadata and store it in res*/
    virtual void parseType(Mlt::Properties *metadata, Info &res) = 0;

    /** @brief Retrieves additional info about asset from a custom XML file
       The resulting assets are stored in customAssets
     */
    virtual void parseCustomAssetFile(const QString &file_name, std::unordered_map<QString, Info> &customAssets) const = 0;

    /** @brief Returns the path to custom XML description of the assets*/
    virtual QStringList assetDirs() const = 0;

    /** @brief Returns the path to the asset list that will be displayed*/
    virtual QStringList assetIncludedPath() const = 0;

    /** @brief Returns the path to the asset list that will be hidden*/
    virtual QStringList assetExcludedPath() const = 0;

    /** @brief Returns the path to the assets' preferred list*/
    virtual QString assetPreferredListPath() const = 0;

    std::unordered_map<QString, Info> m_assets;

    QSet<QString> m_excludedList;
    QSet<QString> m_includedList;

    QSet<QString> m_preferred_list;
};

#include "abstractassetsrepository.ipp"
