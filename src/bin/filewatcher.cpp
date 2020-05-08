/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
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

#include "filewatcher.hpp"

#include <QFileInfo>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new KDirWatch())
{
    // Init clip modification tracker
    m_modifiedTimer.setInterval(1500);
    connect(m_fileWatcher.get(), &KDirWatch::dirty, this, &FileWatcher::slotUrlModified);
    connect(m_fileWatcher.get(), &KDirWatch::deleted, this, &FileWatcher::slotUrlMissing);
    connect(m_fileWatcher.get(), &KDirWatch::created, this, &FileWatcher::slotUrlAdded);
    connect(&m_modifiedTimer, &QTimer::timeout, this, &FileWatcher::slotProcessModifiedUrls);
}

void FileWatcher::addFile(const QString &binId, const QString &url)
{
    if (url.isEmpty()) {
        return;
    }
    if (m_occurences.count(url) == 0) {
        m_fileWatcher->addFile(url);
    }
    m_occurences[url].insert(binId);
    m_binClipPaths[binId] = url;
}

void FileWatcher::removeFile(const QString &binId)
{
    if (m_binClipPaths.count(binId) == 0) {
        return;
    }
    QString url = m_binClipPaths[binId];
    m_occurences[url].erase(binId);
    m_binClipPaths.erase(binId);
    if (m_occurences[url].empty()) {
        m_fileWatcher->removeFile(url);
        m_occurences.erase(url);
    }
}

void FileWatcher::slotUrlModified(const QString &path)
{
    if (m_modifiedUrls.count(path) == 0) {
        m_modifiedUrls.insert(path);
        for (const QString &id : m_occurences[path]) {
            emit binClipWaiting(id);
        }
    }
    if (!m_modifiedTimer.isActive()) {
        m_modifiedTimer.start();
    }
}

void FileWatcher::slotUrlAdded(const QString &path)
{
    for (const QString &id : m_occurences[path]) {
        emit binClipModified(id);
    }
}

void FileWatcher::slotUrlMissing(const QString &path)
{
    for (const QString &id : m_occurences[path]) {
        emit binClipMissing(id);
    }
}

void FileWatcher::slotProcessModifiedUrls()
{
    auto checkList = m_modifiedUrls;
    for (const QString &path : checkList) {
        if (m_fileWatcher->ctime(path).msecsTo(QDateTime::currentDateTime()) > 1000) {
            for (const QString &id : m_occurences[path]) {
                emit binClipModified(id);
            }
            m_modifiedUrls.erase(path);
        }
    }
    if (m_modifiedUrls.empty()) {
        m_modifiedTimer.stop();
    }
}

void FileWatcher::clear()
{
    m_fileWatcher->stopScan();
    for (const auto &f : m_occurences) {
        m_fileWatcher->removeFile(f.first);
    }
    m_occurences.clear();
    m_modifiedUrls.clear();
    m_binClipPaths.clear();
    m_fileWatcher->startScan();
}
