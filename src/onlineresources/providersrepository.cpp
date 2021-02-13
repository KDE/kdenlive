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

#include "providersrepository.hpp"
#include <QStandardPaths>
#include <QDir>

std::unique_ptr<ProvidersRepository> ProvidersRepository::instance;
std::once_flag ProvidersRepository::m_onceFlag;
ProvidersRepository::ProvidersRepository()
{
    refresh();
}

std::unique_ptr<ProvidersRepository> &ProvidersRepository::get()
{
    std::call_once(m_onceFlag, [] { instance.reset(new ProvidersRepository()); });
    return instance;
}

void ProvidersRepository::refresh(bool fullRefresh) {

    //Lock to avoid changes to config files while reading them
    QWriteLocker locker(&m_mutex);

    if (fullRefresh) {
        // Reset all providers
        m_providers.clear();
    }

    // Helper function to check a provider and print debug info
    auto check_provider = [&](std::unique_ptr<ProviderModel> &provider, const QString &file) {
        if (m_providers.count(file) > 0) {
            return false;
        }
        if (!provider->is_valid()) {
            qCWarning(KDENLIVE_LOG) << "//// WARNING: invalid provider found: " << file << ". Ignoring.";
            return false;
        }
        return true;
    };

    QStringList profilesFiles;
    // list providers
    QStringList customProfilesDir = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("resourceproviders"), QStandardPaths::LocateDirectory);
    for (const auto &dir : qAsConst(customProfilesDir)) {
        QStringList files = QDir(dir).entryList(QDir::Files);
        for (const auto &file : qAsConst(files)) {
            profilesFiles << QDir(dir).absoluteFilePath(file);
        }
    }

    // Iterate through files
    for (const auto &file : qAsConst(profilesFiles)) {
        std::unique_ptr<ProviderModel> provider(new ProviderModel(file));
        if (check_provider(provider, file)) {
            m_providers.insert(std::make_pair(file, std::move(provider)));
        }
    }
}

QVector<QPair<QString, QString>> ProvidersRepository::getAllProviers() const
{
    QReadLocker locker(&m_mutex);

    QVector<QPair<QString, QString>> list;
    for (const auto &provider : m_providers) {
        list.push_back({provider.second->name(), provider.first});
    }
    std::sort(list.begin(), list.end());
    return list;
}

std::unique_ptr<ProviderModel> &ProvidersRepository::getProvider(const QString &path)
{
    QReadLocker locker(&m_mutex);
    if (m_providers.count(path) == 0) {
        return (*(m_providers.begin())).second;
    }
    return m_providers.at(path);
}
