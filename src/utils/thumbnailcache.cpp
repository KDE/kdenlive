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
    void clear()
    {
        m_data.clear();
        m_cache.clear();
        m_currentCost = 0;
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
    bool ok = false;
    auto key = pos < 0 ? getAudioKey(binId, &ok).first() : getKey(binId, pos, &ok);
    if (ok && m_volatileCache->contains(key)) {
        return true;
    }
    if (!ok || volatileOnly) {
        return false;
    }
    QDir thumbFolder = getDir(pos < 0, &ok);
    return ok && thumbFolder.exists(key);
}

QImage ThumbnailCache::getAudioThumbnail(const QString &binId, bool volatileOnly) const
{
    QMutexLocker locker(&m_mutex);
    bool ok = false;
    auto key = getAudioKey(binId, &ok).first();
    if (ok && m_volatileCache->contains(key)) {
        return m_volatileCache->get(key);
    }
    if (!ok || volatileOnly) {
        return QImage();
    }
    QDir thumbFolder = getDir(true, &ok);
    if (ok && thumbFolder.exists(key)) {
        m_storedOnDisk[binId].push_back(-1);
        return QImage(thumbFolder.absoluteFilePath(key));
    }
    return QImage();
}

const QList <QUrl> ThumbnailCache::getAudioThumbPath(const QString &binId) const
{
    QMutexLocker locker(&m_mutex);
    bool ok = false;
    auto key = getAudioKey(binId, &ok);
    QDir thumbFolder = getDir(true, &ok);
    QList <QUrl> pathList;
    if (ok) {
        for (const QString &p : qAsConst(key)) {
            if (thumbFolder.exists(p)) {
                pathList <<QUrl::fromLocalFile(thumbFolder.absoluteFilePath(p));
            }
        }
    }
    return pathList;
}

QImage ThumbnailCache::getThumbnail(const QString &binId, int pos, bool volatileOnly) const
{
    QMutexLocker locker(&m_mutex);
    bool ok = false;
    auto key = getKey(binId, pos, &ok);
    if (ok && m_volatileCache->contains(key)) {
        return m_volatileCache->get(key);
    }
    if (!ok || volatileOnly) {
        return QImage();
    }
    QDir thumbFolder = getDir(false, &ok);
    if (ok && thumbFolder.exists(key)) {
        m_storedOnDisk[binId].push_back(pos);
        return QImage(thumbFolder.absoluteFilePath(key));
    }
    return QImage();
}

void ThumbnailCache::storeThumbnail(const QString &binId, int pos, const QImage &img, bool persistent)
{
    QMutexLocker locker(&m_mutex);
    bool ok = false;
    const QString key = getKey(binId, pos, &ok);
    if (!ok) {
        return;
    }
    if (persistent) {
        QDir thumbFolder = getDir(false, &ok);
        if (ok) {
            if (!img.save(thumbFolder.absoluteFilePath(key))) {
                qDebug() << ".............\n!!!!!!!! ERROR SAVING THUMB in: "<<thumbFolder.absoluteFilePath(key);
            }
            m_storedOnDisk[binId].push_back(pos);
            // if volatile cache also contains this entry, update it
            if (m_volatileCache->contains(key)) {
                m_volatileCache->remove(key);
            } else {
                m_storedVolatile[binId].push_back(pos);
            }
            m_volatileCache->insert(key, img, (int)img.sizeInBytes());
        }
    } else {
        m_volatileCache->insert(key, img, (int)img.sizeInBytes());
        m_storedVolatile[binId].push_back(pos);
    }
}

void ThumbnailCache::saveCachedThumbs(QStringList keys)
{
    bool ok;
    QDir thumbFolder = getDir(false, &ok);
    if (!ok) {
        return;
    }
    for (const QString &key : keys) {
        if (!thumbFolder.exists(key) && m_volatileCache->contains(key)) {
            QImage img = m_volatileCache->get(key);
            if (!img.save(thumbFolder.absoluteFilePath(key))) {
                qDebug() << "// Error writing thumbnails to " << thumbFolder.absolutePath();
                break;
            }
        }
    }
}

void ThumbnailCache::invalidateThumbsForClip(const QString &binId)
{
    QMutexLocker locker(&m_mutex);
    if (m_storedVolatile.find(binId) != m_storedVolatile.end()) {
        bool ok = false;
        for (int pos : m_storedVolatile.at(binId)) {
            auto key = getKey(binId, pos, &ok);
            if (ok) {
                m_volatileCache->remove(key);
            }
        }
        m_storedVolatile.erase(binId);
    }
    bool ok = false;
    // Video thumbs
    QDir thumbFolder = getDir(false, &ok);
    QDir audioThumbFolder = getDir(true, &ok);
    if (ok && m_storedOnDisk.find(binId) != m_storedOnDisk.end()) {
        // Remove persistent cache
        for (int pos : m_storedOnDisk.at(binId)) {
            if (pos >= 0) {
                auto key = getKey(binId, pos, &ok);
                if (ok) {
                    QFile::remove(thumbFolder.absoluteFilePath(key));
                }
            }
        }
        m_storedOnDisk.erase(binId);
    }
}

void ThumbnailCache::clearCache()
{
    QMutexLocker locker(&m_mutex);
    m_volatileCache->clear();
    m_storedVolatile.clear();
    m_storedOnDisk.clear();
}

// static
QString ThumbnailCache::getKey(const QString &binId, int pos, bool *ok)
{
    if (binId.isEmpty()) {
        *ok = false;
        return QString();
    }
    auto binClip = pCore->projectItemModel()->getClipByBinID(binId);
    *ok = binClip != nullptr;
    return *ok ? binClip->hash() + QLatin1Char('#') + QString::number(pos) + QStringLiteral(".jpg") : QString();
}

// static
QStringList ThumbnailCache::getAudioKey(const QString &binId, bool *ok)
{
    auto binClip = pCore->projectItemModel()->getClipByBinID(binId);
    *ok = binClip != nullptr;
    if (ok) {
        QString streams = binClip->getProducerProperty(QStringLiteral("kdenlive:active_streams"));
        if (streams == QString::number(INT_MAX)) {
            // activate all audio streams
            QList <int> streamIxes = binClip->audioStreams().keys();
            if (streamIxes.size() > 1) {
                QStringList streamsList;
                for (const int st : qAsConst(streamIxes)) {
                    streamsList << QString("%1_%2.png").arg(binClip->hash()).arg(st);
                }
                return streamsList;
            }
        }
        if (streams.size() < 2) {
            int audio = binClip->getProducerIntProperty(QStringLiteral("audio_index"));
            if (audio > -1) {
                return {QString("%1_%2.png").arg(binClip->hash()).arg(audio)};
            }
            return {binClip->hash() + QStringLiteral(".png")};
        }
        QStringList streamsList;
        QStringList streamIndexes = streams.split(QLatin1Char(';'));
        for (const QString &st : qAsConst(streamIndexes)) {
            streamsList << QString("%1_%2.png").arg(binClip->hash(), st);
        }
        return streamsList;
    }
    return {};
}

// static
QDir ThumbnailCache::getDir(bool audio, bool *ok)
{
    return pCore->currentDoc()->getCacheDir(audio ? CacheAudio : CacheThumbs, ok);
}
