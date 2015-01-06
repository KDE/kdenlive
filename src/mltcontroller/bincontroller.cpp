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
#include "clipcontroller.h"
#include "kdenlivesettings.h"

#include <QFileInfo>

static const char* kPlaylistTrackId = "main bin";

BinController::BinController(QString profileName) :
  QObject()
{
    m_mltProfile = NULL;
    m_binPlaylist = NULL;
    Mlt::Factory::init();
    if (profileName.isEmpty()) {
        profileName = KdenliveSettings::current_profile();
    }
    resetProfile(profileName);
}

BinController::~BinController()
{
    delete m_mltProfile;
}

void BinController::resetProfile(const QString &newProfile)
{
    m_activeProfile = newProfile;
    if (m_mltProfile) {
        Mlt::Profile tmpProfile(m_activeProfile.toUtf8().constData());
        m_mltProfile->set_colorspace(tmpProfile.colorspace());
        m_mltProfile->set_frame_rate(tmpProfile.frame_rate_num(), tmpProfile.frame_rate_den());
        m_mltProfile->set_height(tmpProfile.height());
        m_mltProfile->set_width(tmpProfile.width());
        m_mltProfile->set_progressive(tmpProfile.progressive());
        m_mltProfile->set_sample_aspect(tmpProfile.sample_aspect_num(), tmpProfile.sample_aspect_den());
        m_mltProfile->get_profile()->display_aspect_num = tmpProfile.display_aspect_num();
        m_mltProfile->get_profile()->display_aspect_den = tmpProfile.display_aspect_den();
    } else {
        m_mltProfile = new Mlt::Profile(m_activeProfile.toUtf8().constData());
    }
    setenv("MLT_PROFILE", m_activeProfile.toUtf8().constData(), 1);
    m_mltProfile->set_explicit(true);
    KdenliveSettings::setCurrent_profile(m_activeProfile);
}

Mlt::Profile *BinController::profile()
{
    return m_mltProfile;
}

void BinController::destroyBin()
{
    if (m_binPlaylist) {
        m_binPlaylist->clear();
        delete m_binPlaylist;
        m_binPlaylist = NULL;
    }
    // Controllers are deleted from the Bin's ProjectClip
    m_clipList.clear();
}

void BinController::initializeBin(Mlt::Playlist playlist)
{
    // Load folders
    Mlt::Properties foldeProperties;
    Mlt::Properties playlistProps(playlist.get_properties());
    foldeProperties.pass_values(playlistProps, "kdenlive.folder.");
    QMap <QString,QString> foldersData;
    for (int i = 0; i < foldeProperties.count(); i++) {
        foldersData.insert(foldeProperties.get_name(i), foldeProperties.get(i));
    }
    emit loadFolders(foldersData);

    // Fill Controller's list
    m_binPlaylist = new Mlt::Playlist(playlist);
    m_binPlaylist->set("id", kPlaylistTrackId);
    for (int i = 0; i < m_binPlaylist->count(); i++) {
        Mlt::Producer *producer = m_binPlaylist->get_clip(i);
        if (producer->is_blank() || !producer->is_valid()) continue;
        QString id = producer->parent().get("id");
        if (id.contains("_")) {
            // This is a track producer
            QString mainId = id.section("_", 0, 0);
            int track = id.section("_", 1, 1).toInt();
            if (m_clipList.contains(mainId)) {
                // The controller for this track producer already exists
                m_clipList.value(mainId)->appendTrackProducer(track, producer->parent());
            }
            else {
                // Create empty controller for this track
                ClipController *controller = new ClipController(this);
                controller->appendTrackProducer(track, producer->parent());
                m_clipList.insert(mainId, controller);
            }
        }
        else {
            if (m_clipList.contains(id)) {
                //Controller was already added by a track producer, add master now
                m_clipList.value(id)->addMasterProducer(producer->parent());
            }
            else {
                // Controller has not been created yet
                ClipController *controller = new ClipController(this, producer->parent());
                m_clipList.insert(id, controller);
            }
        }
    }
}

void BinController::createIfNeeded()
{
    if (m_binPlaylist) return;
    m_binPlaylist = new Mlt::Playlist(*m_mltProfile);
    m_binPlaylist->set("id", kPlaylistTrackId);
}

void BinController::slotStoreFolder(const QString &folderId, const QString &folderName)
{
    QString propertyName = "kdenlive.folder." + folderId;
    if (folderName.isEmpty()) {
        // Remove this folder info
        m_binPlaylist->set(propertyName.toUtf8().constData(), (char *) NULL);
    }
    else {
        m_binPlaylist->set(propertyName.toUtf8().constData(), folderName.toUtf8().constData());
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

void BinController::replaceProducer(const QString &id, Mlt::Producer &producer)
{
    ClipController *ctrl = m_clipList.value(id);
    if (!ctrl) {
        qDebug()<<" / // errror controller not foound, crashiong";
        return;
    }
    ctrl->updateProducer(id, &producer);
    replaceBinPlaylistClip(id, producer);
    producer.set("id", id.toUtf8().constData());
}

void BinController::addClipToBin(const QString &id, ClipController *controller) // Mlt::Producer &producer)
{
    /** Test: we can use filters on clips in the bin this way
    Mlt::Filter f(*m_mltProfile, "sepia");
    producer.attach(f);
    */
    // append or replace clip in MLT's retain playlist
    replaceBinPlaylistClip(id, controller->originalProducer());

    if (m_clipList.contains(id)) {
        // There is something wrong, we should not be recreating an existing controller!
        // we are replacing a producer
        //TODO: replace it in timeline
        /*ClipController *c2 = m_clipList.value(id);
        c2->updateProducer(id, &controller->originalProducer());
        controller->originalProducer().set("id", id.toUtf8().constData());*/
        //removeBinClip(id);
    }
    else m_clipList.insert(id, controller);
}

void BinController::replaceBinPlaylistClip(const QString &id, Mlt::Producer &producer)
{
    removeBinPlaylistClip(id);
    m_binPlaylist->append(producer);
}

void BinController::removeBinPlaylistClip(const QString &id)
{
    int size = m_binPlaylist->count();
    for (int i = 0; i < size; i++) {
        Mlt::Producer *prod = m_binPlaylist->get_clip(i);
        QString prodId = prod->parent().get("id");
        if (prodId == id) {
            m_binPlaylist->remove(i);
            delete prod;
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
    if (!m_clipList.contains(id)) return false;
    removeBinPlaylistClip(id);
    ClipController *controller = m_clipList.take(id);
    delete controller;
    return true;
}


Mlt::Producer *BinController::cloneProducer(Mlt::Producer &original)
{
    QString xml = getProducerXML(original);
    Mlt::Producer *clone = new Mlt::Producer(*m_mltProfile, "xml-string", xml.toUtf8().constData());
    return clone;
}

Mlt::Producer *BinController::getBinClip(const QString &id, int track, PlaylistState::ClipState clipState, double speed)
{
    if (!m_clipList.contains(id)) return NULL;
    // TODO: framebuffer speed clips
    ClipController *controller = m_clipList.value(id);
    return controller->getTrackProducer(track, clipState, speed);
}

double BinController::fps() const
{
    return m_mltProfile->fps();
}

double BinController::dar() const
{
    return m_mltProfile->dar();
}

void BinController::duplicateFilters(Mlt::Producer original, Mlt::Producer clone)
{
    Mlt::Service clipService(original.get_service());
    Mlt::Service dupService(clone.get_service());
    //delete original;
    //delete clone;
    int ct = 0;
    Mlt::Filter *filter = clipService.filter(ct);
    while (filter) {
        // Only duplicate Kdenlive filters, and skip the fade in effects
	fprintf(stderr, "CHKNG FILTER: %s\n", filter->get("kdenlive_id"));
        if (filter->is_valid()/* && strcmp(filter->get("kdenlive_id"), "") && strcmp(filter->get("kdenlive_id"), "fadein") && strcmp(filter->get("kdenlive_id"), "fade_from_black")*/) {
            // looks like there is no easy way to duplicate a filter,
            // so we will create a new one and duplicate its properties
            Mlt::Filter *dup = new Mlt::Filter(*m_mltProfile, filter->get("mlt_service"));
            if (dup && dup->is_valid()) {
                Mlt::Properties entries(filter->get_properties());
                for (int i = 0; i < entries.count(); ++i) {
                    dup->set(entries.get_name(i), entries.get(i));
                }
                dupService.attach(*dup);
            }
        }
        ct++;
        filter = clipService.filter(ct);
    }
}

QStringList BinController::getClipIds() const
{
    return m_clipList.keys();
}

QString BinController::xmlFromId(const QString & id)
{
    ClipController *controller = m_clipList.value(id);
    if (!controller) return NULL;
    Mlt::Producer original = controller->originalProducer();
    QString xml = getProducerXML(original);
    QDomDocument mltData;
    mltData.setContent(xml);
    QDomElement producer = mltData.documentElement().firstChildElement("producer");
    QString str;
    QTextStream stream(&str);
    producer.save(stream, 4);
    return str;
}

QString BinController::getProducerXML(Mlt::Producer &producer)
{
    QString filename = "string";
    Mlt::Consumer c(*m_mltProfile, "xml", filename.toUtf8().constData());
    Mlt::Service s(producer.get_service());
    if (!s.is_valid())
        return "";
    int ignore = s.get_int("ignore_points");
    if (ignore)
        s.set("ignore_points", 0);
    c.set("time_format", "frames");
    c.set("no_meta", 1);
    c.set("store", "kdenlive");
    if (filename != "string") {
        c.set("no_root", 1);
        c.set("root", QFileInfo(filename).absolutePath().toUtf8().constData());
    }
    c.connect(s);
    c.start();
    if (ignore)
        s.set("ignore_points", ignore);
    return QString::fromUtf8(c.get(filename.toUtf8().constData()));
}

ClipController *BinController::getController(const QString &id)
{
    return m_clipList.value(id);
}

const QList <ClipController *> BinController::getControllerList() const
{
    return m_clipList.values();
}

const QStringList BinController::getBinIdsByResource(const QUrl &url) const
{
    QStringList controllers;
    QMapIterator<QString, ClipController *> i(m_clipList);
    while (i.hasNext()) {
        i.next();
        ClipController *ctrl = i.value();
        if (ctrl->clipUrl() == url) {
            controllers << i.key();
        }
    }
    return controllers;
}
