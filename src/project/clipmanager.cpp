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

    KImageCache::deleteCache("kdenlive-thumbs");
    pixmapCache = new KImageCache("kdenlive-thumbs", 10000000);
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
    
    if (!m_audioThumbsThread.isRunning() && !m_audioThumbsQueue.isEmpty()) {
        m_audioThumbsThread = QtConcurrent::run(this, &ClipManager::slotGetAudioThumbs);
    }
}

void ClipManager::slotGetThumbs()
{
    QMap<QString, int>::const_iterator i;
    int max;
    int done = 0;
    int thumbType = 0; // 0 = timeline thumb, 1 = project clip zone thumb, 2 = clip properties thumb
    
    while (!m_requestedThumbs.isEmpty() && !m_abortThumb) {
        m_thumbsMutex.lock();
        i = m_requestedThumbs.constBegin();
        m_processingThumbId = i.key();
        QList<int> values = m_requestedThumbs.values(m_processingThumbId);
        m_requestedThumbs.remove(m_processingThumbId);
        if (m_processingThumbId.startsWith(QLatin1String("?"))) {
            // if id starts with ?, it means the request comes from a clip property widget
            thumbType = 2;
            m_processingThumbId.remove(0, 1);
        }
        if (m_processingThumbId.startsWith(QLatin1String("#"))) {
            // if id starts with #, it means the request comes from project tree
            thumbType = 1;
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

void ClipManager::checkAudioThumbs()
{
    if (!KdenliveSettings::audiothumbnails()) {
        if (m_audioThumbsThread.isRunning()) {
            m_abortAudioThumb = true;
            m_thumbsMutex.lock();
            m_audioThumbsQueue.clear();
            m_thumbsMutex.unlock();
            m_audioThumbsThread.waitForFinished();
            m_abortAudioThumb = false;
        }
        return;
    }

    m_thumbsMutex.lock();
    //TODO
    /*
    for (int i = 0; i < m_clipList.count(); ++i) {
        DocClipBase *clip = m_clipList.at(i);
        if (clip->hasAudioThumb() && !clip->audioThumbCreated())
            m_audioThumbsQueue.append(m_clipList.at(i)->getId());
    }
    */
    m_thumbsMutex.unlock();
    if (!m_audioThumbsThread.isRunning() && !m_audioThumbsQueue.isEmpty()) {
        m_audioThumbsThread = QtConcurrent::run(this, &ClipManager::slotGetAudioThumbs);
    }
}

void ClipManager::askForAudioThumb(const QString &id)
{
    /*DocClipBase *clip = getClipById(id);
    if (clip && KdenliveSettings::audiothumbnails() && (clip->hasAudioThumb())) {
        m_thumbsMutex.lock();
        if (!m_audioThumbsQueue.contains(id)) m_audioThumbsQueue.append(id);
        m_thumbsMutex.unlock();
        if (!m_audioThumbsThread.isRunning()) m_audioThumbsThread = QtConcurrent::run(this, &ClipManager::slotGetAudioThumbs);
    }*/
}

void ClipManager::slotGetAudioThumbs()
{
    /*
    Mlt::Profile prof((char*) KdenliveSettings::current_profile().toUtf8().constData());
    mlt_audio_format audioFormat = mlt_audio_s16;
    while (!m_abortAudioThumb && !m_audioThumbsQueue.isEmpty()) {
        m_thumbsMutex.lock();
        m_processingAudioThumbId = m_audioThumbsQueue.takeFirst();
        m_thumbsMutex.unlock();
        DocClipBase *clip = getClipById(m_processingAudioThumbId);
        if (!clip || clip->audioThumbCreated()) continue;
        QUrl url = clip->fileURL();
        QString hash = clip->getClipHash();
        if (hash.isEmpty()) continue;
        QString audioPath = projectFolder() + "/thumbs/" + hash + ".thumb";
        double lengthInFrames = clip->duration().frames(m_doc->fps());
        int frequency = 0;
        int channels = 0;
        QString data = clip->getProperty("frequency");
        if (!data.isEmpty()) frequency = data.toInt();
        if (frequency <= 0) frequency = 48000;
        data = clip->getProperty("channels");
        if (!data.isEmpty()) channels = data.toInt();
        if (channels <= 0) channels = 2;
        int arrayWidth = 20;
        double frame = 0.0;
        int maxVolume = 0;
        audioByteArray storeIn;
        QFile f(audioPath);
        if (QFileInfo(audioPath).size() > 0 && f.open(QIODevice::ReadOnly)) {
            const QByteArray channelarray = f.readAll();
            f.close();
            if (channelarray.size() != arrayWidth*(frame + lengthInFrames) * channels) {
                //qDebug() << "--- BROKEN THUMB FOR: " << url.fileName() << " ---------------------- ";
                f.remove();
                continue;
            }
            //qDebug() << "reading audio thumbs from file";

            int h1 = arrayWidth * channels;
            int h2 = (int) frame * h1;
            for (int z = (int) frame; z < (int)(frame + lengthInFrames) && !m_abortAudioThumb; ++z) {
                int h3 = 0;
                for (int c = 0; c < channels; ++c) {
                    QByteArray audioArray(arrayWidth, '\x00');
                    for (int i = 0; i < arrayWidth; ++i) {
                        audioArray[i] = channelarray.at(h2 + h3 + i);
                        if (audioArray.at(i) > maxVolume) maxVolume = audioArray.at(i);
                    }
                    h3 += arrayWidth;
                    storeIn[z][c] = audioArray;
                }
                h2 += h1;
            }
            if (!m_abortAudioThumb) {
                clip->setProperty("audio_max", QString::number(maxVolume - 64));
                clip->updateAudioThumbnail(storeIn);
            }
            continue;
        }
        
        if (!f.open(QIODevice::WriteOnly)) {
            //qDebug() << "++++++++  ERROR WRITING TO FILE: " << audioPath;
            //qDebug() << "++++++++  DISABLING AUDIO THUMBS";
            m_thumbsMutex.lock();
            m_audioThumbsQueue.clear();
            m_thumbsMutex.unlock();
            KdenliveSettings::setAudiothumbnails(false);
            break;
        }

        Mlt::Producer producer(prof, url.path().toUtf8().constData());
        if (!producer.is_valid()) {
            //qDebug() << "++++++++  INVALID CLIP: " << url.path();
            continue;
        }
        
        producer.set("video_index", "-1");

        //if (KdenliveSettings::normaliseaudiothumbs()) {
            //Mlt::Filter m_convert(prof, "volume");
            //m_convert.set("gain", "normalise");
            //producer.attach(m_convert);
        //}

        int last_val = 0;
        double framesPerSecond = mlt_producer_get_fps(producer.get_producer());

        for (int z = (int) frame; z < (int)(frame + lengthInFrames) && producer.is_valid() &&  !m_abortAudioThumb; ++z) {
            int val = (int)((z - frame) / (frame + lengthInFrames) * 100.0);
            if (last_val != val && val > 1) {
                setThumbsProgress(i18n("Creating audio thumbnail for %1", url.fileName()), val);
                last_val = val;
            }
            producer.seek(z);
            Mlt::Frame *mlt_frame = producer.get_frame();
            if (mlt_frame && mlt_frame->is_valid()) {
                int samples = mlt_sample_calculator(framesPerSecond, frequency, mlt_frame->get_position());
                qint16* pcm = static_cast<qint16*>(mlt_frame->get_audio(audioFormat, frequency, channels, samples));
                for (int c = 0; c < channels; ++c) {
                    QByteArray audioArray;
                    audioArray.resize(arrayWidth);
                    for (int i = 0; i < audioArray.size(); ++i) {
                        double pcmval = *(pcm + c + i * samples / audioArray.size());
                        if (pcmval >= 0) {
                            pcmval = sqrt(pcmval) / 2.83 + 64;
                            audioArray[i] = pcmval;
                            if (pcmval > maxVolume) maxVolume = pcmval;
                        }
                        else {
                            pcmval = -sqrt(-pcmval) / 2.83 + 64;
                            audioArray[i] = pcmval;
                            if (-pcmval > maxVolume) maxVolume = -pcmval;
                        }
                    }
                    f.write(audioArray);
                    storeIn[z][c] = audioArray;
                }
            } else {
                f.write(QByteArray(arrayWidth, '\x00'));
            }
            delete mlt_frame;
        }
        f.close();
        setThumbsProgress(i18n("Creating audio thumbnail for %1", url.fileName()), -1);
        if (m_abortAudioThumb) {
            f.remove();
        } else {
            clip->updateAudioThumbnail(storeIn);
            clip->setProperty("audio_max", QString::number(maxVolume - 64));
        }
    }
    m_processingAudioThumbId.clear();
    */
}

void ClipManager::setThumbsProgress(const QString &message, int progress)
{
    m_doc->setThumbsProgress(message, progress);
}


QMap <QString, QString> ClipManager::documentFolderList() const
{
    return m_folderList;
}

void ClipManager::deleteProjectItems(QStringList clipIds, QStringList folderIds)
{
    // Create meta command
    QUndoCommand *deleteCommand = new QUndoCommand();
    deleteCommand->setText(i18n("Delete clips"));
    if (pCore->projectManager()->currentTimeline()) {
        // Remove clips from timeline
        if (!clipIds.isEmpty()) {
            for (int i = 0; i < clipIds.size(); ++i) {
                pCore->projectManager()->currentTimeline()->slotDeleteClip(clipIds.at(i), deleteCommand);
            }
        }
        // remove clips and folders from bin
        slotDeleteClips(clipIds, folderIds, deleteCommand);
        m_doc->setModified(true);
    }
}


void ClipManager::deleteProjectClip(const QString &clipId)
{
    pCore->bin()->deleteClip(clipId);
}

void ClipManager::slotDeleteClips(QStringList clipIds, QStringList folderIds, QUndoCommand *deleteCommand)
{
    for (int i = 0; i < clipIds.size(); ++i) {
        QString xml = pCore->binController()->xmlFromId(clipIds.at(i));
	QDomDocument doc;
	doc.setContent(xml);
        if (!xml.isNull()) {
            new AddClipCommand(m_doc, doc.documentElement(), clipIds.at(i), false, deleteCommand);
        }
    }
    
    for (int i = 0; i < folderIds.size(); ++i) {
        pCore->bin()->removeFolder(folderIds.at(i), deleteCommand);
    }

    m_doc->commandStack()->push(deleteCommand);
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


void ClipManager::clearUnusedProducers()
{
/*    for (int i = 0; i < m_clipList.count(); ++i) {
        if (m_clipList.at(i)->numReferences() == 0) m_clipList.at(i)->deleteProducers();
    }*/
}


void ClipManager::slotAddCopiedClip(KIO::Job *job, const QUrl &, const QUrl &dst)
{
    KIO::MetaData meta = job->metaData();
    QMap <QString, QString> data;
    data.insert("group", meta.value("group"));
    data.insert("groupid", meta.value("groupid"));
    data.insert("comment", meta.value("comment"));
    //qDebug()<<"Finished copying: "<<dst<<" / "<<meta.value("group")<<" / "<<meta.value("groupid");
    pCore->bin()->droppedUrls(QList<QUrl> () << dst, data);
}


void ClipManager::slotAddXmlClipFile(const QString &name, const QDomElement &xml, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    doc.appendChild(doc.importNode(xml, true));
    QDomElement prod = doc.documentElement();
    prod.setAttribute("type", (int) Playlist);
    uint id = pCore->bin()->getFreeClipId();
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("name", name);
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}


void ClipManager::slotAddTextClipFile(const QString &titleName, int duration, const QString &xml, const QString &group, const QString &groupId)
{

}

void ClipManager::slotAddTextTemplateClip(QString titleName, const QUrl &path, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    prod.setAttribute("name", titleName);
    prod.setAttribute("resource", path.path());
    uint id = pCore->bin()->getFreeClipId();
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    prod.setAttribute("type", (int) Text);
    prod.setAttribute("transparency", "1");
    prod.setAttribute("in", "0");

    int duration = 0;
    QDomDocument titledoc;
    QFile txtfile(path.path());
    if (txtfile.open(QIODevice::ReadOnly) && titledoc.setContent(&txtfile)) {
        if (titledoc.documentElement().hasAttribute("duration")) {
            duration = titledoc.documentElement().attribute("duration").toInt();
        } else {
            // keep some time for backwards compatibility - 26/12/12
            duration = titledoc.documentElement().attribute("out").toInt();
        }
    }
    txtfile.close();

    if (duration == 0) duration = m_doc->getFramePos(KdenliveSettings::title_duration());
    prod.setAttribute("duration", duration - 1);
    prod.setAttribute("out", duration - 1);

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

QDomElement ClipManager::groupsXml() const
{
    QDomDocument doc;
    QDomElement groups = doc.createElement("groups");
    doc.appendChild(groups);
    for (int i = 0; i < m_groupsList.count(); ++i) {
        QDomElement group = doc.createElement("group");
        groups.appendChild(group);
        QList <QGraphicsItem *> children = m_groupsList.at(i)->childItems();
        for (int j = 0; j < children.count(); ++j) {
            if (children.at(j)->type() == AVWidget || children.at(j)->type() == TransitionWidget) {
                AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(j));
                ItemInfo info = item->info();
                if (item->type() == AVWidget) {
                    QDomElement clip = doc.createElement("clipitem");
                    clip.setAttribute("track", info.track);
                    clip.setAttribute("position", info.startPos.frames(m_doc->fps()));
                    group.appendChild(clip);
                } else if (item->type() == TransitionWidget) {
                    QDomElement clip = doc.createElement("transitionitem");
                    clip.setAttribute("track", info.track);
                    clip.setAttribute("position", info.startPos.frames(m_doc->fps()));
                    group.appendChild(clip);
                }
            }
        }
    }
    return doc.documentElement();
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



