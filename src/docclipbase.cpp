/***************************************************************************
 *                         DocClipBase.cpp  -  description                 *
 *                           -------------------                           *
 *   begin                : Fri Apr 12 2002                                *
 *   Copyright (C) 2002 by Jason Wood (jasonwood@blueyonder.co.uk)         *
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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




#include "docclipbase.h"
#include "kdenlivesettings.h"
#include "kthumb.h"
#include "clipmanager.h"
#include "slideshowclip.h"

#include <KIO/NetAccess>
#include <KStandardDirs>
#include <KDebug>

#include <QCryptographicHash>
#include <QtConcurrentRun>

#include <cstdio>

DocClipBase::DocClipBase(ClipManager *clipManager, QDomElement xml, const QString &id) :
        QObject(),
        m_audioFrameCache(),
        m_refcount(0),
        m_baseTrackProducers(),
        m_videoTrackProducers(),
        m_audioTrackProducers(),
        m_snapMarkers(QList < CommentedTime >()),
        m_duration(),
        m_thumbProd(NULL),
        m_audioThumbCreated(false),
        m_id(id),
        m_placeHolder(xml.hasAttribute("placeholder")),
        m_properties()
{
    int type = xml.attribute("type").toInt();
    m_clipType = (CLIPTYPE) type;
    if (m_placeHolder) xml.removeAttribute("placeholder");
    QDomNamedNodeMap attributes = xml.attributes();
    for (int i = 0; i < attributes.count(); i++) {
        QString name = attributes.item(i).nodeName();
        if (name.startsWith("meta.attr.")) {
            m_metadata.insert(name.section('.', 2, 3), attributes.item(i).nodeValue());
        } else m_properties.insert(name, attributes.item(i).nodeValue());
    }

    if (xml.hasAttribute("cutzones")) {
        QStringList cuts = xml.attribute("cutzones").split(';', QString::SkipEmptyParts);
        for (int i = 0; i < cuts.count(); i++) {
            QString z = cuts.at(i);
            addCutZone(z.section('-', 0, 0).toInt(), z.section('-', 1, 1).toInt(), z.section('-', 2, 2));
        }
    }

    KUrl url = KUrl(xml.attribute("resource"));
    if (!m_properties.contains("file_hash") && !url.isEmpty()) getFileHash(url.path());

    if (xml.hasAttribute("duration")) {
        setDuration(GenTime(xml.attribute("duration").toInt(), KdenliveSettings::project_fps()));
    } else {
        int out = xml.attribute("out").toInt();
        int in = xml.attribute("in").toInt();
        setDuration(GenTime(out - in, KdenliveSettings::project_fps()));
    }

    if (!m_properties.contains("name")) m_properties.insert("name", url.fileName());

    m_thumbProd = new KThumb(clipManager, url, m_id, m_properties.value("file_hash"));
    
    // Setup timer to trigger audio thumbs creation
    m_audioTimer.setSingleShot(true);
    m_audioTimer.setInterval(800);
    connect(&m_audioTimer, SIGNAL(timeout()), m_thumbProd, SLOT(slotCreateAudioThumbs()));
    
}

DocClipBase::~DocClipBase()
{
    m_audioTimer.stop();
    delete m_thumbProd;
    m_thumbProd = NULL;
    qDeleteAll(m_toDeleteProducers);
    m_toDeleteProducers.clear();
    qDeleteAll(m_baseTrackProducers);
    m_baseTrackProducers.clear();
    qDeleteAll(m_audioTrackProducers);
    m_audioTrackProducers.clear();
    qDeleteAll(m_videoTrackProducers);
    m_videoTrackProducers.clear();
}

void DocClipBase::setZone(QPoint zone)
{
    m_properties.insert("zone_in", QString::number(zone.x()));
    m_properties.insert("zone_out", QString::number(zone.y()));
}

QPoint DocClipBase::zone() const
{
    QPoint zone(m_properties.value("zone_in", "0").toInt(), m_properties.value("zone_out", "50").toInt());
    return zone;
}


bool DocClipBase::hasAudioThumb() const
{
    if (m_clipType == AUDIO || m_clipType == AV || m_clipType == PLAYLIST) return true;
    return false;
}

void DocClipBase::slotClearAudioCache()
{
    m_audioFrameCache.clear();
    m_audioThumbCreated = false;
}

/*void DocClipBase::getClipMainThumb() {
    if (m_thumbProd) m_thumbProd->getMainThumb(m_properties.value("thumbnail").toInt());
}*/

KThumb *DocClipBase::thumbProducer()
{
    return m_thumbProd;
}

bool DocClipBase::audioThumbCreated() const
{
    return m_audioThumbCreated;
}

const QString DocClipBase::name() const
{

    return m_properties.value("name");
}

const QString &DocClipBase::getId() const
{
    return m_id;
}

const CLIPTYPE & DocClipBase::clipType() const
{
    return m_clipType;
}

void DocClipBase::setClipType(CLIPTYPE type)
{
    m_clipType = type;
    m_properties.insert("type", QString::number((int) type));
}

KUrl DocClipBase::fileURL() const
{
    QString res = m_properties.value("resource");
    if (m_clipType != COLOR && !res.isEmpty()) return KUrl(res);
    return KUrl();
}

void DocClipBase::setClipThumbFrame(const uint &ix)
{
    m_properties.insert("thumbnail", QString::number((int) ix));
}

uint DocClipBase::getClipThumbFrame() const
{
    return (uint) m_properties.value("thumbnail").toInt();
}

const QString DocClipBase::description() const
{
    return m_properties.value("description");
}

bool DocClipBase::isTransparent() const
{
    return (m_properties.value("transparency") == "1");
}

const QString DocClipBase::getProperty(const QString &prop) const
{
    return m_properties.value(prop);
}

void DocClipBase::setDuration(GenTime dur)
{
    m_duration = dur;
    m_properties.insert("duration", QString::number((int) dur.frames(KdenliveSettings::project_fps())));
}

const GenTime &DocClipBase::duration() const
{
    return m_duration;
}

const GenTime DocClipBase::maxDuration() const
{
    if (m_clipType == COLOR || m_clipType == IMAGE || m_clipType == TEXT || (m_clipType == SLIDESHOW &&  m_properties.value("loop") == "1")) {
        /*const GenTime dur(15000, KdenliveSettings::project_fps());
        return dur;*/
        return GenTime();
    }
    return m_duration;
}

bool DocClipBase::hasFileSize() const
{
    return true;
}

qulonglong DocClipBase::fileSize() const
{
    return m_properties.value("file_size").toULongLong();
}

// virtual
QDomElement DocClipBase::toXML(bool hideTemporaryProperties) const
{
    QDomDocument doc;
    QDomElement clip = doc.createElement("producer");

    QMapIterator<QString, QString> i(m_properties);
    while (i.hasNext()) {
        i.next();
        if (hideTemporaryProperties && i.key().startsWith('_')) continue;
        if (!i.value().isEmpty()) clip.setAttribute(i.key(), i.value());
    }
    doc.appendChild(clip);
    if (!m_cutZones.isEmpty()) {
        QStringList cuts;
        for (int i = 0; i < m_cutZones.size(); i++) {
            CutZoneInfo info = m_cutZones.at(i);
            cuts << QString::number(info.zone.x()) + "-" + QString::number(info.zone.y()) + "-" + info.description;
        }
        clip.setAttribute("cutzones", cuts.join(";"));
    }
    //kDebug() << "/// CLIP XML: " << doc.toString();
    return doc.documentElement();
}


void DocClipBase::setAudioThumbCreated(bool isDone)
{
    m_audioThumbCreated = isDone;
}

void DocClipBase::updateAudioThumbnail(const audioByteArray& data)
{
    //kDebug() << "CLIPBASE RECIEDVED AUDIO DATA*********************************************";
    m_audioFrameCache = data;
    m_audioThumbCreated = true;
    emit gotAudioData();
}

QList < GenTime > DocClipBase::snapMarkers() const
{
    QList < GenTime > markers;

    for (int count = 0; count < m_snapMarkers.count(); ++count) {
        markers.append(m_snapMarkers.at(count).time());
    }

    return markers;
}

QList < CommentedTime > DocClipBase::commentedSnapMarkers() const
{
    return m_snapMarkers;
}


void DocClipBase::addSnapMarker(const GenTime & time, QString comment)
{
    QList < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() >= time)
            break;
    }

    if ((it != m_snapMarkers.end()) && ((*it).time() == time)) {
        (*it).setComment(comment);
        //kError() << "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo";
    } else {
        CommentedTime t(time, comment);
        m_snapMarkers.insert(it, t);
    }

}

void DocClipBase::editSnapMarker(const GenTime & time, QString comment)
{
    QList < CommentedTime >::Iterator it;
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() == time)
            break;
    }
    if (it != m_snapMarkers.end()) {
        (*it).setComment(comment);
    } else {
        kError() << "trying to edit Snap Marker that does not already exists";
    }
}

QString DocClipBase::deleteSnapMarker(const GenTime & time)
{
    QString result = i18n("Marker");
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == time)
            break;
        ++itt;
    }

    if ((itt != m_snapMarkers.end()) && ((*itt).time() == time)) {
        result = (*itt).comment();
        m_snapMarkers.erase(itt);
    }
    return result;
}


GenTime DocClipBase::hasSnapMarkers(const GenTime & time)
{
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == time)
            return time;
        ++itt;
    }

    return GenTime(0.0);
}

GenTime DocClipBase::findPreviousSnapMarker(const GenTime & currTime)
{
    int it;
    for (it = 0; it < m_snapMarkers.count(); it++) {
        if (m_snapMarkers.at(it).time() >= currTime)
            break;
    }
    if (it == 0) return GenTime();
    else if (it == m_snapMarkers.count() - 1 && m_snapMarkers.at(it).time() < currTime)
        return m_snapMarkers.at(it).time();
    else return m_snapMarkers.at(it - 1).time();
}

GenTime DocClipBase::findNextSnapMarker(const GenTime & currTime)
{
    int it;
    for (it = 0; it < m_snapMarkers.count(); it++) {
        if (m_snapMarkers.at(it).time() > currTime)
            break;
    }
    if (it < m_snapMarkers.count() && m_snapMarkers.at(it).time() > currTime) return m_snapMarkers.at(it).time();
    return duration();
}

QString DocClipBase::markerComment(GenTime t)
{
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == t)
            return (*itt).comment();
        ++itt;
    }
    return QString();
}

void DocClipBase::clearThumbProducer()
{
    if (m_thumbProd) m_thumbProd->clearProducer();
}

void DocClipBase::reloadThumbProducer()
{
    if (m_thumbProd && !m_thumbProd->hasProducer())
        m_thumbProd->setProducer(getProducer());
}

void DocClipBase::deleteProducers()
{
    if (m_thumbProd) m_thumbProd->clearProducer();
    
    if (numReferences() > 0 && (!m_baseTrackProducers.isEmpty() || !m_videoTrackProducers.isEmpty() || !m_audioTrackProducers.isEmpty())) {
        // Clip is used in timeline, delay producers deletion
        for (int i = 0; i < m_baseTrackProducers.count(); i++) {
            m_toDeleteProducers.append(m_baseTrackProducers.at(i));
        }
        for (int i = 0; i < m_videoTrackProducers.count(); i++) {
            m_toDeleteProducers.append(m_videoTrackProducers.at(i));
        }
        for (int i = 0; i < m_audioTrackProducers.count(); i++) {
            m_toDeleteProducers.append(m_audioTrackProducers.at(i));
        }
    }
    else {
        qDeleteAll(m_baseTrackProducers);
	qDeleteAll(m_videoTrackProducers);
        qDeleteAll(m_audioTrackProducers);
        m_replaceMutex.unlock();
    }
    m_baseTrackProducers.clear();
    m_videoTrackProducers.clear();
    m_audioTrackProducers.clear();
}

void DocClipBase::cleanupProducers()
{
    /*
    int ct = 0;
    kDebug()<<"----------------------------------------------------------------------------------";
    for (int i = 0; i < m_toDeleteProducers.count(); i++) {
        if (m_toDeleteProducers.at(i) != NULL) {
            Mlt::Properties props(m_toDeleteProducers.at(i)->get_properties());
            if (props.ref_count() > 2) {
                kDebug()<<"PRODUCER: "<<i<<", COUNTS: "<<props.ref_count();
                //exit(1);
            }
            ct++;
        }
    }*/

    if (!isClean()) {
      qDeleteAll(m_toDeleteProducers);
      m_toDeleteProducers.clear();
      m_replaceMutex.unlock();
    }
}

bool DocClipBase::isClean() const
{
    return m_toDeleteProducers.isEmpty();
}

void DocClipBase::setValid()
{
    m_placeHolder = false;
}

void DocClipBase::setProducer(Mlt::Producer *producer, bool reset, bool readPropertiesFromProducer)
{
    if (producer == NULL) return;
    if (reset) {
        QMutexLocker locker(&m_producerMutex);
        m_replaceMutex.lock();
        deleteProducers();
    }
    QString id = producer->get("id");
    if (m_placeHolder || !producer->is_valid()) {
        char *tmp = qstrdup(i18n("Missing clip").toUtf8().constData());
        producer->set("markup", tmp);
        producer->set("bgcolour", "0xff0000ff");
        producer->set("pad", "10");
        delete[] tmp;
    }
    else if (m_thumbProd && !m_thumbProd->hasProducer()) {
        if (m_clipType != AUDIO) {
            if (!id.endsWith("_audio"))
                m_thumbProd->setProducer(producer);
        }
        else m_thumbProd->setProducer(producer);
        getAudioThumbs();
    }
    bool updated = false;
    if (id.contains('_')) {
        // this is a subtrack producer, insert it at correct place
        id = id.section('_', 1);
        if (id.endsWith("audio")) {
            int pos = id.section('_', 0, 0).toInt();
            if (pos >= m_audioTrackProducers.count()) {
                while (m_audioTrackProducers.count() - 1 < pos) {
                    m_audioTrackProducers.append(NULL);
                }
            }
            if (m_audioTrackProducers.at(pos) == NULL) {
                m_audioTrackProducers[pos] = producer;
                updated = true;
            }
            else delete producer;
            return;
        } else if (id.endsWith("video")) {
	    int pos = 0;
	    // Keep compatibility with older projects where video only producers were not track specific
	    if (id.contains('_')) pos = id.section('_', 0, 0).toInt();
            if (pos >= m_videoTrackProducers.count()) {
                while (m_videoTrackProducers.count() - 1 < pos) {
                    m_videoTrackProducers.append(NULL);
                }
            }
            if (m_videoTrackProducers.at(pos) == NULL) {
                m_videoTrackProducers[pos] = producer;
                updated = true;
            }
            else delete producer;
            return;
        }
        int pos = id.toInt();
        if (pos >= m_baseTrackProducers.count()) {
            while (m_baseTrackProducers.count() - 1 < pos) {
                m_baseTrackProducers.append(NULL);
            }
        }
        if (m_baseTrackProducers.at(pos) == NULL) {
            m_baseTrackProducers[pos] = producer;
            updated = true;
        }
        else delete producer;
    } else {
        if (m_baseTrackProducers.isEmpty()) {
            m_baseTrackProducers.append(producer);
            updated = true;
        }
        else if (m_baseTrackProducers.at(0) == NULL) {
            m_baseTrackProducers[0] = producer;
            updated = true;
        }
        else delete producer;
    }
    if (updated && readPropertiesFromProducer && (m_clipType != COLOR && m_clipType != IMAGE && m_clipType != TEXT))
        setDuration(GenTime(producer->get_length(), KdenliveSettings::project_fps()));
}

static double getPixelAspect(QMap<QString, QString>& props) {
    int width = props.value("frame_size").section('x', 0, 0).toInt();
    int height = props.value("frame_size").section('x', 1, 1).toInt();
    int aspectNumerator = props.value("force_aspect_num").toInt();
    int aspectDenominator = props.value("force_aspect_den").toInt();
    if (aspectDenominator != 0 && width != 0)
        return double(height) * aspectNumerator / aspectDenominator / width;    
    else
        return 1.0;
}

Mlt::Producer *DocClipBase::audioProducer(int track)
{
    QMutexLocker locker(&m_producerMutex);
    if (m_audioTrackProducers.count() <= track) {
        while (m_audioTrackProducers.count() - 1 < track) {
            m_audioTrackProducers.append(NULL);
        }
    }
    if (m_audioTrackProducers.at(track) == NULL) {
        int i;
        for (i = 0; i < m_audioTrackProducers.count(); i++)
            if (m_audioTrackProducers.at(i) != NULL) break;
        Mlt::Producer *base;
        if (i >= m_audioTrackProducers.count()) {
            // Could not find a valid producer for that clip
            locker.unlock();
            base = getProducer();
            if (base == NULL) {
                return NULL;
            }
            locker.relock();
        }
        else base = m_audioTrackProducers.at(i);
        m_audioTrackProducers[track] = cloneProducer(base);
        adjustProducerProperties(m_audioTrackProducers.at(track), QString(getId() + '_' + QString::number(track) + "_audio"), false, true);
    }
    return m_audioTrackProducers.at(track);
}


void DocClipBase::adjustProducerProperties(Mlt::Producer *prod, const QString &id, bool mute, bool blind)
{
        if (m_properties.contains("force_aspect_num") && m_properties.contains("force_aspect_den") && m_properties.contains("frame_size"))
            prod->set("force_aspect_ratio", getPixelAspect(m_properties));
        if (m_properties.contains("force_fps")) prod->set("force_fps", m_properties.value("force_fps").toDouble());
        if (m_properties.contains("force_progressive")) prod->set("force_progressive", m_properties.value("force_progressive").toInt());
        if (m_properties.contains("force_tff")) prod->set("force_tff", m_properties.value("force_tff").toInt());
        if (m_properties.contains("threads")) prod->set("threads", m_properties.value("threads").toInt());
        if (mute) prod->set("audio_index", -1);
        else if (m_properties.contains("audio_index")) prod->set("audio_index", m_properties.value("audio_index").toInt());
        if (blind) prod->set("video_index", -1);
        else if (m_properties.contains("video_index")) prod->set("video_index", m_properties.value("video_index").toInt());
        prod->set("id", id.toUtf8().constData());
        if (m_properties.contains("force_colorspace")) prod->set("force_colorspace", m_properties.value("force_colorspace").toInt());
        if (m_properties.contains("full_luma")) prod->set("set.force_full_luma", m_properties.value("full_luma").toInt());
        if (m_properties.contains("proxy_out")) {
            // We have a proxy clip, make sure the proxy has same duration as original
            prod->set("length", m_properties.value("duration").toInt());
            prod->set("out", m_properties.value("proxy_out").toInt());
        }

}

Mlt::Producer *DocClipBase::videoProducer(int track)
{
    QMutexLocker locker(&m_producerMutex);
    if (m_videoTrackProducers.count() <= track) {
        while (m_videoTrackProducers.count() - 1 < track) {
            m_videoTrackProducers.append(NULL);
        }
    }
    if (m_videoTrackProducers.at(track) == NULL) {
        int i;
        for (i = 0; i < m_videoTrackProducers.count(); i++)
            if (m_videoTrackProducers.at(i) != NULL) break;
        Mlt::Producer *base;
        if (i >= m_videoTrackProducers.count()) {
            // Could not find a valid producer for that clip
            locker.unlock();
            base = getProducer();
            if (base == NULL) {
                return NULL;
            }
            locker.relock();
        }
        else base = m_videoTrackProducers.at(i);
        m_videoTrackProducers[track] = cloneProducer(base);
        adjustProducerProperties(m_videoTrackProducers.at(track), QString(getId() + '_' + QString::number(track) + "_video"), true, false);
    }
    return m_videoTrackProducers.at(track);
}

Mlt::Producer *DocClipBase::getCloneProducer()
{
    Mlt::Producer *source = NULL;
    Mlt::Producer *prod = NULL;
    if (m_clipType != AUDIO && m_clipType != AV && m_clipType != PLAYLIST) {
        source = getProducer();
        if (!source) return NULL;
    }
    if (m_clipType == COLOR) {
        prod = new Mlt::Producer(*(source->profile()), 0, QString("colour:" + QString(source->get("resource"))).toUtf8().constData());
    } else if (m_clipType == TEXT) {
        prod = new Mlt::Producer(*(source->profile()), 0, QString("kdenlivetitle:" + QString(source->get("resource"))).toUtf8().constData());
        if (prod && prod->is_valid() && m_properties.contains("xmldata"))
            prod->set("xmldata", m_properties.value("xmldata").toUtf8().constData());
    }
    if (!prod) {
        if (!source) {
            QMutexLocker locker(&m_producerMutex);
            for (int i = 0; i < m_baseTrackProducers.count(); i++) {
                if (m_baseTrackProducers.at(i) != NULL) {
                    source = m_baseTrackProducers.at(i);
                    break;
                }
            }
            if (!source) return NULL;
        }
        prod = cloneProducer(source);
    }
    if (prod) {
        adjustProducerProperties(prod, getId() + "_", false, false);
        if (!m_properties.contains("proxy_out")) {
            // Adjust length in case...
            if (m_properties.contains("duration")) prod->set("length", m_properties.value("duration").toInt());
            if (m_properties.contains("out"))prod->set("out", m_properties.value("out").toInt());
        }
    }
    return prod;
}


Mlt::Producer *DocClipBase::getProducer(int track)
{
    QMutexLocker locker(&m_producerMutex);
    if (track == -1 || (m_clipType != AUDIO && m_clipType != AV && m_clipType != PLAYLIST)) {
        if (m_baseTrackProducers.count() == 0) {
            return NULL;
        }
        for (int i = 0; i < m_baseTrackProducers.count(); i++) {
            if (m_baseTrackProducers.at(i) != NULL) {
                return m_baseTrackProducers.at(i);
            }
        }
        return NULL;
    }
    if (track >= m_baseTrackProducers.count()) {
        while (m_baseTrackProducers.count() - 1 < track) {
            m_baseTrackProducers.append(NULL);
        }
    }
    if (m_baseTrackProducers.at(track) == NULL) {
        int i;
        for (i = 0; i < m_baseTrackProducers.count(); i++)
            if (m_baseTrackProducers.at(i) != NULL) break;

        if (i >= m_baseTrackProducers.count()) {
            // Could not find a valid producer for that clip, check in 
            return NULL;
        }
        Mlt::Producer *prod = cloneProducer(m_baseTrackProducers.at(i));
        adjustProducerProperties(prod, QString(getId() + '_' + QString::number(track)), false, false);
        m_baseTrackProducers[track] = prod;
    }
    return m_baseTrackProducers.at(track);
}


Mlt::Producer *DocClipBase::cloneProducer(Mlt::Producer *source)
{
    Mlt::Producer *result = NULL;
    QString url = QString::fromUtf8(source->get("resource"));
    if (url == "<playlist>" || url == "<tractor>" || url == "<producer>") {
	// Xml producer sometimes loses the correct url
	url = m_properties.value("resource");
    }
    if (m_clipType == SLIDESHOW || KIO::NetAccess::exists(KUrl(url), KIO::NetAccess::SourceSide, 0)) {
        result = new Mlt::Producer(*(source->profile()), url.toUtf8().constData());
    }
    if (result == NULL || !result->is_valid()) {
        // placeholder clip
        QString txt = "+" + i18n("Missing clip") + ".txt";
        char *tmp = qstrdup(txt.toUtf8().constData());
        result = new Mlt::Producer(*source->profile(), tmp);
        delete[] tmp;
        if (result == NULL || !result->is_valid())
            result = new Mlt::Producer(*(source->profile()), "colour:red");
        else {
            result->set("bgcolour", "0xff0000ff");
            result->set("pad", "10");
        }
        return result;
    }
    /*Mlt::Properties src_props(source->get_properties());
    Mlt::Properties props(result->get_properties());
    props.inherit(src_props);*/
    return result;
}

void DocClipBase::setProducerProperty(const char *name, int data)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::setProducerProperty(const char *name, double data)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::setProducerProperty(const char *name, const char *data)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::resetProducerProperty(const char *name)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, (const char*) NULL);
    }
}

const char *DocClipBase::producerProperty(const char *name) const
{
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL) {
            return m_baseTrackProducers.at(i)->get(name);
        }
    }
    return NULL;
}


void DocClipBase::slotRefreshProducer()
{
    if (m_baseTrackProducers.count() == 0) return;
    if (m_clipType == SLIDESHOW) {
        setProducerProperty("ttl", getProperty("ttl").toInt());
        //m_clipProducer->set("id", getProperty("id"));
        if (!getProperty("animation").isEmpty()) {
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(ct);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "affine") == 0) {
                    break;
                } else if (strcmp(filter->get("mlt_service"), "boxblur") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }

            if (!filter || strcmp(filter->get("mlt_service"), "affine")) {
                // filter does not exist, create it.
                Mlt::Filter *filter = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "affine");
                if (filter && filter->is_valid()) {
                    int cycle = getProperty("ttl").toInt();
                    QString geometry = SlideshowClip::animationToGeometry(getProperty("animation"), cycle);
                    if (!geometry.isEmpty()) {
                        if (getProperty("animation").contains("low-pass")) {
                            Mlt::Filter *blur = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "boxblur");
                            if (blur && blur->is_valid())
                                clipService.attach(*blur);
                        }
                        filter->set("transition.geometry", geometry.toUtf8().data());
                        filter->set("transition.cycle", cycle);
                        clipService.attach(*filter);
                    }
                }
            }
        } else {
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(0);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "affine") == 0 || strcmp(filter->get("mlt_service"), "boxblur") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }
        }
        if (getProperty("fade") == "1") {
            // we want a fade filter effect
            kDebug() << "////////////   FADE WANTED";
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(ct);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "luma") == 0) {
                    break;
                }
                ct++;
                filter = clipService.filter(ct);
            }

            if (filter && strcmp(filter->get("mlt_service"), "luma") == 0) {
                filter->set("cycle", getProperty("ttl").toInt());
                filter->set("duration", getProperty("luma_duration").toInt());
                filter->set("luma.resource", getProperty("luma_file").toUtf8().data());
                if (!getProperty("softness").isEmpty()) {
                    int soft = getProperty("softness").toInt();
                    filter->set("luma.softness", (double) soft / 100.0);
                }
            } else {
                // filter does not exist, create it...
                Mlt::Filter *filter = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "luma");
                filter->set("cycle", getProperty("ttl").toInt());
                filter->set("duration", getProperty("luma_duration").toInt());
                filter->set("luma.resource", getProperty("luma_file").toUtf8().data());
                if (!getProperty("softness").isEmpty()) {
                    int soft = getProperty("softness").toInt();
                    filter->set("luma.softness", (double) soft / 100.0);
                }
                clipService.attach(*filter);
            }
        } else {
            kDebug() << "////////////   FADE NOT WANTED!!!";
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(0);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "luma") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }
        }
        if (getProperty("crop") == "1") {
            // we want a center crop filter effect
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(ct);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "crop") == 0) {
                    break;
                }
                ct++;
                filter = clipService.filter(ct);
            }

            if (!filter || strcmp(filter->get("mlt_service"), "crop")) {
                // filter does not exist, create it...
                Mlt::Filter *filter = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "crop");
                filter->set("center", 1);
                clipService.attach(*filter);
            }
        } else {
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(0);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "crop") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }
        }
    }
}

void DocClipBase::setProperties(QMap <QString, QString> properties)
{
    // changing clip type is not allowed
    properties.remove("type");
    QMapIterator<QString, QString> i(properties);
    bool refreshProducer = false;
    QStringList keys;
    keys << "luma_duration" << "luma_file" << "fade" << "ttl" << "softness" << "crop" << "animation";
    QString oldProxy = m_properties.value("proxy");
    while (i.hasNext()) {
        i.next();
        setProperty(i.key(), i.value());
        if (m_clipType == SLIDESHOW && keys.contains(i.key())) refreshProducer = true;
    }
    if (properties.contains("proxy")) {
        QString value = properties.value("proxy");
        // If value is "-", that means user manually disabled proxy on this clip
        if (value.isEmpty() || value == "-") {
            // reset proxy
            emit abortProxy(m_id, oldProxy);
        }
        else {
            emit createProxy(m_id);
        }
    }
    if (refreshProducer) slotRefreshProducer();
}

void DocClipBase::setMetadata(QMap <QString, QString> properties)
{
    QMapIterator<QString, QString> i(properties);
    while (i.hasNext()) {
        i.next();
        if (i.value().isEmpty() && m_metadata.contains(i.key())) {
            m_metadata.remove(i.key());
        } else {
            m_metadata.insert(i.key(), i.value());
        }
    }
}

QMap <QString, QString> DocClipBase::metadata() const
{
    return m_metadata;
}

void DocClipBase::clearProperty(const QString &key)
{
    m_properties.remove(key);
}

void DocClipBase::getFileHash(const QString &url)
{
    if (m_clipType == SLIDESHOW) return;
    QFile file(url);
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        QByteArray fileData;
        QByteArray fileHash;
        //kDebug() << "SETTING HASH of" << value;
        m_properties.insert("file_size", QString::number(file.size()));
        /*
               * 1 MB = 1 second per 450 files (or faster)
               * 10 MB = 9 seconds per 450 files (or faster)
               */
        if (file.size() > 1000000*2) {
            fileData = file.read(1000000);
            if (file.seek(file.size() - 1000000))
                fileData.append(file.readAll());
        } else
            fileData = file.readAll();
        file.close();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        m_properties.insert("file_hash", QString(fileHash.toHex()));
    }
}

bool DocClipBase::checkHash() const
{
    KUrl url = fileURL();
    if (!url.isEmpty() && getClipHash() != getHash(url.path())) return false;
    return true;
}

QString DocClipBase::getClipHash() const
{
    QString hash;
    if (m_clipType == SLIDESHOW) hash = QCryptographicHash::hash(m_properties.value("resource").toAscii().data(), QCryptographicHash::Md5).toHex();
    else if (m_clipType == COLOR) hash = QCryptographicHash::hash(m_properties.value("colour").toAscii().data(), QCryptographicHash::Md5).toHex();
    else if (m_clipType == TEXT) hash = QCryptographicHash::hash(QString("title" + getId() + m_properties.value("xmldata")).toUtf8().data(), QCryptographicHash::Md5).toHex();
    else {
        if (m_properties.contains("file_hash")) hash = m_properties.value("file_hash");
        if (hash.isEmpty()) hash = getHash(fileURL().path());
        
    }
    return hash;
}

void DocClipBase::setPlaceHolder(bool place)
{
    m_placeHolder = place;
}

// static
QString DocClipBase::getHash(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        QByteArray fileData;
        QByteArray fileHash;
        /*
               * 1 MB = 1 second per 450 files (or faster)
               * 10 MB = 9 seconds per 450 files (or faster)
               */
        if (file.size() > 1000000*2) {
            fileData = file.read(1000000);
            if (file.seek(file.size() - 1000000))
                fileData.append(file.readAll());
        } else
            fileData = file.readAll();
        file.close();
        return QCryptographicHash::hash(fileData, QCryptographicHash::Md5).toHex();
    }
    return QString();
}

void DocClipBase::refreshThumbUrl()
{
    if (m_thumbProd) m_thumbProd->updateThumbUrl(m_properties.value("file_hash"));
}

void DocClipBase::setProperty(const QString &key, const QString &value)
{
    m_properties.insert(key, value);
    if (key == "resource") {
        getFileHash(value);
        if (m_thumbProd) m_thumbProd->updateClipUrl(KUrl(value), m_properties.value("file_hash"));
    } else if (key == "out") setDuration(GenTime(value.toInt(), KdenliveSettings::project_fps()));
    //else if (key == "transparency") m_clipProducer->set("transparency", value.toInt());
    else if (key == "colour") {
        setProducerProperty("colour", value.toUtf8().data());
    } else if (key == "templatetext") {
        setProducerProperty("templatetext", value.toUtf8().data());
        setProducerProperty("force_reload", 1);
    } else if (key == "xmldata") {
        setProducerProperty("xmldata", value.toUtf8().data());
        setProducerProperty("force_reload", 1);
    } else if (key == "force_aspect_num") {
        if (value.isEmpty()) {
            m_properties.remove("force_aspect_num");
            resetProducerProperty("force_aspect_ratio");
        } else setProducerProperty("force_aspect_ratio", getPixelAspect(m_properties));
    } else if (key == "force_aspect_den") {
        if (value.isEmpty()) {
            m_properties.remove("force_aspect_den");
            resetProducerProperty("force_aspect_ratio");
        } else setProducerProperty("force_aspect_ratio", getPixelAspect(m_properties));
    } else if (key == "force_fps") {
        if (value.isEmpty()) {
            m_properties.remove("force_fps");
            resetProducerProperty("force_fps");
        } else setProducerProperty("force_fps", value.toDouble());
    } else if (key == "force_progressive") {
        if (value.isEmpty()) {
            m_properties.remove("force_progressive");
            resetProducerProperty("force_progressive");
        } else setProducerProperty("force_progressive", value.toInt());
    } else if (key == "force_tff") {
        if (value.isEmpty()) {
            m_properties.remove("force_tff");
            resetProducerProperty("force_tff");
        } else setProducerProperty("force_tff", value.toInt());
    } else if (key == "threads") {
        if (value.isEmpty()) {
            m_properties.remove("threads");
            setProducerProperty("threads", 1);
        } else setProducerProperty("threads", value.toInt());
    } else if (key == "video_index") {
        if (value.isEmpty()) {
            m_properties.remove("video_index");
            setProducerProperty("video_index", m_properties.value("default_video").toInt());
        } else setProducerProperty("video_index", value.toInt());
    } else if (key == "audio_index") {
        if (value.isEmpty()) {
            m_properties.remove("audio_index");
            setProducerProperty("audio_index", m_properties.value("default_audio").toInt());
        } else setProducerProperty("audio_index", value.toInt());
    } else if (key == "force_colorspace") {
        if (value.isEmpty()) {
            m_properties.remove("force_colorspace");
            resetProducerProperty("force_colorspace");
        } else setProducerProperty("force_colorspace", value.toInt());
    } else if (key == "full_luma") {
        if (value.isEmpty()) {
            m_properties.remove("full_luma");
            resetProducerProperty("set.force_full_luma");
        } else setProducerProperty("set.force_full_luma", value.toInt());
    }
}

QMap <QString, QString> DocClipBase::properties() const
{
    return m_properties;
}

QMap <QString, QString> DocClipBase::currentProperties(QMap <QString, QString> props)
{
    QMap <QString, QString> currentProps;
    QMap<QString, QString>::const_iterator i = props.constBegin();
    while (i != props.constEnd()) {
        currentProps.insert(i.key(), m_properties.value(i.key()));
        ++i;
    }
    return currentProps;
}

bool DocClipBase::getAudioThumbs()
{
    if (m_thumbProd == NULL || isPlaceHolder() || !KdenliveSettings::audiothumbnails()) return false;
    if (m_audioThumbCreated) {
        return false;
    }
    m_audioTimer.start();
    return true;
}

bool DocClipBase::isPlaceHolder() const
{
    return m_placeHolder;
}

void DocClipBase::addCutZone(int in, int out, QString desc)
{
    CutZoneInfo info;
    info.zone = QPoint(in, out);
    info.description = desc;
    for (int i = 0; i < m_cutZones.count(); i++)
        if (m_cutZones.at(i).zone == info.zone) {
            return;
        }
    m_cutZones.append(info);
}

bool DocClipBase::hasCutZone(QPoint p) const
{
    for (int i = 0; i < m_cutZones.count(); i++)
        if (m_cutZones.at(i).zone == p) return true;
    return false;
}


void DocClipBase::removeCutZone(int in, int out)
{
    QPoint p(in, out);
    for (int i = 0; i < m_cutZones.count(); i++) {
        if (m_cutZones.at(i).zone == p) {
            m_cutZones.removeAt(i);
            i--;
        }
    }
}

void DocClipBase::updateCutZone(int oldin, int oldout, int in, int out, QString desc)
{
    QPoint old(oldin, oldout);
    for (int i = 0; i < m_cutZones.size(); ++i) {
        if (m_cutZones.at(i).zone == old) {
            CutZoneInfo info;
            info.zone = QPoint(in, out);
            info.description = desc;
            m_cutZones.replace(i, info);
            break;
        }
    }
}

QList <CutZoneInfo> DocClipBase::cutZones() const
{
    return m_cutZones;
}

bool DocClipBase::hasVideoCodec(const QString &codec) const
{
    Mlt::Producer *prod = NULL;
    if (m_baseTrackProducers.count() == 0) return false;
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL) {
            prod = m_baseTrackProducers.at(i);
            break;
        }
    }

    if (!prod) return false;
    int default_video = prod->get_int("video_index");
    char property[200];
    snprintf(property, sizeof(property), "meta.media.%d.codec.name", default_video);
    return prod->get(property) == codec;
}

bool DocClipBase::hasAudioCodec(const QString &codec) const
{
    Mlt::Producer *prod = NULL;
    if (m_baseTrackProducers.count() == 0) return false;
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL) {
            prod = m_baseTrackProducers.at(i);
            break;
        }
    }
    if (!prod) return false;
    int default_video = prod->get_int("audio_index");
    char property[200];
    snprintf(property, sizeof(property), "meta.media.%d.codec.name", default_video);
    return prod->get(property) == codec;
}


void DocClipBase::slotExtractImage(QList <int> frames)
{
    if (m_thumbProd == NULL) return;
    m_thumbProd->extractImage(frames);
}

QImage DocClipBase::extractImage(int frame, int width, int height)
{
    if (m_thumbProd == NULL) return QImage();
    QMutexLocker locker(&m_producerMutex);
    return m_thumbProd->extractImage(frame, width, height);
}


