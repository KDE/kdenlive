/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include "definitions.h"
#include <QTimer>
#include <unordered_map>
#include <unordered_set>

/** @class FileWatcher
    @brief This class is responsible for watching all files used in the project
    and triggers a reload notification when a file changes.
 */
class FileWatcher : public QObject
{
    Q_OBJECT

public:
    // Constructor
    explicit FileWatcher(QObject *parent = nullptr);
    /** @brief Add a file to the queue for watched items */
    void addFile(const QString &binId, const QString &url);
    /** @brief Remove a binId from the list of watched items */
    void removeFile(const QString &binId);
    /** @returns  True if this url is already watched */
    bool contains(const QString &path) const;
    /** @brief Reset all watched files */
    void clear();

signals:
    /** @brief This signal is triggered whenever the file corresponding to a bin clip has been modified and should be reloaded. Note that this signal is sent no
     * more than every 1500 ms. We also make sure that at least 1000ms has passed since the last modification of the file. */
    void binClipModified(const QString &binId);
    /** @brief Same signal than binClipModified, but triggers immediately. Can be useful to refresh UI without actually reloading the file (yet)*/
    void binClipWaiting(const QString &binId);
    void binClipMissing(const QString &binId);

private slots:
    void slotUrlModified(const QString &path);
    void slotUrlMissing(const QString &path);
    void slotUrlAdded(const QString &path);
    void slotProcessModifiedUrls();
    void slotProcessQueue();

private:
    /// A list with urls as keys, and the corresponding clip ids as value
    std::unordered_map<QString, std::unordered_set<QString>> m_occurences;
    // keys are binId, keys are stored paths
    std::unordered_map<QString, QString> m_binClipPaths;

    /// List of files for which we received an update since the last send
    std::unordered_set<QString> m_modifiedUrls;
    
    /// When loading a project or adding many clips, adding many files to the watcher causes a freeze, so queue them
    std::unordered_map<QString, QString> m_pendingUrls;

    QTimer m_modifiedTimer;
    QTimer m_queueTimer;
    /// Add a file to the list of watched items
    void doAddFile(const QString &binId, const QString &url);
};

#endif
