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
#include "bin/bin.h"
#include "core.h"

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
{
    m_fileWatcher = KDirWatch::self();
    // Init clip modification tracker
    m_modifiedTimer.setInterval(1500);
    connect(m_fileWatcher, &KDirWatch::dirty, this, &FileWatcher::slotUrlModified);
    connect(m_fileWatcher, &KDirWatch::deleted, this, &FileWatcher::slotUrlMissing);
    connect(&m_modifiedTimer, &QTimer::timeout, this, &FileWatcher::slotProcessModifiedUrls);
}

void FileWatcher::addFile(const QString &binId, const QString &url)
{
    if (m_occurences.contains(url)) {
        QStringList currentIds = m_occurences.value(url);
        if (!currentIds.contains(binId)) {
            currentIds << binId;
            m_occurences[url] = currentIds;
        }
    } else {
        // Unknown file, add to list
        m_occurences.insert(url, QStringList() << binId);
        m_fileWatcher->addFile(url);
    }
}

void FileWatcher::removeFile(const QString &binId, const QString &url)
{
    if (!m_occurences.contains(url)) {
        return;
    }
    QStringList currentIds = m_occurences.value(url);
    currentIds.removeAll(binId);
    if (currentIds.isEmpty()) {
        m_fileWatcher->removeFile(url);
        m_occurences.remove(url);
    } else {
        m_occurences[url] = currentIds;
    }
}

void FileWatcher::slotUrlModified(const QString &path)
{
    if (!m_modifiedUrls.contains(path)) {
        m_modifiedUrls << path;
        const QStringList ids = m_occurences.value(path);
        for (const QString &id : ids) {
            pCore->bin()->setWaitingStatus(id);
        }
    }
    if (!m_modifiedTimer.isActive()) {
        m_modifiedTimer.start();
    }
}

void FileWatcher::slotUrlMissing(const QString &path)
{
    // TODO handle missing clips by replacing producer with an invalid producer
    const QStringList ids = m_occurences.value(path);
    /*for (const QString &id :  ids) {
        emit missingClip(id);
    }*/
}

void FileWatcher::slotProcessModifiedUrls()
{
    QStringList checkList = m_modifiedUrls;
    for (const QString &path : checkList) {
        if (m_fileWatcher->ctime(path).msecsTo(QDateTime::currentDateTime()) > 1000) {
            const QStringList ids = m_occurences.value(path);
            for (const QString &id : ids) {
                pCore->bin()->reloadClip(id);
            }
            m_modifiedUrls.removeAll(path);
        }
    }
    if (m_modifiedUrls.isEmpty()) {
        m_modifiedTimer.stop();
    }
}

void FileWatcher::clear()
{
    m_fileWatcher->stopScan();
    const QList<QString> files = m_occurences.keys();
    for (const QString &url : files) {
        m_fileWatcher->removeFile(url);
    }
    m_occurences.clear();
    m_fileWatcher->startScan();
}
