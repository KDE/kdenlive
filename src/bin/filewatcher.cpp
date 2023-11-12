/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "filewatcher.hpp"

#include <KDirWatch>
#include <QFileInfo>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new KDirWatch)
{
    // Init clip modification tracker
    m_modifiedTimer.setInterval(2000);
    connect(m_fileWatcher.get(), &KDirWatch::dirty, this, &FileWatcher::slotUrlModified);
    connect(m_fileWatcher.get(), &KDirWatch::deleted, this, &FileWatcher::slotUrlMissing);
    connect(m_fileWatcher.get(), &KDirWatch::created, this, &FileWatcher::slotUrlAdded);
    connect(&m_modifiedTimer, &QTimer::timeout, this, &FileWatcher::slotProcessModifiedUrls);
    m_queueTimer.setInterval(300);
    m_queueTimer.setSingleShot(true);
    connect(&m_queueTimer, &QTimer::timeout, this, &FileWatcher::slotProcessQueue);
}

void FileWatcher::slotProcessQueue()
{
    if (m_pendingUrls.size() == 0) {
        return;
    }
    auto iter = m_pendingUrls.begin();
    doAddFile(iter->first, iter->second);
    m_pendingUrls.erase(iter->first);
    if (m_pendingUrls.size() > 0 && !m_queueTimer.isActive()) {
        m_queueTimer.start();
    }
}

void FileWatcher::addFile(const QString &binId, const QString &url)
{
    std::unordered_map<QString, std::unordered_set<QString>>::const_iterator pos = m_occurences.find(url);
    if (pos != m_occurences.end()) {
        // Url already queued, only add ref to binId if necessary
        if (pos->second.find(binId) == pos->second.end()) {
            m_occurences[url].insert(binId);
            m_binClipPaths[binId] = url;
        }
        return;
    }
    m_pendingUrls[binId] = url;
    if (!m_queueTimer.isActive()) {
        m_queueTimer.start();
    }
}

void FileWatcher::doAddFile(const QString &binId, const QString &url)
{
    if (url.isEmpty()) {
        return;
    }
    if (m_occurences.count(url) == 0) {
        // QtConcurrent::run([=] { KDirWatch::self()->addFile(url); });
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
    if (m_modifiedUrls.insert(path).second) {
        for (const QString &id : m_occurences[path]) {
            Q_EMIT binClipWaiting(id);
        }
    }
    if (!m_modifiedTimer.isActive()) {
        m_modifiedTimer.start();
    }
}

void FileWatcher::slotUrlAdded(const QString &path)
{
    for (const QString &id : m_occurences[path]) {
        Q_EMIT binClipModified(id);
    }
}

void FileWatcher::slotUrlMissing(const QString &path)
{
    for (const QString &id : m_occurences[path]) {
        Q_EMIT binClipMissing(id);
    }
}

void FileWatcher::slotProcessModifiedUrls()
{
    auto checkList = m_modifiedUrls;
    for (const QString &path : checkList) {
        if (m_fileWatcher->ctime(path).msecsTo(QDateTime::currentDateTime()) > 2000) {
            for (const QString &id : m_occurences[path]) {
                Q_EMIT binClipModified(id);
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
    m_queueTimer.stop();
    m_fileWatcher->stopScan();
    for (const auto &f : m_occurences) {
        m_fileWatcher->removeFile(f.first);
    }
    m_pendingUrls.clear();
    m_occurences.clear();
    m_modifiedUrls.clear();
    m_binClipPaths.clear();
    m_fileWatcher->startScan();
}

bool FileWatcher::contains(const QString &path) const
{
    return m_fileWatcher->contains(path);
}
