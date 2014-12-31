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
#include "kdenlivesettings.h"

#include <QFileInfo>

static const char* kPlaylistTrackId = "main bin";

BinController::BinController(QString profileName)
{
    m_binPlaylist = NULL;
    m_mltProfile = NULL;
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
}

void BinController::initializeBin(Mlt::Playlist playlist)
{
    m_binPlaylist = new Mlt::Playlist(playlist);
    m_binPlaylist->set("id", kPlaylistTrackId);
    rebuildIndex();
}

void BinController::createIfNeeded()
{
    if (m_binPlaylist) return;
    m_binPlaylist = new Mlt::Playlist(*m_mltProfile);
    m_binPlaylist->set("id", kPlaylistTrackId);
}

const QString BinController::id()
{
    return kPlaylistTrackId;
}


mlt_service BinController::service()
{
    return m_binPlaylist->get_service();
}

int BinController::clipCount() const
{
    return m_binIdList.size();
}

Mlt::Producer *BinController::getOriginalProducerAtIndex(int ix)
{
    if (ix < 0 || ix > m_binIdList.size()) return NULL;
    return m_binPlaylist->get_clip(ix);
}

void BinController::addClipToBin(const QString &id, Mlt::Producer &producer)
{
    /** Test: we can use filters on clips in the bin this way
    Mlt::Filter f(*m_mltProfile, "sepia");
    producer.attach(f);
    */
    if (m_binIdList.contains(id)) {
        // we are replacing a producer
        //TODO: replace it in timeline
        removeBinClip(id);
    }
    m_binPlaylist->append(producer);
    m_binIdList.append(id);
}

bool BinController::hasClip(const QString &id)
{
    return m_binIdList.contains(id);
}


int BinController::getBinClipDuration(const QString &id)
{
    Mlt::Producer *prod = getBinClip(id, -1);
    if (prod == NULL) return -1;
    int length = prod->get_playtime();
    delete prod;
    return length;
}

bool BinController::removeBinClip(const QString &id)
{
    Mlt::Producer *producer = NULL;
    int index = m_binIdList.indexOf(id);
    if (index == -1) {
	// Clip not found in our bin's list.
	return false;
    }
    bool error;
    if (index > 0 && index < m_binPlaylist->count()) {
	error = m_binPlaylist->remove(index);
    }
    rebuildIndex();
    return !error;
}

void BinController::rebuildIndex()
{
    // Rebuild bin index
    m_binIdList.clear();
    Mlt::Producer *producer;
    for (int ix = 0;ix < m_binPlaylist->count(); ix++) {
        producer = m_binPlaylist->get_clip(ix);
        if (!producer) continue;
        m_binIdList.append(QString::fromUtf8(producer->parent().get("id")));
        delete producer;
    }
}

Mlt::Producer *BinController::getBinClip(const QString &id, int track, int clipState, double speed)
{
    //qDebug()<<" ++++++++++++++++\nGet BIN CLIP: "<<id<<"\nLIST: "<<m_binIdList<<"\n+++++++++++++";
    QString clipWithTrackId = id;
    if (track > -1) {
	clipWithTrackId.append("_" + QString::number(track));
    }
    if (clipState == PlaylistState::AudioOnly) clipWithTrackId.append("_audio");
    else if (clipState == PlaylistState::VideoOnly) clipWithTrackId.append("_video");

    // TODO: framebuffer speed clips
    int index = m_binIdList.indexOf(clipWithTrackId);
    if (index == -1 && clipWithTrackId != id) {
	// Clip not found, try to create it if a master clip exists
	index = m_binIdList.indexOf(id);
	if (index == -1) {
	    // Error cannot find the requested clip
	    return NULL;
	}
	// Create a clone of the master producer for this track
	Mlt::Producer *original = m_binPlaylist->get_clip(index);
	QString xml = getProducerXML(original->parent());
	Mlt::Producer *clone = new Mlt::Producer(*m_mltProfile, "xml-string", xml.toUtf8().constData());
	clone->set("id", clipWithTrackId.toUtf8().constData());
	delete original;
	//duplicateFilters(original->parent(), *clone);
	return clone;
    }
    if (index == -1) return NULL;
    return m_binPlaylist->get_clip(index);
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

QString BinController::xmlFromId(const QString & id)
{
    int index = m_binIdList.indexOf(id);
    Mlt::Producer *original = m_binPlaylist->get_clip(index);
    if (original == NULL) return QString();
    QString xml = getProducerXML(original->parent());
    delete original;
    QDomDocument mltData;
    mltData.setContent(xml);
    QDomElement producer = mltData.documentElement().firstChildElement("producer");
    QString str;
    QTextStream stream(&str);
    producer.save(stream, 4);
    qDebug()<<"PRODUCER XML\n-----------\n"<<str;
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

int BinController::clipBinIndex(const QString &id) const
{
    return m_binIdList.indexOf(id) + 1;
}
