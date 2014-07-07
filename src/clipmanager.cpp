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
#include "doc/doccommands.h"
#include "kdenlivesettings.h"
#include "doc/docclipbase.h"
#include "doc/kdenlivedoc.h"
#include "timeline/abstractclipitem.h"
#include "timeline/abstractgroupitem.h"
#include "titledocument.h"
#include "project/subprojectitem.h"
#include "kthumb.h"

#include <mlt++/Mlt.h>

#include <KDebug>
#include <KMessageBox>
#include <KFileDialog>
#include <KApplication>
#include <kio/netaccess.h>
#include <kio/jobuidelegate.h>

#include <solid/device.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>

#include <QGraphicsItemGroup>
#include <QtConcurrentRun>

#include <KFileMetaInfo>


ClipManager::ClipManager(KdenliveDoc *doc) :
    QObject(),
    m_audioThumbsQueue(),
    m_doc(doc),
    m_abortThumb(false),
    m_closing(false),
    m_abortAudioThumb(false)
{
    m_clipIdCounter = 1;
    m_folderIdCounter = 1;
    m_modifiedTimer.setInterval(1500);
    connect(&m_fileWatcher, SIGNAL(dirty(QString)), this, SLOT(slotClipModified(QString)));
    connect(&m_fileWatcher, SIGNAL(deleted(QString)), this, SLOT(slotClipMissing(QString)));

    // Seems like a dirty signal is emitted anyways when a watched file is created, so don't react twice.
    //connect(&m_fileWatcher, SIGNAL(created(QString)), this, SLOT(slotClipAvailable(QString)));
    connect(&m_modifiedTimer, SIGNAL(timeout()), this, SLOT(slotProcessModifiedClips()));

#if KDE_IS_VERSION(4,5,0)
    KImageCache::deleteCache("kdenlive-thumbs");
    pixmapCache = new KImageCache("kdenlive-thumbs", 1000000);
    pixmapCache->setEvictionPolicy(KSharedDataCache::EvictOldest);
#endif
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

    qDeleteAll(m_clipList);
    m_clipList.clear();
#if KDE_IS_VERSION(4,5,0)
    delete pixmapCache;
#endif
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
    qDeleteAll(m_clipList);
    m_clipList.clear();
    m_clipIdCounter = 1;
    m_folderIdCounter = 1;
#if KDE_IS_VERSION(4,5,0)
    pixmapCache->clear();
#endif
}

void ClipManager::clearCache()
{
#if KDE_IS_VERSION(4,5,0)
    pixmapCache->clear();
#endif
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
        DocClipBase *clip = getClipById(m_processingThumbId);
        if (!clip) continue;
        max = m_requestedThumbs.size() + values.count();
        while (!values.isEmpty() && clip->thumbProducer() && !m_abortThumb) {
            int pos = values.takeFirst();
            switch (thumbType) {
            case 1:
                clip->thumbProducer()->getGenericThumb(pos, SubProjectItem::itemDefaultHeight(), thumbType);
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
    for (int i = 0; i < m_clipList.count(); ++i) {
        DocClipBase *clip = m_clipList.at(i);
        if (clip->hasAudioThumb() && !clip->audioThumbCreated())
            m_audioThumbsQueue.append(m_clipList.at(i)->getId());
    }
    m_thumbsMutex.unlock();
    if (!m_audioThumbsThread.isRunning() && !m_audioThumbsQueue.isEmpty()) {
        m_audioThumbsThread = QtConcurrent::run(this, &ClipManager::slotGetAudioThumbs);
    }
}

void ClipManager::askForAudioThumb(const QString &id)
{
    DocClipBase *clip = getClipById(id);
    if (clip && KdenliveSettings::audiothumbnails() && (clip->hasAudioThumb())) {
        m_thumbsMutex.lock();
        if (!m_audioThumbsQueue.contains(id)) m_audioThumbsQueue.append(id);
        m_thumbsMutex.unlock();
        if (!m_audioThumbsThread.isRunning()) m_audioThumbsThread = QtConcurrent::run(this, &ClipManager::slotGetAudioThumbs);
    }
}

void ClipManager::slotGetAudioThumbs()
{
    Mlt::Profile prof((char*) KdenliveSettings::current_profile().toUtf8().constData());
    mlt_audio_format audioFormat = mlt_audio_s16;
    while (!m_abortAudioThumb && !m_audioThumbsQueue.isEmpty()) {
        m_thumbsMutex.lock();
        m_processingAudioThumbId = m_audioThumbsQueue.takeFirst();
        m_thumbsMutex.unlock();
        DocClipBase *clip = getClipById(m_processingAudioThumbId);
        if (!clip || clip->audioThumbCreated()) continue;
        KUrl url = clip->fileURL();
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
                kDebug() << "--- BROKEN THUMB FOR: " << url.fileName() << " ---------------------- ";
                f.remove();
                continue;
            }
            kDebug() << "reading audio thumbs from file";

            int h1 = arrayWidth * channels;
            int h2 = (int) frame * h1;
            for (int z = (int) frame; z < (int)(frame + lengthInFrames) && !m_abortAudioThumb; z++) {
                int h3 = 0;
                for (int c = 0; c < channels; c++) {
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
            kDebug() << "++++++++  ERROR WRITING TO FILE: " << audioPath;
            kDebug() << "++++++++  DISABLING AUDIO THUMBS";
            m_thumbsMutex.lock();
            m_audioThumbsQueue.clear();
            m_thumbsMutex.unlock();
            KdenliveSettings::setAudiothumbnails(false);
            break;
        }

        Mlt::Producer producer(prof, url.path().toUtf8().constData());
        if (!producer.is_valid()) {
            kDebug() << "++++++++  INVALID CLIP: " << url.path();
            continue;
        }
        
        producer.set("video_index", "-1");

        if (KdenliveSettings::normaliseaudiothumbs()) {
            /*Mlt::Filter m_convert(prof, "volume");
            m_convert.set("gain", "normalise");
            producer.attach(m_convert);*/
        }

        int last_val = 0;
        double framesPerSecond = mlt_producer_get_fps(producer.get_producer());

        for (int z = (int) frame; z < (int)(frame + lengthInFrames) && producer.is_valid() &&  !m_abortAudioThumb; z++) {
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
                for (int c = 0; c < channels; c++) {
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
}

void ClipManager::setThumbsProgress(const QString &message, int progress)
{
    m_doc->setThumbsProgress(message, progress);
}

QList <DocClipBase*> ClipManager::documentClipList() const
{
    return m_clipList;
}

QMap <QString, QString> ClipManager::documentFolderList() const
{
    return m_folderList;
}

void ClipManager::addClip(DocClipBase *clip)
{
    m_clipList.append(clip);
    if (clip->clipType() != Color && clip->clipType() != SlideShow  && !clip->fileURL().isEmpty()) {
        // listen for file change
        //kDebug() << "// LISTEN FOR: " << clip->fileURL().path();
        m_fileWatcher.addFile(clip->fileURL().path());
    }
    const QString id = clip->getId();
    if (id.toInt() >= m_clipIdCounter) m_clipIdCounter = id.toInt() + 1;
    const QString gid = clip->getProperty("groupid");
    if (!gid.isEmpty() && gid.toInt() >= m_folderIdCounter) m_folderIdCounter = gid.toInt() + 1;
}

void ClipManager::slotDeleteClips(QStringList ids)
{
    QUndoCommand *delClips = new QUndoCommand();
    delClips->setText(i18np("Delete clip", "Delete clips", ids.size()));

    for (int i = 0; i < ids.size(); ++i) {
        DocClipBase *clip = getClipById(ids.at(i));
        if (clip) {
            new AddClipCommand(m_doc, clip->toXML(), ids.at(i), false, delClips);
        }
    }
    m_doc->commandStack()->push(delClips);
}

void ClipManager::deleteClip(const QString &clipId)
{
    for (int i = 0; i < m_clipList.count(); ++i) {
        if (m_clipList.at(i)->getId() == clipId) {
            DocClipBase *clip = m_clipList.takeAt(i);
            if (clip->clipType() != Color && clip->clipType() != SlideShow  && !clip->fileURL().isEmpty()) {
                //if (m_clipList.at(i)->clipType() == IMAGE || m_clipList.at(i)->clipType() == AUDIO || (m_clipList.at(i)->clipType() == TEXT && !m_clipList.at(i)->fileURL().isEmpty())) {
                // listen for file change
                m_fileWatcher.removeFile(clip->fileURL().path());
            }
            delete clip;
            clip = NULL;
            break;
        }
    }
}

DocClipBase *ClipManager::getClipAt(int pos)
{
    return m_clipList.at(pos);
}

DocClipBase *ClipManager::getClipById(QString clipId)
{
    //kDebug() << "++++  CLIP MAN, LOOKING FOR CLIP ID: " << clipId;
    clipId = clipId.section('_', 0, 0);
    for (int i = 0; i < m_clipList.count(); ++i) {
        if (m_clipList.at(i)->getId() == clipId) {
            //kDebug() << "++++  CLIP MAN, FOUND FOR CLIP ID: " << clipId;
            return m_clipList.at(i);
        }
    }
    return NULL;
}

const QList <DocClipBase *> ClipManager::getClipByResource(const QString &resource)
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
}


void ClipManager::clearUnusedProducers()
{
    for (int i = 0; i < m_clipList.count(); ++i) {
        if (m_clipList.at(i)->numReferences() == 0) m_clipList.at(i)->deleteProducers();
    }
}

void ClipManager::resetProducersList(const QList <Mlt::Producer *> prods, bool displayRatioChanged, bool fpsChanged)
{
    for (int i = 0; i < m_clipList.count(); ++i) {
        if (m_clipList.at(i)->numReferences() > 0 || displayRatioChanged || fpsChanged) {
            m_clipList.at(i)->deleteProducers();
        }
    }
    QString id;
    Mlt::Producer *prod;
    QStringList brokenClips;
    for (int i = 0; i < prods.count(); ++i) {
        prod = prods.at(i);
        id = prod->get("id");
        if (id.contains('_')) id = id.section('_', 0, 0);
        DocClipBase *clip = getClipById(id);
        QString markup = prod->get("markup");
        if (prod->is_blank() || !prod->is_valid() || !markup.isEmpty()) {
            // The clip is broken (missing proxy or source clip)
            kDebug()<<"// WARNING, CLIP "<<id<<" Cannot be loaded";
            brokenClips << id;
        }
        else if (clip) {
            clip->setProducer(prod, false, true);
        }
    }
    emit checkAllClips(displayRatioChanged, fpsChanged, brokenClips);
}

void ClipManager::slotAddClip(KIO::Job *job, const KUrl &, const KUrl &dst)
{
    KIO::MetaData meta = job->metaData();
    QMap <QString, QString> data;
    data.insert("group", meta.value("group"));
    data.insert("groupid", meta.value("groupid"));
    data.insert("comment", meta.value("comment"));
    kDebug()<<"Finished copying: "<<dst<<" / "<<meta.value("group")<<" / "<<meta.value("groupid");
    slotAddClipList(KUrl::List () << dst, data);
}

void ClipManager::slotAddClipList(const KUrl::List &urls, const QMap <QString, QString> &data)
{
    QUndoCommand *addClips = new QUndoCommand();
    // Update list of removable volumes
    //TODO: update only when new volume is plugged / unplugged
    listRemovableVolumes();
    foreach(const KUrl & file, urls) {
        if (QFile::exists(file.path())) {//KIO::NetAccess::exists(file, KIO::NetAccess::SourceSide, NULL)) {
            if (!data.contains("bypassDuplicate") && !getClipByResource(file.path()).empty()) {
                if (KMessageBox::warningContinueCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br />already exists in project, what do you want to do?", file.path()), i18n("Clip already exists")) == KMessageBox::Cancel)
                    continue;
            }
            if (isOnRemovableDevice(file)) {
                int answer = KMessageBox::warningYesNoCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br /> is on a removable device, will not be available when device is unplugged", file.path()), i18n("File on a Removable Device"), KGuiItem(i18n("Copy file to project folder")), KGuiItem(i18n("Continue")), KStandardGuiItem::cancel(), QString("copyFilesToProjectFolder"));
                if (answer == KMessageBox::Cancel) continue;
                else if (answer == KMessageBox::Yes) {
                    // Copy files to project folder
                    QString sourcesFolder = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "clips/";
                    KIO::NetAccess::mkdir(sourcesFolder, kapp->activeWindow());
                    //KIO::filesize_t m_requestedSize;
                    KIO::CopyJob *copyjob = KIO::copy (file, KUrl(sourcesFolder));
                    //TODO: for some reason, passing metadata does not work...
                    copyjob->addMetaData("group", data.value("group"));
                    copyjob->addMetaData("groupId", data.value("groupId"));
                    copyjob->addMetaData("comment", data.value("comment"));
                    copyjob->ui()->setWindow(kapp->activeWindow());
                    connect(copyjob, SIGNAL(copyingDone(KIO::Job*,KUrl,KUrl,time_t,bool,bool)), this, SLOT(slotAddClip(KIO::Job*,KUrl,KUrl)));
                    continue;
                }
            }
            kDebug() << "Adding clip: " << file.path();
            QDomDocument doc;
            QDomElement prod = doc.createElement("producer");
            doc.appendChild(prod);
            prod.setAttribute("resource", file.path());
            uint id = m_clipIdCounter++;
            prod.setAttribute("id", QString::number(id));
            if (data.contains("comment")) prod.setAttribute("description", data.value("comment"));
            if (data.contains("group")) {
                prod.setAttribute("groupname", data.value("group"));
                prod.setAttribute("groupid", data.value("groupId"));
            }
            if (data.contains("video_index")) prod.setAttribute("video_index", data.value("video_index"));
            if (data.contains("audio_index")) prod.setAttribute("audio_index", data.value("audio_index"));

            KMimeType::Ptr type = KMimeType::findByUrl(file);
            if (type->name().startsWith(QLatin1String("image/"))) {
                prod.setAttribute("type", (int) Image);
                prod.setAttribute("in", 0);
                prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::image_duration()) - 1);
                if (KdenliveSettings::autoimagetransparency()) prod.setAttribute("transparency", 1);
                // Read EXIF metadata for JPEG
                if (type->is("image/jpeg")) {
                    KFileMetaInfo metaInfo(file.path(), QString("image/jpeg"), KFileMetaInfo::TechnicalInfo);
                    const QHash<QString, KFileMetaInfoItem> metaInfoItems = metaInfo.items();
                    foreach(const KFileMetaInfoItem & metaInfoItem, metaInfoItems) {
                        QDomElement meta = doc.createElement("metaproperty");
                        meta.setAttribute("name", "meta.attr." + metaInfoItem.name().section('#', 1));
                        QDomText value = doc.createTextNode(metaInfoItem.value().toString());
                        meta.setAttribute("tool", "KDE Metadata");
                        meta.appendChild(value);
                        prod.appendChild(meta);
                    }
                }
            } else if (type->is("application/x-kdenlivetitle")) {
                // opening a title file
                QDomDocument txtdoc("titledocument");
                QFile txtfile(file.path());
                if (txtfile.open(QIODevice::ReadOnly) && txtdoc.setContent(&txtfile)) {
                    txtfile.close();
                    prod.setAttribute("type", (int) Text);
                    // extract embeded images
                    QDomNodeList items = txtdoc.elementsByTagName("content");
                    for (int i = 0; i < items.count() ; ++i) {
                        QDomElement content = items.item(i).toElement();
                        if (content.hasAttribute("base64")) {
                            QString titlesFolder = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
                            QString path = TitleDocument::extractBase64Image(titlesFolder, content.attribute("base64"));
                            if (!path.isEmpty()) {
                                content.setAttribute("url", path);
                                content.removeAttribute("base64");
                            }
                        }
                    }
                    prod.setAttribute("transparency", 1);
                    prod.setAttribute("in", 0);
                    if (!txtdoc.documentElement().hasAttribute("out")) {
                        prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::title_duration()) - 1);
                        txtdoc.documentElement().setAttribute("out", m_doc->getFramePos(KdenliveSettings::title_duration()) - 1);
                    }
                    else {
                        int out = txtdoc.documentElement().attribute("out").toInt();
                        if (out >= 0)
                            prod.setAttribute("out", out);
                        else {
                            prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::title_duration()) - 1);
                            txtdoc.documentElement().setAttribute("out", m_doc->getFramePos(KdenliveSettings::title_duration()) - 1);
                        }
                    }
                    QString titleData = txtdoc.toString();
                    prod.setAttribute("xmldata", titleData);
                } else
                    txtfile.close();
            }
            new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true, addClips);
        }
        else kDebug()<<"// CANNOT READ FILE: "<<file;
    }
    if (addClips->childCount() > 0) {
        addClips->setText(i18np("Add clip", "Add clips", addClips->childCount()));
        m_doc->commandStack()->push(addClips);
    }
}

void ClipManager::slotAddClipFile(const KUrl &url, const QMap <QString, QString> &data)
{
    slotAddClipList(KUrl::List(url), data);
}

void ClipManager::slotAddXmlClipFile(const QString &name, const QDomElement &xml, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    doc.appendChild(doc.importNode(xml, true));
    QDomElement prod = doc.documentElement();
    prod.setAttribute("type", (int) Playlist);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("name", name);
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddColorClipFile(const QString &name, const QString &color, const QString &duration, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    prod.setAttribute("mlt_service", "colour");
    prod.setAttribute("colour", color);
    prod.setAttribute("type", (int) Color);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("in", "0");
    prod.setAttribute("out", m_doc->getFramePos(duration) - 1);
    prod.setAttribute("name", name);
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddSlideshowClipFile(QMap <QString, QString> properties, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    QMap<QString, QString>::const_iterator i = properties.constBegin();
    while (i != properties.constEnd()) {
        prod.setAttribute(i.key(), i.value());
        ++i;
    }
    prod.setAttribute("type", (int) SlideShow);
    uint id = m_clipIdCounter++;
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}



void ClipManager::slotAddTextClipFile(const QString &titleName, int duration, const QString &xml, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    //prod.setAttribute("resource", imagePath);
    prod.setAttribute("name", titleName);
    prod.setAttribute("xmldata", xml);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    prod.setAttribute("type", (int) Text);
    prod.setAttribute("transparency", "1");
    prod.setAttribute("in", "0");
    prod.setAttribute("out", duration - 1);
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddTextTemplateClip(QString titleName, const KUrl &path, const QString &group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    prod.setAttribute("name", titleName);
    prod.setAttribute("resource", path.path());
    uint id = m_clipIdCounter++;
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
        txtfile.close();
        if (titledoc.documentElement().hasAttribute("duration")) {
            duration = titledoc.documentElement().attribute("duration").toInt();
        }
        else {
            // keep some time for backwards compatibility - 26/12/12
            duration = titledoc.documentElement().attribute("out").toInt();
        }
    } else txtfile.close();

    if (duration == 0) duration = m_doc->getFramePos(KdenliveSettings::title_duration());
    prod.setAttribute("out", duration - 1);

    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

int ClipManager::getFreeClipId()
{
    return m_clipIdCounter++;
}

int ClipManager::getFreeFolderId()
{
    return m_folderIdCounter++;
}

int ClipManager::lastClipId() const
{
    return m_clipIdCounter - 1;
}

QString ClipManager::projectFolder() const
{
    return m_doc->projectFolder().path();
}

void ClipManager::addFolder(const QString &id, const QString &name)
{
    m_folderList.insert(id, name);
}

void ClipManager::deleteFolder(const QString &id)
{
    m_folderList.remove(id);
}

AbstractGroupItem *ClipManager::createGroup()
{
    AbstractGroupItem *group = new AbstractGroupItem(m_doc->fps());
    m_groupsList.append(group);
    return group;
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
        for (int j = 0; j < children.count(); j++) {
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
    //kDebug() << "// CLIP: " << path << " WAS MODIFIED";
    const QList <DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != NULL) {
            QString id = clip->getId();
            if (!m_modifiedClips.contains(id))
                emit modifiedClip(id);
            m_modifiedClips[id] = QTime::currentTime();
        }
    }
    if (!m_modifiedTimer.isActive()) m_modifiedTimer.start();
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
    // kDebug() << "// CLIP: " << path << " WAS MISSING";
    const QList <DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != NULL) emit missingClip(clip->getId());
    }
}

void ClipManager::slotClipAvailable(const QString &path)
{
    // kDebug() << "// CLIP: " << path << " WAS ADDED";
    const QList <DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != NULL) emit availableClip(clip->getId());
    }
}

int ClipManager::clipsCount() const
{
    return m_clipList.count();
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

bool ClipManager::isOnRemovableDevice(const KUrl &url)
{
    //SolidVolumeInfo volume;
    QString path = url.path(KUrl::RemoveTrailingSlash);
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


#include "clipmanager.moc"
