/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "filewatcher.hpp"

#include <KDirWatch>
#include <QFileInfo>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
{
    // Init clip modification tracker
    m_modifiedTimer.setInterval(1500);
    connect(KDirWatch::self(), &KDirWatch::dirty, this, &FileWatcher::slotUrlModified);
    connect(KDirWatch::self(), &KDirWatch::deleted, this, &FileWatcher::slotUrlMissing);
    connect(KDirWatch::self(), &KDirWatch::created, this, &FileWatcher::slotUrlAdded);
    connect(&m_modifiedTimer, &QTimer::timeout, this, &FileWatcher::slotProcessModifiedUrls);
}

void FileWatcher::addFile(const QString &binId, const QString &url)
{
    if (url.isEmpty()) {
        return;
    }
    if (m_occurences.count(url) == 0) {
        KDirWatch::self()->addFile(url);
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
        KDirWatch::self()->removeFile(url);
        m_occurences.erase(url);
    }
}

void FileWatcher::slotUrlModified(const QString &path)
{
    if (m_modifiedUrls.insert(path).second) {
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
        if (KDirWatch::self()->ctime(path).msecsTo(QDateTime::currentDateTime()) > 2000) {
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
    KDirWatch::self()->stopScan();
    for (const auto &f : m_occurences) {
        KDirWatch::self()->removeFile(f.first);
    }
    m_occurences.clear();
    m_modifiedUrls.clear();
    m_binClipPaths.clear();
    KDirWatch::self()->startScan();
}

bool FileWatcher::contains(const QString &path) const
{
    return KDirWatch::self()->contains(path);
}
