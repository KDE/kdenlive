/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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
        if (m_fileWatcher->ctime(path).msecsTo(QDateTime::currentDateTime()) > 2000) {
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

bool FileWatcher::contains(const QString &path) const
{
    return m_fileWatcher->contains(path);
}
