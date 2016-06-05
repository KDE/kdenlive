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
#include "kthumb.h"

#include "kdenlivesettings.h"
#include "project/clipmanager.h"
#include "project/dialogs/slideshowclip.h"

#include <klocalizedstring.h>

#include <QDebug>

#include <QCryptographicHash>

#include <cstdio>
#include <kmessagebox.h>

DocClipBase::DocClipBase(ClipManager *clipManager, QDomElement xml, const QString &id) :
    QObject(),
    lastSeekPosition(0),
    audioFrameCache(),
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
    m_clipType = (ClipType) type;
    if (m_placeHolder) xml.removeAttribute("placeholder");
    QDomNamedNodeMap attributes = xml.attributes();
    for (int i = 0; i < attributes.count(); ++i) {
        QString name = attributes.item(i).nodeName();
        if (name.startsWith(QLatin1String("meta.attr."))) {
            m_metadata.insert(name.section('.', 2), QStringList() << attributes.item(i).nodeValue());
        } else m_properties.insert(name, attributes.item(i).nodeValue());
    }
    QDomNodeList metas = xml.elementsByTagName("metaproperty");
    for (int i = 0; i < metas.count(); ++i) {
        QDomElement e = metas.item(i).toElement();
        if (!e.isNull()) {
            m_metadata.insert(e.attribute("name").section('.', 2), QStringList() << e.firstChild().nodeValue() << e.attribute("tool"));
        }
    }
    if (xml.hasAttribute("cutzones")) {
        QStringList cuts = xml.attribute("cutzones").split(';', QString::SkipEmptyParts);
        for (int i = 0; i < cuts.count(); ++i) {
            QString z = cuts.at(i);
            addCutZone(z.section('-', 0, 0).toInt(), z.section('-', 1, 1).toInt(), z.section('-', 2, 2));
        }
    }

    if (xml.hasAttribute("analysisdata")) {
        QStringList adata = xml.attribute("analysisdata").split('#', QString::SkipEmptyParts);
        for (int i = 0; i < adata.count(); ++i)
            m_analysisdata.insert(adata.at(i).section('?', 0, 0), adata.at(i).section('?', 1, 1));
    }

    QUrl url = QUrl::fromLocalFile(xml.attribute("resource"));
    if (!m_properties.contains("kdenlive:file_hash") && url.isValid()) getFileHash(url.toLocalFile());

    if (xml.hasAttribute("duration")) {
        setDuration(GenTime(xml.attribute("duration").toInt(), KdenliveSettings::project_fps()));
    } else {
        int out = xml.attribute("out").toInt();
        int in = xml.attribute("in").toInt();
        if (out > in) setDuration(GenTime(out - in + 1, KdenliveSettings::project_fps()));
    }

    if (!m_properties.contains("name")) m_properties.insert("name", url.fileName());

    m_thumbProd = new KThumb(clipManager, url, m_id, m_properties.value("kdenlive:file_hash"));
    
    // Setup timer to trigger audio thumbs creation
    m_audioTimer.setSingleShot(true);
    m_audioTimer.setInterval(800);
    connect(this, SIGNAL(getAudioThumbs()), this, SLOT(slotGetAudioThumbs()));
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

void DocClipBase::setZone(const QPoint &zone)
{
    m_properties.insert("zone_in", QString::number(zone.x()));
    m_properties.insert("zone_out", QString::number(zone.y()));
}

QPoint DocClipBase::zone() const
{
    QPoint zone(m_properties.value("zone_in", "0").toInt(), m_properties.value("zone_out", "50").toInt());
    return zone;
}

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

const ClipType & DocClipBase::clipType() const
{
    return m_clipType;
}

void DocClipBase::setClipType(ClipType type)
{
    m_clipType = type;
    m_properties.insert("type", QString::number((int) type));
}

QUrl DocClipBase::fileURL() const
{
    QString res = m_properties.value("resource");
    if (m_clipType != Color && m_clipType != QText && !res.isEmpty()) return QUrl::fromLocalFile(res);
    return QUrl();
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

void DocClipBase::setDuration(const GenTime &dur)
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
    if (m_clipType == Color || m_clipType == Image || m_clipType == Text || m_clipType == QText || (m_clipType == SlideShow &&  m_properties.value("loop") == "1")) {
        /*const GenTime dur(15000, KdenliveSettings::project_fps());
        return dur;*/
        return GenTime();
    }
    return m_duration;
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

    QMapIterator<QString, QStringList> j(m_metadata);
    // Metadata name can have special chars so we cannot pass it as simple attribute
    while (j.hasNext()) {
        j.next();
        if (!j.value().isEmpty()) {
            QDomElement property = doc.createElement("metaproperty");
            property.setAttribute("name", "meta.attr." + j.key());
            QStringList values = j.value();
            QDomText value = doc.createTextNode(values.at(0));
            if (values.count() > 1) property.setAttribute("tool", values.at(1));
            property.appendChild(value);
            clip.appendChild(property);
        }
    }
    doc.appendChild(clip);
    if (!m_cutZones.isEmpty()) {
        QStringList cuts;
        for (int i = 0; i < m_cutZones.size(); ++i) {
            CutZoneInfo info = m_cutZones.at(i);
            cuts << QString::number(info.zone.x()) + '-' + QString::number(info.zone.y()) + '-' + info.description;
        }
        clip.setAttribute("cutzones", cuts.join(";"));
    }
    QString adata;
    if (!m_analysisdata.isEmpty()) {
        QMapIterator<QString, QString> i(m_analysisdata);
        while (i.hasNext()) {
            i.next();
            //WARNING: a ? and # separator is not a good idea
            adata.append(i.key() + '?' + i.value() + '#');
        }
    }
    clip.setAttribute("analysisdata", adata);
    ////qDebug() << "/// CLIP XML: " << doc.toString();
    return doc.documentElement();
}

void DocClipBase::updateAudioThumbnail(const audioByteArray& data)
{
    ////qDebug() << "CLIPBASE RECIEDVED AUDIO DATA*********************************************";
    audioFrameCache = data;
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


void DocClipBase::addSnapMarker(const CommentedTime &marker)
{
    QList < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() >= marker.time())
            break;
    }

    if ((it != m_snapMarkers.end()) && ((*it).time() == marker.time())) {
        (*it).setComment(marker.comment());
        (*it).setMarkerType(marker.markerType());
        //qCritical() << "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo";
    } else {
        m_snapMarkers.insert(it, marker);
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


QString DocClipBase::markerComment(const GenTime &t) const
{
    QList < CommentedTime >::ConstIterator itt = m_snapMarkers.begin();
    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == t)
            return (*itt).comment();
        ++itt;
    }
    return QString();
}

CommentedTime DocClipBase::markerAt(const GenTime &t) const
{
    QList < CommentedTime >::ConstIterator itt = m_snapMarkers.begin();
    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == t)
            return (*itt);
        ++itt;
    }
    return CommentedTime();
}

bool DocClipBase::isClean() const
{
    return m_toDeleteProducers.isEmpty();
}

void DocClipBase::setProducer(Mlt::Producer &producer, bool reset, bool readPropertiesFromProducer)
{
    setDuration(GenTime(producer.get_length(), KdenliveSettings::project_fps()));
    return;
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
        for (i = 0; i < m_audioTrackProducers.count(); ++i)
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

Mlt::Producer *DocClipBase::getProducer(int track)
{
    QMutexLocker locker(&m_producerMutex);

    if (track == -1 || (m_clipType != Audio && m_clipType != AV && m_clipType != Playlist)) {
        if (m_baseTrackProducers.count() == 0) {
            return NULL;
        }
        for (int i = 0; i < m_baseTrackProducers.count(); ++i) {
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
        for (i = 0; i < m_baseTrackProducers.count(); ++i)
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
    if (m_clipType == SlideShow || QFile::exists(url)) {
        QString xml = getProducerXML(*source);
        result = new Mlt::Producer(*(source->profile()), "xml-string", xml.toUtf8().constData());
    }
    if (result == NULL || !result->is_valid()) {

        // placeholder clip
        QString txt = '+' + i18n("Missing clip") + ".txt";
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

QString DocClipBase::getProducerXML(Mlt::Producer &producer)
{
    QString filename = "string";
    Mlt::Consumer c(*(producer.profile()), "xml", filename.toUtf8().constData());
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

void DocClipBase::setProducerProperty(const char *name, int data)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); ++i) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::setProducerProperty(const char *name, double data)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); ++i) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::setProducerProperty(const char *name, const char *data)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); ++i) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::resetProducerProperty(const char *name)
{
    QMutexLocker locker(&m_producerMutex);
    for (int i = 0; i < m_baseTrackProducers.count(); ++i) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, (const char*) NULL);
    }
}

void DocClipBase::slotRefreshProducer()
{
    if (m_baseTrackProducers.count() == 0) return;
    if (m_clipType == SlideShow) {
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
            //qDebug() << "////////////   FADE WANTED";
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
            //qDebug() << "////////////   FADE NOT WANTED!!!";
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
        if (m_clipType == SlideShow && keys.contains(i.key())) refreshProducer = true;
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

void DocClipBase::setMetadata(const QMap <QString, QString> &properties, const QString &tool)
{
    QMapIterator<QString, QString> i(properties);
    while (i.hasNext()) {
        i.next();
        if (i.value().isEmpty() && m_metadata.contains(i.key())) {
            m_metadata.remove(i.key());
        } else {
            m_metadata.insert(i.key(), QStringList() << i.value() << tool);
        }
    }
}

QMap <QString, QStringList> DocClipBase::metadata() const
{
    return m_metadata;
}

void DocClipBase::getFileHash(const QString &url)
{
    if (m_clipType == SlideShow) return;
    QFile file(url);
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        QByteArray fileData;
        QByteArray fileHash;
        ////qDebug() << "SETTING HASH of" << value;
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
        m_properties.insert("kdenlive:file_hash", QString(fileHash.toHex()));
    }
}

QString DocClipBase::getClipHash() const
{
    QString hash;
    if (m_clipType == SlideShow) hash = QCryptographicHash::hash(m_properties.value("resource").toLatin1().data(), QCryptographicHash::Md5).toHex();
    else if (m_clipType == Color) hash = QCryptographicHash::hash(m_properties.value("colour").toLatin1().data(), QCryptographicHash::Md5).toHex();
    else if (m_clipType == Text) hash = QCryptographicHash::hash(QString("title" + getId() + m_properties.value("xmldata")).toUtf8().data(), QCryptographicHash::Md5).toHex();
    else if (m_clipType == QText) hash = QCryptographicHash::hash(m_properties.value("text").toUtf8().data(), QCryptographicHash::Md5).toHex();
    else {
        if (m_properties.contains("kdenlive:file_hash")) hash = m_properties.value("kdenlive:file_hash");
        if (hash.isEmpty()) hash = getHash(fileURL().path());
        
    }
    return hash;
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

void DocClipBase::setProperty(const QString &key, const QString &value)
{
    m_properties.insert(key, value);
    if (key == "resource") {
        getFileHash(value);
        //else if (key == "transparency") m_clipProducer->set("transparency", value.toInt());
    } else if (key == "out") {
        setDuration(GenTime(value.toInt() + 1, KdenliveSettings::project_fps()));
    }
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

QMap <QString, QString> DocClipBase::currentProperties(const QMap <QString, QString> &props)
{
    QMap <QString, QString> currentProps;
    QMap<QString, QString>::const_iterator i = props.constBegin();
    while (i != props.constEnd()) {
        currentProps.insert(i.key(), m_properties.value(i.key()));
        ++i;
    }
    return currentProps;
}

void DocClipBase::slotGetAudioThumbs()
{
    if (m_thumbProd == NULL || isPlaceHolder() || !KdenliveSettings::audiothumbnails()) return;
    if (m_audioThumbCreated) {
        return;
    }
    m_audioTimer.start();
    return;
}

bool DocClipBase::isPlaceHolder() const
{
    return m_placeHolder;
}

void DocClipBase::addCutZone(int in, int out, const QString &desc)
{
    CutZoneInfo info;
    info.zone = QPoint(in, out);
    info.description = desc;
    for (int i = 0; i < m_cutZones.count(); ++i) {
        if (m_cutZones.at(i).zone == info.zone) {
            return;
        }
    }
    m_cutZones.append(info);
}

void DocClipBase::slotExtractImage(const QList <int> &frames)
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

const QString DocClipBase::geometryWithOffset(const QString &data, int offset)
{
    if (offset == 0) return data;
    Mlt::Profile *profile = m_baseTrackProducers.at(0)->profile();
    Mlt::Geometry geometry(data.toUtf8().data(), m_properties.value("duration").toInt(), profile->width(), profile->height());
    Mlt::Geometry newgeometry(NULL, m_properties.value("duration").toInt(), profile->width(), profile->height());
    Mlt::GeometryItem item;
    int pos = 0;
    while (!geometry.next_key(&item, pos)) {
        pos = item.frame();
        item.frame(pos + offset);
        pos++;
        newgeometry.insert(item);
    }
    return newgeometry.serialise();
}

QMap <QString, QString> DocClipBase::analysisData() const
{
    return m_analysisdata;
}
