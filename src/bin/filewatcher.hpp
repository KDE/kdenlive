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

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <KDirWatch>
#include <QMap>
#include <QTimer>

/** @brief This class is responsible for watching all files used in the project
    and triggers a reload notification when a file changes.
 */

class FileWatcher : public QObject
{

public:
    // Constructor
    explicit FileWatcher(QObject *parent = nullptr);
    // Add a file to the list of watched items
    void addFile(const QString &binId, const QString &url);
    // Remove a file from the list of watched items
    void removeFile(const QString &binId, const QString &url);
    // Reset all watched files
    void clear();

private slots:
    void slotUrlModified(const QString &path);
    void slotUrlMissing(const QString &path);
    void slotProcessModifiedUrls();

private:
    KDirWatch *m_fileWatcher;
    // A list with urls as keys, and the corresponding clip ids as value
    QMap<QString, QStringList> m_occurences;
    QStringList m_modifiedUrls;
    QTimer m_modifiedTimer;
};

#endif
