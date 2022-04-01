/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
