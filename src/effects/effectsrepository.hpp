/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef EFFECTSREPOSITORY_H
#define EFFECTSREPOSITORY_H

#include "assets/abstractassetsrepository.hpp"
#include "definitions.h"
#include <memory>
#include <mutex>
#include <unordered_map>

/** @class EffectsRepository
    @brief This class stores all the effects that can be added by the user.
    You can query any effect based on its name.
    Note that this class is a Singleton
 */
class EffectsRepository : public AbstractAssetsRepository<AssetListType::AssetType>
{

public:
    /** @brief Returns the instance of the Singleton */
    static std::unique_ptr<EffectsRepository> &get();

    /** @brief returns a fresh instance of the given effect */
    std::unique_ptr<Mlt::Filter> getEffect(const QString &effectId) const;
    /** @brief returns true if an effect exists in MLT (bypasses the blacklist/metadata parsing) */
    bool hasInternalEffect(const QString &effectId) const;
    QPair<QString, QString> reloadCustom(const QString &path);
    QString getCustomPath(const QString &id);

    /** @brief Returns whether this belongs to main effects */
    bool isPreferred(const QString &effectId) const;

    /** @brief Check custom effects (older custom effects need an update to default and current values
    *  returns a list of effects that were incorrectly converted */
    QPair<QStringList, QStringList> fixDeprecatedEffects();
    /** @brief Returns true if this is an effect group */
    bool isGroup(const QString &assetId) const;
    void deleteEffect(const QString &id);
    bool isAudioEffect(const QString &assetId) const;

protected:
    /** @brief Constructor is protected because class is a Singleton */
    EffectsRepository();

    /** @brief Retrieves the list of all available effects from Mlt */
    Mlt::Properties *retrieveListFromMlt() const override;

    /** @brief Retrieves additional info about effects from a custom XML file
       The resulting assets are stored in customAssets
    */
    void parseCustomAssetFile(const QString &file_name, std::unordered_map<QString, Info> &customAssets) const override;

    /** @brief Returns the path to the effects' blacklist*/
    QString assetBlackListPath() const override;

    /** @brief Returns the path to the effects' preferred list*/
    QString assetPreferredListPath() const override;

    QStringList assetDirs() const override;

    void parseType(QScopedPointer<Mlt::Properties> &metadata, Info &res) override;

    /** @brief Returns the metadata associated with the given asset*/
    Mlt::Properties *getMetadata(const QString &assetId) const override;

    QPair <QString, QString> fixCustomAssetFile(const QString &path);

    static std::unique_ptr<EffectsRepository> instance;

    /** @brief flag to create the repository only once */
    static std::once_flag m_onceFlag;
};

#endif
