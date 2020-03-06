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

#ifndef TRANSITIONSREPOSITORY_H
#define TRANSITIONSREPOSITORY_H

#include "assets/abstractassetsrepository.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "definitions.h"
#include <memory>
#include <mutex>

/** @brief This class stores all the transitions that can be added by the user.
 * You can query any transitions based on its name.
 * Note that this class is a Singleton
 */

class TransitionsRepository : public AbstractAssetsRepository<AssetListType::AssetType>
{

public:
    // Returns the instance of the Singleton
    static std::unique_ptr<TransitionsRepository> &get();

    /* @brief Creates and return an instance of a transition given its id.
     */
    std::unique_ptr<Mlt::Transition> getTransition(const QString &transitionId) const;

    /* @brief returns true if the transition corresponding to @transitionId is a composition*/
    bool isComposition(const QString &transitionId) const;

    /* @brief Returns the id of the transition to be used for compositing */
    const QString getCompositingTransition();

protected:
    // Constructor is protected because class is a Singleton
    TransitionsRepository();

    /* Retrieves the list of all available effects from Mlt*/
    Mlt::Properties *retrieveListFromMlt() const override;

    /* @brief Retrieves additional info about effects from a custom XML file
       The resulting assets are stored in customAssets
     */
    void parseCustomAssetFile(const QString &file_name, std::unordered_map<QString, Info> &customAssets) const override;

    /* @brief Returns the paths where the custom transitions' descriptions are stored */
    QStringList assetDirs() const override;

    /* @brief Returns the path to the transitions' blacklist*/
    QString assetBlackListPath() const override;

    /* @brief Returns the path to the effects' preferred list*/
    QString assetPreferredListPath() const override;

    void parseType(QScopedPointer<Mlt::Properties> &metadata, Info &res) override;

    /* @brief Returns the metadata associated with the given asset*/
    Mlt::Properties *getMetadata(const QString &assetId) const override;

    /* @brief Returns all transitions that can be represented as Single Track Transitions*/
    static QSet<QString> getSingleTrackTransitions();

    static std::unique_ptr<TransitionsRepository> instance;
    static std::once_flag m_onceFlag; // flag to create the repository only once;
};

#endif
