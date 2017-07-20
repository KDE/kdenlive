/***************************************************************************
 *   Copyright (C) 2014 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "bincontroller.h"
#include "bin/model/markerlistmodel.hpp"
#include "clipcontroller.h"
#include "kdenlivesettings.h"
#include "timeline/clip.h"

static const char *kPlaylistTrackId = "main bin";

BinController::BinController(const QString &profileName)
    : QObject()
{
    // resetProfile(profileName.isEmpty() ? KdenliveSettings::current_profile() : profileName);
}

BinController::~BinController()
{
    qDebug() << "/// delete bincontroller";
    qDebug()<<"REMAINING CLIPS: "<<m_clipList.keys();
    destroyBin();
}

Mlt::Profile *BinController::profile()
{
    return m_binPlaylist->profile();
}

void BinController::destroyBin()
{
    if (m_binPlaylist) {
        m_binPlaylist->clear();
    }
    qDeleteAll(m_extraClipList);
    m_extraClipList.clear();

    m_clipList.clear();
}

void BinController::setDocumentRoot(const QString &root)
{
    if (root.isEmpty()) {
        m_documentRoot.clear();
    } else {
        m_documentRoot = root;
        if (!m_documentRoot.endsWith(QLatin1Char('/'))) {
            m_documentRoot.append(QLatin1Char('/'));
        }
    }
}

const QString BinController::documentRoot() const
{
    return m_documentRoot;
}

void BinController::loadExtraProducer(const QString &id, Mlt::Producer *prod)
{
    if (m_extraClipList.contains(id)) {
        return;
    }
    m_extraClipList.insert(id, prod);
}

QStringList BinController::getProjectHashes()
{
    QStringList hashes;
    QMapIterator<QString, std::shared_ptr<ClipController>> i(m_clipList);
    hashes.reserve(m_clipList.count());
    while (i.hasNext()) {
        i.next();
        hashes << i.value()->getClipHash();
    }
    hashes.removeDuplicates();
    return hashes;
}

void BinController::initializeBin(Mlt::Playlist playlist)
{
    qDebug() << "init bin";
    // Load folders
    Mlt::Properties folderProperties;
    Mlt::Properties playlistProps(playlist.get_properties());
    folderProperties.pass_values(playlistProps, "kdenlive:folder.");
    QMap<QString, QString> foldersData;
    for (int i = 0; i < folderProperties.count(); i++) {
        foldersData.insert(folderProperties.get_name(i), folderProperties.get(i));
    }
    qDebug() << "Found " << folderProperties.count() << "folders";
    emit loadFolders(foldersData);

    // Read notes
    QString notes = playlistProps.get("kdenlive:documentnotes");
    emit setDocumentNotes(notes);

    // Fill Controller's list
    m_binPlaylist.reset(new Mlt::Playlist(playlist));
    m_binPlaylist->set("id", kPlaylistTrackId);
    qDebug() << "Found " << m_binPlaylist->count() << "clips";
    int max = m_binPlaylist->count();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<Mlt::Producer> producer(new Mlt::Producer(m_binPlaylist->get_clip(i)->parent()));
        qDebug() << "dealing with bin clip" << i;
        if (producer->is_blank() || !producer->is_valid()) {
            qDebug() << "producer is not valid or blank";
            continue;
        }
        QString id = producer->get("kdenlive:id");
        qDebug() << "clip id" << id;
        if (id.contains(QLatin1Char('_'))) {
            // This is a track producer
            QString mainId = id.section(QLatin1Char('_'), 0, 0);
            // QString track = id.section(QStringLiteral("_"), 1, 1);
            if (m_clipList.contains(mainId)) {
                // The controller for this track producer already exists
            } else {
                // Create empty controller for this clip
                requestClipInfo info;
                info.imageHeight = 0;
                info.clipId = id;
                info.replaceProducer = true;
                emit slotProducerReady(info, ClipController::mediaUnavailable);
            }
        } else {
            // Controller was already added by a track producer, add master now
            if (m_clipList.contains(id)) {
                m_clipList[id]->addMasterProducer(producer);
            } else {
                // Controller has not been created yet
                // fix MLT somehow adding root to color producer's resource (report upstream)
                if (strcmp(producer->get("mlt_service"), "color") == 0) {
                    QString color = producer->get("resource");
                    if (color.contains(QLatin1Char('/'))) {
                        color = color.section(QLatin1Char('/'), -1, -1);
                        producer->set("resource", color.toUtf8().constData());
                    }
                }
                requestClipInfo info;
                info.imageHeight = 0;
                info.clipId = id;
                info.replaceProducer = true;
                emit slotProducerReady(info, producer);
            }
        }
        emit loadingBin(i + 1);
    }
}

QMap<double, QString> BinController::takeGuidesData()
{
    QLocale locale;
    // Load guides
    Mlt::Properties guidesProperties;
    Mlt::Properties playlistProps(m_binPlaylist->get_properties());
    guidesProperties.pass_values(playlistProps, "kdenlive:guide.");
    qCDebug(KDENLIVE_LOG) << "***********\nFOUND GUIDES: " << guidesProperties.count() << "\n**********";
    QMap<double, QString> guidesData;
    for (int i = 0; i < guidesProperties.count(); i++) {
        double time = locale.toDouble(guidesProperties.get_name(i));
        guidesData.insert(time, guidesProperties.get(i));
        // Clear bin data
        QString propertyName = "kdenlive:guide." + QString(guidesProperties.get_name(i));
        m_binPlaylist->set(propertyName.toUtf8().constData(), (char *)nullptr);
    }
    return guidesData;
}
// TODO REFACTOR: DELETE
void BinController::createIfNeeded(Mlt::Profile *profile)
{
    if (m_binPlaylist.get()) {
        return;
    }
    m_binPlaylist.reset(new Mlt::Playlist(*profile));
    m_binPlaylist->set("id", kPlaylistTrackId);
}

void BinController::loadBinPlaylist(Mlt::Tractor *documentTractor, Mlt::Tractor *modelTractor)
{
    destroyBin();
    Mlt::Properties retainList((mlt_properties)documentTractor->get_data("xml_retain"));
    qDebug() << "Loading bin playlist...";
    if (retainList.is_valid() && (retainList.get_data(binPlaylistId().toUtf8().constData()) != nullptr)) {
        Mlt::Playlist playlist((mlt_playlist)retainList.get_data(binPlaylistId().toUtf8().constData()));
        qDebug() << "retain is valid";
        if (playlist.is_valid() && playlist.type() == playlist_type) {
            qDebug() << "playlist is valid";
            // Load bin clips
            initializeBin(playlist);
        }
    }
    // If no Playlist found, create new one
    if (!m_binPlaylist) {
        qDebug() << "no playlist valid, creating";
        m_binPlaylist.reset(new Mlt::Playlist(*modelTractor->profile()));
        m_binPlaylist->set("id", kPlaylistTrackId);
    }
    QString retain = QStringLiteral("xml_retain %1").arg(binPlaylistId());
    modelTractor->set(retain.toUtf8().constData(), m_binPlaylist->get_service(), 0);
}

void BinController::slotStoreFolder(const QString &folderId, const QString &parentId, const QString &oldParentId, const QString &folderName)
{
    if (!oldParentId.isEmpty()) {
        // Folder was moved, remove old reference
        QString oldPropertyName = "kdenlive:folder." + oldParentId + QLatin1Char('.') + folderId;
        m_binPlaylist->set(oldPropertyName.toUtf8().constData(), (char *)nullptr);
    }
    QString propertyName = "kdenlive:folder." + parentId + QLatin1Char('.') + folderId;
    if (folderName.isEmpty()) {
        // Remove this folder info
        m_binPlaylist->set(propertyName.toUtf8().constData(), (char *)nullptr);
    } else {
        m_binPlaylist->set(propertyName.toUtf8().constData(), folderName.toUtf8().constData());
    }
}

void BinController::storeMarker(const QString &markerId, const QString &markerHash)
{
    QString propertyName = "kdenlive:marker." + markerId;
    if (markerHash.isEmpty()) {
        // Remove this marker
        m_binPlaylist->set(propertyName.toUtf8().constData(), (char *)nullptr);
    } else {
        m_binPlaylist->set(propertyName.toUtf8().constData(), markerHash.toUtf8().constData());
    }
}

mlt_service BinController::service()
{
    return m_binPlaylist->get_service();
}

const QString BinController::binPlaylistId()
{
    return kPlaylistTrackId;
}

int BinController::clipCount() const
{
    return m_clipList.size();
}

void BinController::replaceProducer(const requestClipInfo &info, const std::shared_ptr<Mlt::Producer> &producer)
{
    if (!m_clipList.contains(info.clipId)) {
        qCDebug(KDENLIVE_LOG) << " / // error controller not found, crashing";
        return;
    }
    std::shared_ptr<ClipController> ctrl = m_clipList.value(info.clipId);
    pasteEffects(info.clipId, producer);
    ctrl->updateProducer(producer);
    replaceBinPlaylistClip(info.clipId, producer);
    producer->set("id", info.clipId.toUtf8().constData());
    // Remove video only producer
    QString videoId = info.clipId + QStringLiteral("_video");
    if (m_extraClipList.contains(videoId)) {
        m_extraClipList.remove(videoId);
    }
    removeBinPlaylistClip("#" + info.clipId);
    emit prepareTimelineReplacement(info);
}

void BinController::addClipToBin(const QString &id, const std::shared_ptr<ClipController> &controller)
{
    /** Test: we can use filters on clips in the bin this way
    Mlt::Filter f(*m_mltProfile, "sepia");
    producer.attach(f);
    */
    // append or replace clip in MLT's retain playlist
    replaceBinPlaylistClip(id, controller->originalProducer());
    if (!m_clipList.contains(id)) {
        m_clipList.insert(id, controller);
    }
}

void BinController::replaceBinPlaylistClip(const QString &id, const std::shared_ptr<Mlt::Producer> &producer)
{
    removeBinPlaylistClip(id);
    m_binPlaylist->append(*producer.get());
}

void BinController::pasteEffects(const QString &id, const std::shared_ptr<Mlt::Producer> &producer)
{
    int size = m_binPlaylist->count();
    for (int i = 0; i < size; i++) {
        QScopedPointer<Mlt::Producer> prod(m_binPlaylist->get_clip(i));
        QString prodId = prod->parent().get("id");
        if (prodId == id) {
            duplicateFilters(prod->parent(), *producer.get());
            break;
        }
    }
}

void BinController::removeBinPlaylistClip(const QString &id)
{
    int size = m_binPlaylist->count();
    for (int i = 0; i < size; i++) {
        QScopedPointer<Mlt::Producer> prod(m_binPlaylist->get_clip(i));
        QString prodId = prod->parent().get("id");
        if (prodId == id) {
            m_binPlaylist->remove(i);
            break;
        }
    }
}

bool BinController::hasClip(const QString &id)
{
    return m_clipList.contains(id);
}

bool BinController::removeBinClip(const QString &id)
{
    if (!m_clipList.contains(id)) {
        return false;
    }
    removeBinPlaylistClip(id);
    m_clipList.remove(id);
    return true;
}

Mlt::Producer *BinController::cloneProducer(Mlt::Producer &original)
{
    Clip clp(original);
    Mlt::Producer *clone = clp.clone();
    return clone;
}

std::shared_ptr<Mlt::Producer> BinController::getBinProducer(const QString &id)
{
    // TODO: framebuffer speed clips
    if (!m_clipList.contains(id)) {
        qDebug() << "ERROR: requesting invalid bin producer";
        return nullptr;
    }
    return m_clipList[id]->originalProducer();
}

Mlt::Producer *BinController::getBinVideoProducer(const QString &id)
{
    QString videoId = id + QStringLiteral("_video");
    if (!m_extraClipList.contains(videoId)) {
        // create clone
        QString originalId = id.section(QLatin1Char('_'), 0, 0);
        std::shared_ptr<Mlt::Producer> original = getBinProducer(originalId);
        Mlt::Producer *videoOnly = cloneProducer(*original.get());
        videoOnly->set("audio_index", -1);
        videoOnly->set("id", videoId.toUtf8().constData());
        m_extraClipList.insert(videoId, videoOnly);
        return videoOnly;
    }
    return m_extraClipList.value(videoId);
}

void BinController::duplicateFilters(Mlt::Producer original, Mlt::Producer clone)
{
    Mlt::Service clipService(original.get_service());
    Mlt::Service dupService(clone.get_service());
    for (int ix = 0; ix < clipService.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> filter(clipService.filter(ix));
        // Only duplicate Kdenlive filters
        if (filter->is_valid()) {
            QString effectId = filter->get("kdenlive_id");
            if (effectId.isEmpty()) {
                continue;
            }
            // looks like there is no easy way to duplicate a filter,
            // so we will create a new one and duplicate its properties
            auto *dup = new Mlt::Filter(*original.profile(), filter->get("mlt_service"));
            if ((dup != nullptr) && dup->is_valid()) {
                for (int i = 0; i < filter->count(); ++i) {
                    QString paramName = filter->get_name(i);
                    if (paramName.at(0) != QLatin1Char('_')) {
                        dup->set(filter->get_name(i), filter->get(i));
                    }
                }
                dupService.attach(*dup);
            }
            delete dup;
        }
    }
}

QStringList BinController::getClipIds() const
{
    return m_clipList.keys();
}

QString BinController::xmlFromId(const QString &id)
{
    if (!m_clipList.contains(id)) {
        qDebug() << "Error: impossible to retrieve xml from unknown bin clip";
        return QString();
    }
    std::shared_ptr<ClipController> controller = m_clipList[id];
    std::shared_ptr<Mlt::Producer> original = controller->originalProducer();
    QString xml = getProducerXML(original);
    QDomDocument mltData;
    mltData.setContent(xml);
    QDomElement producer = mltData.documentElement().firstChildElement(QStringLiteral("producer"));
    QString str;
    QTextStream stream(&str);
    producer.save(stream, 4);
    return str;
}

// static
QString BinController::getProducerXML(const std::shared_ptr<Mlt::Producer> &producer, bool includeMeta)
{
    Mlt::Consumer c(*producer->profile(), "xml", "string");
    Mlt::Service s(producer->get_service());
    if (!s.is_valid()) {
        return QString();
    }
    int ignore = s.get_int("ignore_points");
    if (ignore != 0) {
        s.set("ignore_points", 0);
    }
    c.set("time_format", "frames");
    if (!includeMeta) {
        c.set("no_meta", 1);
    }
    c.set("store", "kdenlive");
    c.set("no_root", 1);
    c.set("root", "/");
    c.connect(s);
    c.start();
    if (ignore != 0) {
        s.set("ignore_points", ignore);
    }
    return QString::fromUtf8(c.get("string"));
}

std::shared_ptr<ClipController> BinController::getController(const QString &id)
{
    if (!m_clipList.contains(id)) {
        qDebug() << "Error: invalid bin clip requested" << id;
        Q_ASSERT(false);
    }
    return m_clipList.value(id);
}

const QList<std::shared_ptr<ClipController>> BinController::getControllerList() const
{
    return m_clipList.values();
}

const QStringList BinController::getBinIdsByResource(const QFileInfo &url) const
{
    QStringList controllers;
    QMapIterator<QString, std::shared_ptr<ClipController>> i(m_clipList);
    while (i.hasNext()) {
        i.next();
        auto ctrl = i.value();
        if (QFileInfo(ctrl->clipUrl()) == url) {
            controllers << i.key();
        }
    }
    return controllers;
}

void BinController::updateTrackProducer(const QString &id)
{
    emit updateTimelineProducer(id);
}

void BinController::checkThumbnails(const QDir &thumbFolder)
{
    // Parse all controllers and load thumbnails
    QMapIterator<QString, std::shared_ptr<ClipController>> i(m_clipList);
    while (i.hasNext()) {
        i.next();
        std::shared_ptr<ClipController> ctrl = i.value();
        if (ctrl->clipType() == Audio) {
            // no thumbnails for audio clip
            continue;
        }
        bool foundFile = false;
        if (!ctrl->getClipHash().isEmpty()) {
            QImage img(thumbFolder.absoluteFilePath(ctrl->getClipHash() + QStringLiteral(".png")));
            if (!img.isNull()) {
                emit loadThumb(ctrl->clipId(), img, true);
                foundFile = true;
            }
        }
        if (!foundFile) {
            // Add clip id to thumbnail generation thread
            QDomDocument doc;
            ctrl->getProducerXML(doc);
            QDomElement xml = doc.documentElement().firstChildElement(QStringLiteral("producer"));
            if (!xml.isNull()) {
                xml.setAttribute(QStringLiteral("thumbnailOnly"), 1);
                emit createThumb(xml, ctrl->clipId(), 150);
            }
        }
    }
}

void BinController::checkAudioThumbs()
{
    QMapIterator<QString, std::shared_ptr<ClipController>> i(m_clipList);
    while (i.hasNext()) {
        i.next();
        std::shared_ptr<ClipController> ctrl = i.value();
        if (!ctrl->m_audioThumbCreated) {
            if (KdenliveSettings::audiothumbnails()) {
                // We want audio thumbnails
                emit requestAudioThumb(ctrl->clipId());
            } else {
                // Abort all pending thumb creation
                emit abortAudioThumbs();
                break;
            }
        }
    }
}

void BinController::saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata,
                                           std::shared_ptr<MarkerListModel> guideModel)
{
    // Clear previous properites
    Mlt::Properties playlistProps(m_binPlaylist->get_properties());
    Mlt::Properties docProperties;
    docProperties.pass_values(playlistProps, "kdenlive:docproperties.");
    for (int i = 0; i < docProperties.count(); i++) {
        QString propName = QStringLiteral("kdenlive:docproperties.") + docProperties.get_name(i);
        playlistProps.set(propName.toUtf8().constData(), (char *)nullptr);
    }

    // Clear previous metadata
    Mlt::Properties docMetadata;
    docMetadata.pass_values(playlistProps, "kdenlive:docmetadata.");
    for (int i = 0; i < docMetadata.count(); i++) {
        QString propName = QStringLiteral("kdenlive:docmetadata.") + docMetadata.get_name(i);
        playlistProps.set(propName.toUtf8().constData(), (char *)nullptr);
    }

    // Clear previous guides
    Mlt::Properties guideProperties;
    guideProperties.pass_values(playlistProps, "kdenlive:guide.");
    for (int i = 0; i < guideProperties.count(); i++) {
        QString propName = QStringLiteral("kdenlive:guide.") + guideProperties.get_name(i);
        playlistProps.set(propName.toUtf8().constData(), (char *)nullptr);
    }

    QMapIterator<QString, QString> i(props);
    while (i.hasNext()) {
        i.next();
        playlistProps.set(("kdenlive:docproperties." + i.key()).toUtf8().constData(), i.value().toUtf8().constData());
    }

    QMapIterator<QString, QString> j(metadata);
    while (j.hasNext()) {
        j.next();
        playlistProps.set(("kdenlive:docmetadata." + j.key()).toUtf8().constData(), j.value().toUtf8().constData());
    }

    // Append guides
    QLocale locale;
    for (const auto &guide : *guideModel) {
        QString propertyName = "kdenlive:guide." + locale.toString(guide.first.seconds());
        playlistProps.set(propertyName.toUtf8().constData(), guide.second.first.toUtf8().constData());
    }
}

void BinController::saveProperty(const QString &name, const QString &value)
{
    m_binPlaylist->set(name.toUtf8().constData(), value.toUtf8().constData());
}

const QString BinController::getProperty(const QString &name)
{
    return QString(m_binPlaylist->get(name.toUtf8().constData()));
}

QMap<QString, QString> BinController::getProxies()
{
    QMap<QString, QString> proxies;
    int size = m_binPlaylist->count();
    for (int i = 0; i < size; i++) {
        QScopedPointer<Mlt::Producer> prod(m_binPlaylist->get_clip(i));
        if (!prod->is_valid() || prod->is_blank()) {
            continue;
        }
        QString proxy = prod->parent().get("kdenlive:proxy");
        if (proxy.length() > 2) {
            if (QFileInfo(proxy).isRelative()) {
                proxy.prepend(m_documentRoot);
            }
            QString sourceUrl(prod->parent().get("kdenlive:originalurl"));
            if (QFileInfo(sourceUrl).isRelative()) {
                sourceUrl.prepend(m_documentRoot);
            }
            proxies.insert(proxy, sourceUrl);
        }
    }
    return proxies;
}
