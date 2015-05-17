/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef CLIPMANAGER_H
#define CLIPMANAGER_H

#include <QtXml/qdom.h>
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QFuture>

#include <QUrl>
#include <KDirWatch>
#include <KIO/CopyJob>
#include <kimagecache.h>


#include "gentime.h"
#include "definitions.h"


class KdenliveDoc;
class AbstractGroupItem;
class QUndoCommand;

class SolidVolumeInfo
{

public:
    QString path; // mount path of volume, with trailing slash
    QString uuid; // UUID as from Solid
    QString label; // volume label (think of CDs)
    bool isRemovable; // may be removed
    bool isMounted;

    bool isNull() const { return path.isNull(); }
};

namespace Mlt
{
class Producer;
}

/**
 * @class ClipManager
 * @brief Takes care of clip operations that might affect timeline and bin
 */

class ClipManager: public QObject
{
Q_OBJECT public:

    explicit ClipManager(KdenliveDoc *doc);
    virtual ~ ClipManager();
    void deleteClip(const QString &clipId);
    void deleteProjectClip(const QString &clipId);

    void slotAddTextClipFile(const QString &titleName, int out, const QString &xml, const QString &group, const QString &groupId);
    void slotAddTextTemplateClip(QString titleName, const QUrl &path, const QString &group, const QString &groupId);
    void slotAddXmlClipFile(const QString &name, const QDomElement &xml, const QString &group, const QString &groupId);
    //const QList <DocClipBase *> getClipByResource(const QString &resource);
    void slotDeleteClips(QStringList clipIds, QStringList folderIds, QUndoCommand *deleteCommand);
    void setThumbsProgress(const QString &message, int progress);
    QMap <QString, QString> documentFolderList() const;
    int getFreeFolderId();
    int lastClipId() const;
    QString projectFolder() const;
    void clearUnusedProducers();
    /** @brief Prepare deletion of clips and folders from the Bin. */
    void deleteProjectItems(QStringList clipIds, QStringList folderIds);
    void clear();
    void clearCache();
    AbstractGroupItem *createGroup();
    void removeGroup(AbstractGroupItem *group);
    QDomElement groupsXml() const;
    /** @brief remove a clip id from the queue list. */
    void stopThumbs(const QString &id);
    void projectTreeThumbReady(const QString &id, int frame, const QImage &img, int type);
    KImageCache* pixmapCache;

public slots:
    /** @brief Request creation of a clip thumbnail for specified frames. */
    void slotRequestThumbs(const QString &id, const QList<int> &frames);
    
private slots:
    /** A clip was externally modified, monitor for more changes and prepare for reload */
    void slotClipModified(const QString &path);
    void slotClipMissing(const QString &path);
    void slotClipAvailable(const QString &path);
    /** Check the list of externally modified clips, and process them if they were not modified in the last 1500 milliseconds */
    void slotProcessModifiedClips();
    void slotGetThumbs();
    /** @brief Clip has been copied, add it now. */
    void slotAddCopiedClip(KIO::Job *job, const QUrl &, const QUrl &dst);

private:   // Private attributes
    /** the list of clips in the document */
    /** the list of groups in the document */
    QList <AbstractGroupItem *> m_groupsList;
    QMap <QString, QString> m_folderList;
    QList <QString> m_audioThumbsQueue;
    /** the document undo stack*/
    KdenliveDoc *m_doc;
    int m_folderIdCounter;
    KDirWatch m_fileWatcher;
    /** Timer used to reload clips when they have been externally modified */
    QTimer m_modifiedTimer;
    /** List of the clip IDs that need to be reloaded after being externally modified */
    QMap <QString, QTime> m_modifiedClips;
    /** Struct containing the list of clip thumbnails to request (clip id and frames) */
    QMap <QString, int> m_requestedThumbs;
    QMutex m_thumbsMutex;
    QFuture<void> m_thumbsThread;
    /** @brief The id of currently processed clip for thumbs creation. */
    QString m_processingThumbId;
    /** @brief If true, abort processing of clip thumbs before removing a clip. */
    bool m_abortThumb;
    /** @brief We are about to delete the clip producer, stop processing thumbs. */
    bool m_closing;
    QFuture<void> m_audioThumbsThread;
    /** @brief If true, abort processing of audio thumbs. */
    bool m_abortAudioThumb;
    /** @brief The id of currently processed clip for audio thumbs creation. */
    QString m_processingAudioThumbId;
    /** @brief The list of removable drives. */
    QList<SolidVolumeInfo> m_removableVolumes;

    QPoint m_projectTreeThumbSize;

    /** @brief Get a list of drives, to check if we have files on removable media. */
    void listRemovableVolumes();
    /** @brief Check if added file is on a removable drive. */
    bool isOnRemovableDevice(const QUrl &url);

signals:
    void reloadClip(const QString &);
    void modifiedClip(const QString &);
    void missingClip(const QString &);
    void availableClip(const QString &);
    void checkAllClips(bool displayRatioChanged, bool fpsChanged, const QStringList &brokenClips);
    void displayMessage(const QString &, int);
    void thumbReady(const QString &id, int, const QImage&);
    void gotClipPropertyThumbnail(const QString &id, const QImage&);
};

#endif
