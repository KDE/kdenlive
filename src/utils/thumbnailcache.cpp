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

#include "thumbnailcache.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include <QDir>
#include <QMutexLocker>
#include <list>

std::unique_ptr<ThumbnailCache> ThumbnailCache::instance;
std::once_flag ThumbnailCache::m_onceFlag;

class ThumbnailCache::Cache_t
{
public:
    Cache_t(int maxCost)
        : m_maxCost(maxCost)
    {
    }

    bool contains(const QString &key) const { return m_cache.count(key) > 0; }

    void remove(const QString &key)
    {
        if (!contains(key)) {
            return;
        }
        auto it = m_cache.at(key);
        m_currentCost -= (*it).second.second;
        m_data.erase(it);
        m_cache.erase(key);
    }

    void insert(const QString &key, const QImage &img, int cost)
    {
        if (cost > m_maxCost) {
            return;
        }
        m_data.push_front({key, {img, cost}});
        auto it = m_data.begin();
        m_cache[key] = it;
        m_currentCost += cost;
        while (m_currentCost > m_maxCost) {
            remove(m_data.back().first);
        }
    }

    QImage get(const QString &key)
    {
        if (!contains(key)) {
            return QImage();
        }
        // when a get operation occurs, we put the corresponding list item in front to remember last access
        std::pair<QString, std::pair<QImage, int>> data;
        auto it = m_cache.at(key);
        std::swap(data, (*it));                                         // take data out without copy
        QImage result = data.second.first;                              // a copy occurs here
        m_data.erase(it);                                               // delete old iterator
        m_cache[key] = m_data.emplace(m_data.begin(), std::move(data)); // reinsert without copy and store iterator
        return result;
    }

protected:
    int m_maxCost;
    int m_currentCost{0};

    std::list<std::pair<QString, std::pair<QImage, int>>> m_data; // the data is stored as (key,(image, cost))
    std::unordered_map<QString, decltype(m_data.begin())> m_cache;
};

ThumbnailCache::ThumbnailCache()
    : m_volatileCache(new Cache_t(10000000))
{
}

std::unique_ptr<ThumbnailCache> &ThumbnailCache::get()
{
    std::call_once(m_onceFlag, [] { instance.reset(new ThumbnailCache()); });
    return instance;
}

bool ThumbnailCache::hasThumbnail(const QString &binId, int pos, bool volatileOnly) const
{
    QMutexLocker locker(&m_mutex);
    auto key = getKey(binId, pos);
    if (m_volatileCache->contains(key)) {
        return true;
    }
    if (volatileOnly) {
        return false;
    }
    bool ok = false;
    QDir thumbFolder = getDir(&ok);
    return ok && thumbFolder.exists(key);
}

QImage ThumbnailCache::getThumbnail(const QString &binId, int pos, bool volatileOnly) const
{
    QMutexLocker locker(&m_mutex);
    auto key = getKey(binId, pos);
    if (m_volatileCache->contains(key)) {
        return m_volatileCache->get(key);
    }
    if (volatileOnly) {
        return QImage();
    }
    bool ok = false;
    QDir thumbFolder = getDir(&ok);
    if (ok && thumbFolder.exists(key)) {
        return QImage(thumbFolder.absoluteFilePath(key));
    }
    return QImage();
}

void ThumbnailCache::storeThumbnail(const QString &binId, int pos, const QImage &img, bool persistent)
{
    QMutexLocker locker(&m_mutex);
    auto key = getKey(binId, pos);
    if (persistent) {
        bool ok = false;
        QDir thumbFolder = getDir(&ok);
        if (ok) {
            img.save(thumbFolder.absoluteFilePath(key));
        }
    } else {
        m_volatileCache->insert(key, img, img.byteCount());
        m_storedVolatile[binId].push_back(pos);
    }
}

void ThumbnailCache::invalidateThumbsForClip(const QString &binId)
{
    QMutexLocker locker(&m_mutex);
    for (int pos : m_storedVolatile.at(binId)) {
        auto key = getKey(binId, pos);
        m_volatileCache->remove(key);
    }
    m_storedVolatile.erase(binId);

    Q_ASSERT(false);
    // TODO implement the invalidation for persistent cache
}

// static
QString ThumbnailCache::getKey(const QString &binId, int pos)
{
    auto binClip = pCore->projectItemModel()->getClipByBinID(binId);
    return binClip->hash() + QLatin1Char('#') + QString::number(pos) + QStringLiteral(".png");
}

// static
QDir ThumbnailCache::getDir(bool *ok)
{
    return pCore->currentDoc()->getCacheDir(CacheThumbs, ok);
}
