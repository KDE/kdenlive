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


#include "clipmanager.h"
#include "subprojectitem.h"
#include "mltcontroller/clipcontroller.h"
#include "kdenlivesettings.h"
#include "doc/kthumb.h"
#include "doc/doccommands.h"
#include "doc/kdenlivedoc.h"
#include "project/projectmanager.h"
#include "timeline/abstractclipitem.h"
#include "timeline/abstractgroupitem.h"
#include "titler/titledocument.h"
#include "mltcontroller/bincontroller.h"
#include "renderer.h"
#include "dialogs/slideshowclip.h"
#include "core.h"
#include "bin/bin.h"

#include <mlt++/Mlt.h>


#include <KMessageBox>
#include <KIO/JobUiDelegate>
#include <KIO/MkdirJob>
#include <solid/device.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <klocalizedstring.h>
#include <KJobWidgets/KJobWidgets>
#include <KRecentDirs>
#include <KFileItem>

#include <QGraphicsItemGroup>
#include <QtConcurrent>
#include <QApplication>
#include <QMimeType>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QFileDialog>

ClipManager::ClipManager(KdenliveDoc *doc) :
    QObject(),
    m_audioThumbsQueue(),
    m_doc(doc),
    m_abortThumb(false),
    m_closing(false),
    m_abortAudioThumb(false)
{
    m_folderIdCounter = 1;
    m_modifiedTimer.setInterval(1500);
    connect(&m_fileWatcher, &KDirWatch::dirty, this, &ClipManager::slotClipModified);
    connect(&m_fileWatcher, &KDirWatch::deleted, this, &ClipManager::slotClipMissing);
    connect(&m_modifiedTimer, &QTimer::timeout, this, &ClipManager::slotProcessModifiedClips);

    KImageCache::deleteCache(QStringLiteral("kdenlive-thumbs"));
    pixmapCache = new KImageCache(QStringLiteral("kdenlive-thumbs"), 10000000);
    pixmapCache->setEvictionPolicy(KSharedDataCache::EvictOldest);
}

ClipManager::~ClipManager()
{
    m_closing = true;
    m_abortThumb = true;
    m_abortAudioThumb = true;
    m_thumbsThread.waitForFinished();
    m_audioThumbsThread.waitForFinished();
    m_thumbsMutex.lock();
    m_requestedThumbs.clear();
    m_audioThumbsQueue.clear();
    m_thumbsMutex.unlock();

    delete pixmapCache;
}

void ClipManager::clear()
{
    m_abortThumb = true;
    m_abortAudioThumb = true;
    m_thumbsThread.waitForFinished();
    m_audioThumbsThread.waitForFinished();
    m_thumbsMutex.lock();
    m_requestedThumbs.clear();
    m_audioThumbsQueue.clear();
    m_thumbsMutex.unlock();
    m_abortThumb = false;
    m_abortAudioThumb = false;
    m_folderList.clear();
    m_modifiedClips.clear();
    m_folderIdCounter = 1;
    pixmapCache->clear();
}

void ClipManager::clearCache()
{
    pixmapCache->clear();
}

void ClipManager::slotRequestThumbs(const QString &id, const QList <int>& frames)
{
    m_thumbsMutex.lock();
    foreach (int frame, frames) {
        m_requestedThumbs.insertMulti(id, frame);
    }
    m_thumbsMutex.unlock();
    if (!m_thumbsThread.isRunning() && !m_abortThumb) {
        m_thumbsThread = QtConcurrent::run(this, &ClipManager::slotGetThumbs);
    }
}

void ClipManager::stopThumbs(const QString &id)
{
    if (m_closing || (m_requestedThumbs.isEmpty() && m_processingThumbId != id && m_audioThumbsQueue.isEmpty() && m_processingAudioThumbId != id)) return;
    // Abort video thumbs for this clip
    m_abortThumb = true;
    m_thumbsThread.waitForFinished();
    m_thumbsMutex.lock();
    m_requestedThumbs.remove(id);
    m_audioThumbsQueue.removeAll(id);
    m_thumbsMutex.unlock();
    m_abortThumb = false;

    // Abort audio thumbs for this clip
    if (m_processingAudioThumbId == id) {
        m_abortAudioThumb = true;
        m_audioThumbsThread.waitForFinished();
        m_abortAudioThumb = false;
    }
    
    if (!m_thumbsThread.isRunning() && !m_requestedThumbs.isEmpty()) {
        m_thumbsThread = QtConcurrent::run(this, &ClipManager::slotGetThumbs);
    }
}

void ClipManager::slotGetThumbs()
{
    QMap<QString, int>::const_iterator i;
    
    while (!m_requestedThumbs.isEmpty() && !m_abortThumb) {
        m_thumbsMutex.lock();
        i = m_requestedThumbs.constBegin();
        m_processingThumbId = i.key();
        QList<int> values = m_requestedThumbs.values(m_processingThumbId);
        m_requestedThumbs.remove(m_processingThumbId);
        //TODO int thumbType = 0; // 0 = timeline thumb, 1 = project clip zone thumb, 2 = clip properties thumb
        if (m_processingThumbId.startsWith(QLatin1String("?"))) {
            // if id starts with ?, it means the request comes from a clip property widget
            //TODO thumbType = 2;
            m_processingThumbId.remove(0, 1);
        }
        if (m_processingThumbId.startsWith(QLatin1String("#"))) {
            // if id starts with #, it means the request comes from project tree
            //TODO thumbType = 1;
            m_processingThumbId.remove(0, 1);
        }
        m_thumbsMutex.unlock();
        qSort(values);
        //TODO
        /*
        DocClipBase *clip = getClipById(m_processingThumbId);
        if (!clip) continue;
        max = m_requestedThumbs.size() + values.count();
        // keep in sync with declaration un projectitem.cpp and subprojectitem.cpp
        int minHeight = qMax(38, QFontMetrics(QApplication::font()).lineSpacing() * 2);
        while (!values.isEmpty() && clip->thumbProducer() && !m_abortThumb) {
            int pos = values.takeFirst();
            switch (thumbType) {
            case 1:
                clip->thumbProducer()->getGenericThumb(pos, minHeight, thumbType);
                break;
            case 2:
                clip->thumbProducer()->getGenericThumb(pos, 180, thumbType);
                break;
            default:
                clip->thumbProducer()->getThumb(pos);
            }
            done++;
            if (max > 3) emit displayMessage(i18n("Loading thumbnails"), 100 * done / max);
        }
        */
    }
    m_processingThumbId.clear();
    emit displayMessage(QString(), -1);
}


void ClipManager::setThumbsProgress(const QString &message, int progress)
{
    m_doc->setThumbsProgress(message, progress);
}


QMap <QString, QString> ClipManager::documentFolderList() const
{
    return m_folderList;
}

void ClipManager::deleteProjectItems(QStringList clipIds, QStringList folderIds, QStringList subClipIds, QUndoCommand *deleteCommand)
{
    // Create meta command
    bool execute = deleteCommand == NULL;
    if (execute) {
        deleteCommand = new QUndoCommand();
    }
    if (clipIds.isEmpty()) {
        // Deleting folder only
        if (!subClipIds.isEmpty()) {
            deleteCommand->setText(i18np("Delete subclip", "Delete subclips", subClipIds.count()));
        }
        else {
            deleteCommand->setText(i18np("Delete folder", "Delete folders", folderIds.count()));
        }
    }
    else deleteCommand->setText(i18np("Delete clip", "Delete clips", clipIds.count()));
    if (pCore->projectManager()->currentTimeline()) {
        // Remove clips from timeline
        if (!clipIds.isEmpty()) {
            for (int i = 0; i < clipIds.size(); ++i) {
                pCore->projectManager()->currentTimeline()->slotDeleteClip(clipIds.at(i), deleteCommand);
            }
        }
        // remove clips and folders from bin
        slotDeleteClips(clipIds, folderIds, subClipIds, deleteCommand, execute);
    }
}

void ClipManager::slotDeleteClips(QStringList clipIds, QStringList folderIds, QStringList subClipIds, QUndoCommand *deleteCommand, bool execute)
{
    for (int i = 0; i < clipIds.size(); ++i) {
        QString xml = pCore->binController()->xmlFromId(clipIds.at(i));
        if (!xml.isEmpty()) {
	    QDomDocument doc;
	    doc.setContent(xml);
            new AddClipCommand(m_doc, doc.documentElement(), clipIds.at(i), false, deleteCommand);	    
        }
    }
    for (int i = 0; i < folderIds.size(); ++i) {
        pCore->bin()->removeFolder(folderIds.at(i), deleteCommand);
    }
    for (int i = 0; i < subClipIds.size(); ++i) {
        pCore->bin()->removeSubClip(subClipIds.at(i), deleteCommand);
    }

    if (execute) {
        m_doc->commandStack()->push(deleteCommand);
    }
}

void ClipManager::deleteClip(const QString &clipId)
{
    ClipController *controller = pCore->binController()->getController(clipId);
    ClipType type = controller->clipType();
    QString url = controller->clipUrl().toLocalFile();
    if (type != Color && type != SlideShow  && !url.isEmpty()) {
        m_fileWatcher.removeFile(url);
    }
    // Delete clip in bin
    pCore->bin()->deleteClip(clipId);
    // Delete controller and Mlt::Producer
    pCore->binController()->removeBinClip(clipId);
}

/*const QList <DocClipBase *> ClipManager::getClipByResource(const QString &resource)
{
    QList <DocClipBase *> list;
    QString clipResource;
    QString proxyResource;
    for (int i = 0; i < m_clipList.count(); ++i) {
        clipResource = m_clipList.at(i)->getProperty("resource");
        proxyResource = m_clipList.at(i)->getProperty("proxy");
        if (clipResource.isEmpty()) clipResource = m_clipList.at(i)->getProperty("colour");
        if (clipResource == resource || proxyResource == resource) {
            list.append(m_clipList.at(i));
        }
    }
    return list;
}*/

void ClipManager::slotAddCopiedClip(KIO::Job*, const QUrl&, const QUrl &dst)
{
    pCore->bin()->droppedUrls(QList<QUrl>() << dst);
}


void ClipManager::slotAddTextTemplateClip(QString titleName, const QUrl &path, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement(QStringLiteral("producer"));
    doc.appendChild(prod);
    prod.setAttribute(QStringLiteral("name"), titleName);
    prod.setAttribute(QStringLiteral("resource"), path.path());
    uint id = pCore->bin()->getFreeClipId();
    prod.setAttribute(QStringLiteral("id"), QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute(QStringLiteral("groupname"), group);
        prod.setAttribute(QStringLiteral("groupid"), groupId);
    }
    prod.setAttribute(QStringLiteral("type"), (int) Text);
    prod.setAttribute(QStringLiteral("transparency"), QStringLiteral("1"));
    prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));

    int duration = 0;
    QDomDocument titledoc;
    QFile txtfile(path.path());
    if (txtfile.open(QIODevice::ReadOnly) && titledoc.setContent(&txtfile)) {
        if (titledoc.documentElement().hasAttribute(QStringLiteral("duration"))) {
            duration = titledoc.documentElement().attribute(QStringLiteral("duration")).toInt();
        } else {
            // keep some time for backwards compatibility - 26/12/12
            duration = titledoc.documentElement().attribute(QStringLiteral("out")).toInt();
        }
    }
    txtfile.close();

    if (duration == 0) duration = m_doc->getFramePos(KdenliveSettings::title_duration());
    prod.setAttribute(QStringLiteral("duration"), duration - 1);
    prod.setAttribute(QStringLiteral("out"), duration - 1);

    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

int ClipManager::getFreeFolderId()
{
    return m_folderIdCounter++;
}

QString ClipManager::projectFolder() const
{
    return m_doc->projectFolder().path();
}


AbstractGroupItem *ClipManager::createGroup()
{
    AbstractGroupItem *group = new AbstractGroupItem(m_doc->fps());
    m_groupsList.append(group);
    return group;
}

int ClipManager::lastClipId() const
{
    return pCore->bin()->lastClipId();
}

void ClipManager::removeGroup(AbstractGroupItem *group)
{
    m_groupsList.removeAll(group);
}

QString ClipManager::groupsXml() const
{
    QDomDocument doc;
    QDomElement groups = doc.createElement(QStringLiteral("groups"));
    doc.appendChild(groups);
    for (int i = 0; i < m_groupsList.count(); ++i) {
        QDomElement group = doc.createElement(QStringLiteral("group"));
        groups.appendChild(group);
        QList <QGraphicsItem *> children = m_groupsList.at(i)->childItems();
        for (int j = 0; j < children.count(); ++j) {
            if (children.at(j)->type() == AVWidget || children.at(j)->type() == TransitionWidget) {
                AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(j));
                ItemInfo info = item->info();
                if (item->type() == AVWidget) {
                    QDomElement clip = doc.createElement(QStringLiteral("clipitem"));
                    clip.setAttribute(QStringLiteral("track"), info.track);
                    clip.setAttribute(QStringLiteral("position"), info.startPos.frames(m_doc->fps()));
                    group.appendChild(clip);
                } else if (item->type() == TransitionWidget) {
                    QDomElement clip = doc.createElement(QStringLiteral("transitionitem"));
                    clip.setAttribute(QStringLiteral("track"), info.track);
                    clip.setAttribute(QStringLiteral("position"), info.startPos.frames(m_doc->fps()));
                    group.appendChild(clip);
                }
            }
        }
    }
    return doc.toString();
}


void ClipManager::slotClipModified(const QString &path)
{
    qDebug() << "// CLIP: " << path << " WAS MODIFIED";
    //TODO
    /*const QList <DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != NULL) {
            QString id = clip->getId();
            if (!m_modifiedClips.contains(id))
                emit modifiedClip(id);
            m_modifiedClips[id] = QTime::currentTime();
        }
    }
    if (!m_modifiedTimer.isActive()) m_modifiedTimer.start();*/
}

void ClipManager::slotProcessModifiedClips()
{
    if (!m_modifiedClips.isEmpty()) {
        QMapIterator<QString, QTime> i(m_modifiedClips);
        while (i.hasNext()) {
            i.next();
            if (QTime::currentTime().msecsTo(i.value()) <= -1500) {
                emit reloadClip(i.key());
                m_modifiedClips.remove(i.key());
                break;
            }
        }
    }
    if (m_modifiedClips.isEmpty()) m_modifiedTimer.stop();
}

void ClipManager::slotClipMissing(const QString &path)
{
    qDebug() << "// CLIP: " << path << " WAS MISSING";
    //TODO
    /*const QList <DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != NULL) emit missingClip(clip->getId());
    }*/
}

void ClipManager::slotClipAvailable(const QString &path)
{
    qDebug() << "// CLIP: " << path << " WAS ADDED";
    //TODO
    /*const QList <DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != NULL) emit availableClip(clip->getId());
    }*/
}


void ClipManager::listRemovableVolumes()
{
    QList<SolidVolumeInfo> volumes;
    m_removableVolumes.clear();

    QList<Solid::Device> devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);

    foreach(const Solid::Device &accessDevice, devices)
    {
        // check for StorageAccess
        if (!accessDevice.is<Solid::StorageAccess>())
            continue;

        const Solid::StorageAccess *access = accessDevice.as<Solid::StorageAccess>();

        if (!access->isAccessible())
            continue;

        // check for StorageDrive
        Solid::Device driveDevice;
        for (Solid::Device currentDevice = accessDevice; currentDevice.isValid(); currentDevice = currentDevice.parent())
        {
            if (currentDevice.is<Solid::StorageDrive>())
            {
                driveDevice = currentDevice;
                break;
            }
        }
        if (!driveDevice.isValid())
            continue;

        Solid::StorageDrive *drive = driveDevice.as<Solid::StorageDrive>();
        if (!drive->isRemovable()) continue;

        // check for StorageVolume
        Solid::Device volumeDevice;
        for (Solid::Device currentDevice = accessDevice; currentDevice.isValid(); currentDevice = currentDevice.parent())
        {
            if (currentDevice.is<Solid::StorageVolume>())
            {
                volumeDevice = currentDevice;
                break;
            }
        }
        if (!volumeDevice.isValid())
            continue;

        Solid::StorageVolume *volume = volumeDevice.as<Solid::StorageVolume>();

        SolidVolumeInfo info;
        info.path = access->filePath();
        info.isMounted = access->isAccessible();
        if (!info.path.isEmpty() && !info.path.endsWith('/'))
            info.path += '/';
        info.uuid = volume->uuid();
        info.label = volume->label();
        info.isRemovable = drive->isRemovable();
        m_removableVolumes << info;
    }
}

bool ClipManager::isOnRemovableDevice(const QUrl &url)
{
    //SolidVolumeInfo volume;
    QString path = url.adjusted(QUrl::StripTrailingSlash).path();
    int volumeMatch = 0;

    //FIXME: Network shares! Here we get only the volume of the mount path...
    // This is probably not really clean. But Solid does not help us.
    foreach (const SolidVolumeInfo &v, m_removableVolumes)
    {
        if (v.isMounted && !v.path.isEmpty() && path.startsWith(v.path))
        {
            int length = v.path.length();
            if (length > volumeMatch)
            {
                volumeMatch = v.path.length();
                //volume = v;
            }
        }
    }

    return volumeMatch;
}

void ClipManager::projectTreeThumbReady(const QString &id, int frame, const QImage &img, int type)
{
    switch (type) {
    case 2:
        emit gotClipPropertyThumbnail(id, img);
        break;
    default:
        emit thumbReady(id, frame, img);
    }
}



