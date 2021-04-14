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

#ifndef PROVIDERSREPOSITORY_H
#define PROVIDERSREPOSITORY_H

#include "definitions.h"
#include "providermodel.hpp"
#include <QReadWriteLock>
#include <mutex>
#include <unordered_map>

/** @class ProvidersRepository
    @brief This class is used to read all the provider configs available.
    You can then query providers based on their paths
    Note that this class is a Singleton, with Mutex protections to allow concurrent access.
 */
class ProvidersRepository
{
public:
    // Returns the instance of the Singleton
    static std::unique_ptr<ProvidersRepository> &get();

    /** @brief Reloads all the providers from the disk */
    void refresh(bool fullRefresh = true); //TODO: change to false

    QVector<QPair<QString,QString>> getAllProviers() const;
    std::unique_ptr<ProviderModel> &getProvider(const QString &path);


protected:
    ProvidersRepository();

    static std::unique_ptr<ProvidersRepository> instance;
    /** @brief flag to create the repository only once */
    static std::once_flag m_onceFlag;

    mutable QReadWriteLock m_mutex;

    std::unordered_map<QString, std::unique_ptr<ProviderModel>> m_providers;

};

#endif // PROVIDERSREPOSITORY_H
