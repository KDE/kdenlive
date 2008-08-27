/**************************1*************************************************
                          DocClipBase.cpp  -  description
                             -------------------
    begin                : Fri Apr 12 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <KDebug>

#include "kdenlivesettings.h"
#include "docclipbase.h"
#include "kthumb.h"
#include "clipmanager.h"

DocClipBase::DocClipBase(ClipManager *clipManager, QDomElement xml, const QString &id):
        m_id(id), m_description(QString()), m_refcount(0), m_audioThumbCreated(false), m_duration(GenTime()), m_thumbProd(NULL), m_audioTimer(NULL), m_clipProducer(NULL) {
    int type = xml.attribute("type").toInt();
    m_clipType = (CLIPTYPE) type;
    m_name = xml.attribute("name");

    QDomNamedNodeMap attributes = xml.attributes();
    for (unsigned int i = 0; i < attributes.count(); i++) {
        m_properties.insert(attributes.item(i).nodeName(), attributes.item(i).nodeValue());
    }

    KUrl url = KUrl(xml.attribute("resource"));
    int out = xml.attribute("out").toInt();
    if (out != 0) {
        setDuration(GenTime(out, KdenliveSettings::project_fps()));
    } else {
        out = xml.attribute("duration").toInt();
        if (out != 0) setDuration(GenTime(out, KdenliveSettings::project_fps()));
    }
    if (m_name.isEmpty()) m_name = url.fileName();

    if (!url.isEmpty() && QFile::exists(url.path())) {
        m_thumbProd = new KThumb(clipManager, url);
        if (m_clipType == AV || m_clipType == AUDIO) slotCreateAudioTimer();
    }
    //kDebug() << "type is video" << (m_clipType == AV) << " " << m_clipType;
}

/*DocClipBase & DocClipBase::operator=(const DocClipBase & clip) {
    DocClipBase::operator=(clip);
    m_id = clip.getId();
    m_clipType = clip.clipType();
    m_name = clip.name();
    m_duration = clip.duration();
    m_audioThumbCreated = clip.audioThumbCreated();
    m_properties = clip.properties();
    return *this;
}*/

DocClipBase::~DocClipBase() {
    if (m_thumbProd) delete m_thumbProd;
    if (m_clipProducer) delete m_clipProducer;
}

void DocClipBase::slotCreateAudioTimer() {
    connect(m_thumbProd, SIGNAL(audioThumbReady(QMap <int, QMap <int, QByteArray> >)), this , SLOT(updateAudioThumbnail(QMap <int, QMap <int, QByteArray> >)));
    connect(this, SIGNAL(getAudioThumbs()), this , SLOT(slotGetAudioThumbs()));
    m_audioTimer = new QTimer(this);
    connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(slotGetAudioThumbs()));
}

void DocClipBase::slotRequestAudioThumbs() {
    emit getAudioThumbs();
}

void DocClipBase::slotClearAudioCache() {
    if (m_thumbProd) m_thumbProd->stopAudioThumbs();
    if (m_audioTimer != NULL) m_audioTimer->stop();
    audioFrameChache.clear();
    m_audioThumbCreated = false;
}

KThumb *DocClipBase::thumbProducer() {
    return m_thumbProd;
}

bool DocClipBase::audioThumbCreated() const {
    return m_audioThumbCreated;
}

void DocClipBase::setName(const QString name) {
    m_name = name;
}

const QString & DocClipBase::name() const {

    return m_name;
}

const QString &DocClipBase::getId() const {
    return m_id;
}

void DocClipBase::setId(const QString &newId) {
    m_id = newId;
}

const CLIPTYPE & DocClipBase::clipType() const {
    return m_clipType;
}

void DocClipBase::setClipType(CLIPTYPE type) {
    m_clipType = type;
    m_properties.insert("type", QString::number((int) type));
    if (m_thumbProd && m_audioTimer == NULL && (m_clipType == AV || m_clipType == AUDIO))
        slotCreateAudioTimer();
}

KUrl DocClipBase::fileURL() const {
    QString res = m_properties.value("resource");
    if (m_clipType != COLOR && !res.isEmpty()) return KUrl(res);
    return KUrl();
}

void DocClipBase::setClipThumbFrame(const uint &ix) {
    m_properties.insert("thumbnail", QString::number((int) ix));
}

uint DocClipBase::getClipThumbFrame() const {
    return (uint) m_properties.value("thumbnail").toInt();
}

const QString DocClipBase::description() const {
    return m_properties.value("description");
}

bool DocClipBase::isTransparent() const {
    return (m_properties.value("transparency") == "1");
}

const QString DocClipBase::getProperty(const QString prop) const {
    return m_properties.value(prop);
}

void DocClipBase::setDuration(GenTime dur) {
    m_duration = dur;
    m_properties.insert("duration", QString::number((int) dur.frames(KdenliveSettings::project_fps())));
}

const GenTime &DocClipBase::duration() const {
    return m_duration;
}

const GenTime &DocClipBase::maxDuration() const {
    if (m_clipType == COLOR || m_clipType == IMAGE || m_clipType == TEXT || (m_clipType == SLIDESHOW &&  m_properties.value("loop") == "1")) {
        const GenTime dur(10000, KdenliveSettings::project_fps());
        return dur;
    }
    return m_duration;
}

bool DocClipBase::hasFileSize() const {
    return true;
}


// virtual
QDomElement DocClipBase::toXML() const {
    QDomDocument doc;

    QDomElement clip = doc.createElement("producer");

    QMapIterator<QString, QString> i(m_properties);
    while (i.hasNext()) {
        i.next();
        if (!i.value().isEmpty()) clip.setAttribute(i.key(), i.value());
    }
    //doc.appendChild(clip);
    //kDebug()<<"/// CLIP XML: "<<doc.toString();
    return clip;
}

DocClipBase *DocClipBase::
createClip(KdenliveDoc *doc, const QDomElement & element) {
    DocClipBase *clip = 0;
    QString description;
    QDomNode node = element;
    node.normalize();
    if (element.tagName() != "kdenliveclip") {
        kWarning() <<
        "DocClipBase::createClip() element has unknown tagName : " << element.tagName();
        return 0;
    }

    QDomNode n = element.firstChild();

    while (!n.isNull()) {
        QDomElement e = n.toElement();
        if (!e.isNull()) {
            QString tagName = e.tagName();
            if (e.tagName() == "avfile") {
                // clip = DocClipAVFile::createClip(e);
            } else if (e.tagName() == "DocTrackBaseList") {
                // clip = DocClipProject::createClip(doc, e);
            }
        } else {
            QDomText text = n.toText();
            if (!text.isNull()) {
                description = text.nodeValue();
            }
        }

        n = n.nextSibling();
    }
    if (clip == 0) {
        kWarning() << "DocClipBase::createClip() unable to create clip";
    } else {
        // setup DocClipBase specifics of the clip.
        QMap <QString, QString> props;
        props.insert("description", description);
        clip->setProperties(props);
        clip->setAudioThumbCreated(false);
    }
    return clip;
}

void DocClipBase::setAudioThumbCreated(bool isDone) {
    m_audioThumbCreated = isDone;
}


void DocClipBase::setThumbnail(const QPixmap & pixmap) {
    m_thumbnail = pixmap;
}

const QPixmap & DocClipBase::thumbnail() const {
    return m_thumbnail;
}

void DocClipBase::updateAudioThumbnail(QMap<int, QMap<int, QByteArray> > data) {
    kDebug() << "CLIPBASE RECIEDVED AUDIO DATA*********************************************";
    audioFrameChache = data;
    m_audioThumbCreated = true;
    emit gotAudioData();
}

QList < GenTime > DocClipBase::snapMarkers() const {
    QList < GenTime > markers;

    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
        markers.append(m_snapMarkers[count].time());
    }

    return markers;
}

QList < CommentedTime > DocClipBase::commentedSnapMarkers() const {
    return m_snapMarkers;
}

void DocClipBase::setSnapMarkers(QList < CommentedTime > markers) {
    m_snapMarkers = markers;
}

void DocClipBase::addSnapMarker(const GenTime & time, QString comment) {
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

void DocClipBase::editSnapMarker(const GenTime & time, QString comment) {
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

QString DocClipBase::deleteSnapMarker(const GenTime & time) {
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


GenTime DocClipBase::hasSnapMarkers(const GenTime & time) {
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == time)
            return time;
        ++itt;
    }

    return GenTime(0.0);
}

GenTime DocClipBase::findPreviousSnapMarker(const GenTime & currTime) {
    int it;
    for (it = 0; it < m_snapMarkers.count(); it++) {
        if (m_snapMarkers[it].time() >= currTime)
            break;
    }
    if (it == 0) return GenTime();
    else if (it == m_snapMarkers.count() - 1 && m_snapMarkers[it].time() < currTime)
        return m_snapMarkers[it].time();
    else return m_snapMarkers[it-1].time();
}

GenTime DocClipBase::findNextSnapMarker(const GenTime & currTime) {
    int it;
    for (it = 0; it < m_snapMarkers.count(); it++) {
        if (m_snapMarkers[it].time() > currTime)
            break;
    }
    if (it < m_snapMarkers.count() && m_snapMarkers[it].time() > currTime) return m_snapMarkers[it].time();
    return duration();
}

QString DocClipBase::markerComment(GenTime t) {
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == t)
            return (*itt).comment();
        ++itt;
    }
    return QString::null;
}

void DocClipBase::setProducer(Mlt::Producer *producer) {
    m_clipProducer = producer;
    m_clipProducer->set("transparency", m_properties.value("transparency").toInt());
    if (m_thumbProd) m_thumbProd->setProducer(producer);
}

Mlt::Producer *DocClipBase::producer() {
    return m_clipProducer;
}

void DocClipBase::slotRefreshProducer() {
    if (m_clipProducer == NULL) return;
    kDebug() << "////////////   REFRESH CLIP !!!!!!!!!!!!!!!!";
    if (m_clipType == SLIDESHOW) {
        /*char *tmp = (char *) qstrdup(getProperty("resource").toUtf8().data());
               Mlt::Producer producer(*(m_clipProducer->profile()), tmp);
               delete[] tmp;
        delete m_clipProducer;
        m_clipProducer = new Mlt::Producer(producer.get_producer());
        if (!getProperty("out").isEmpty()) m_clipProducer->set_in_and_out(getProperty("in").toInt(), getProperty("out").toInt());*/
        m_clipProducer->set("ttl", getProperty("ttl").toInt());
        //m_clipProducer->set("id", getProperty("id"));
        if (getProperty("fade") == "1") {
            // we want a fade filter effect
            kDebug() << "////////////   FADE WANTED";
            Mlt::Service clipService(m_clipProducer->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(ct);
            while (filter) {
                if (filter->get("mlt_service") == "luma") {
                    break;
                }
                ct++;
                filter = clipService.filter(ct);
            }

            if (filter && filter->get("mlt_service") == "luma") {
                filter->set("period", getProperty("ttl").toInt() - 1);
                filter->set("luma.out", getProperty("luma_duration").toInt());
                QString resource = getProperty("luma_file");
                char *tmp = (char *) qstrdup(resource.toUtf8().data());
                filter->set("luma.resource", tmp);
                delete[] tmp;
                if (getProperty("softness") != QString()) {
                    int soft = getProperty("softness").toInt();
                    filter->set("luma.softness", (double) soft / 100.0);
                }
            } else {
                // filter does not exist, create it...
                Mlt::Filter *filter = new Mlt::Filter(*(m_clipProducer->profile()), "luma");
                filter->set("period", getProperty("ttl").toInt() - 1);
                filter->set("luma.out", getProperty("luma_duration").toInt());
                QString resource = getProperty("luma_file");
                char *tmp = (char *) qstrdup(resource.toUtf8().data());
                filter->set("luma.resource", tmp);
                delete[] tmp;
                if (getProperty("softness") != QString()) {
                    int soft = getProperty("softness").toInt();
                    filter->set("luma.softness", (double) soft / 100.0);
                }
                clipService.attach(*filter);
            }
        } else {
            kDebug() << "////////////   FADE NOT WANTED!!!";
            Mlt::Service clipService(m_clipProducer->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(0);
            while (filter) {
                if (filter->get("mlt_service") == "luma") {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }
        }
    }
}

void DocClipBase::setProperties(QMap <QString, QString> properties) {
    // changing clip type is not allowed
    properties.remove("type");
    QMapIterator<QString, QString> i(properties);
    bool refreshProducer = false;
    QStringList keys;
    keys << "luma_duration" << "luma_file" << "fade" << "ttl" << "softness";
    while (i.hasNext()) {
        i.next();
        m_properties.insert(i.key(), i.value());
        if (i.key() == "resource") m_thumbProd->updateClipUrl(KUrl(i.value()));
        else if (i.key() == "out") setDuration(GenTime(i.value().toInt(), KdenliveSettings::project_fps()));
        else if (m_clipType == SLIDESHOW && keys.contains(i.key())) refreshProducer = true;
        else if (i.key() == "transparency") m_clipProducer->set("transparency", i.value().toInt());
    }
    if (refreshProducer) slotRefreshProducer();
}

void DocClipBase::setProperty(QString key, QString value) {
    m_properties.insert(key, value);
    if (key == "resource") m_thumbProd->updateClipUrl(KUrl(value));
    else if (key == "out") setDuration(GenTime(value.toInt(), KdenliveSettings::project_fps()));
    else if (key == "transparency") m_clipProducer->set("transparency", value.toInt());
}

QMap <QString, QString> DocClipBase::properties() const {
    return m_properties;
}

void DocClipBase::slotGetAudioThumbs() {
    if (m_thumbProd == NULL) return;
    if (m_audioThumbCreated) {
        if (m_audioTimer != NULL)
            m_audioTimer->stop();
    } else {
        if (m_audioTimer != NULL)
            m_audioTimer->start(2000);
        double lengthInFrames = duration().frames(/*framesPerSecond()*/25);
        m_thumbProd->getAudioThumbs(2, 0, lengthInFrames /*must be number of frames*/, 20);
    }
}



