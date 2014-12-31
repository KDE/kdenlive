/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "projectclip.h"
#include "projectfolder.h"
#include "bin.h"

#include <QDomElement>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>

#include <KLocalizedString>

#include <mlt++/Mlt.h>


ProjectClip::ProjectClip(const QString &id, Mlt::Producer *producer, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, id, parent),
    m_producer(producer)
{
    m_properties = QMap <QString, QString> ();
    QString resource = producer->get("resource");
    if (!resource.isEmpty()) {
        m_url = QUrl::fromLocalFile(resource);
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
    }
    m_duration = producer->get_length_time(mlt_time_smpte_df);

    getFileHash();
    setParent(parent);
}

ProjectClip::ProjectClip(const QDomElement& description, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, description, parent)
    , m_producer(NULL)
{
    Q_ASSERT(description.hasAttribute("id"));
    m_properties = QMap <QString, QString> ();
    m_url = QUrl::fromLocalFile(getXmlProperty(description, "resource"));
    m_name = m_url.fileName();
    if (description.hasAttribute("zone"))
	m_zone = QPoint(description.attribute("zone").section(':', 0, 0).toInt(), description.attribute("zone").section(':', 1, 1).toInt());
    setParent(parent);
}


ProjectClip::~ProjectClip()
{
    // Cancel all clip jobs so that we don't crash trying to access the deleted clip
    bin()->discardJobs(m_id);
    delete m_producer;
}

QString ProjectClip::getXmlProperty(const QDomElement &producer, const QString &propertyName)
{
    QString value;
    QDomNodeList props = producer.elementsByTagName("property");
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().attribute("name") == propertyName) {
            value = props.at(i).firstChild().nodeValue();
            break;
        }
    }
    return value;
}

ClipType ProjectClip::clipType() const
{
    //TODO: store in properties
    if (m_producer == NULL) return Unknown;
    QString service = m_producer->get("mlt_service");
    if (service == "avformat" || service == "avformat-novalidate") {
	return AV;
    }
    if (service == "qimage" || service == "pixbuf") {
	return Image;
    }
    if (service == "color") {
	return Color;
    }
}

ProjectClip* ProjectClip::clip(const QString &id)
{
    if (id == m_id) {
        return this;
    }
    return NULL;
}

ProjectFolder* ProjectClip::folder(const QString &id)
{
    return NULL;
}

ProjectClip* ProjectClip::clipAt(int ix)
{
    if (ix == index()) {
        return this;
    }
    return NULL;
}

bool ProjectClip::hasUrl() const
{
    return m_url.isValid();
}

QUrl ProjectClip::url() const
{
    return m_url;
}

bool ProjectClip::hasLimitedDuration() const
{
    //TODO: should be false for color and image clips
    return m_hasLimitedDuration;
}

int ProjectClip::duration() const
{
    if (m_producer) {
	return m_producer->get_playtime();
    }
    return -1;
}

QString ProjectClip::serializeClip()
{
    /*Mlt::Consumer *consumer = bin()->project()->xmlConsumer();
    consumer->connect(*m_baseProducer);
    consumer->run();
    return QString::fromUtf8(consumer->get("kdenlive_clip"));*/
    return QString();
}

void ProjectClip::reloadProducer()
{
    QDomDocument doc;
    QDomElement xml = toXml(doc);
    bin()->reloadProducer(m_id, doc.documentElement());
}

void ProjectClip::setCurrent(bool current, bool notify)
{
    AbstractProjectItem::setCurrent(current, notify);
    if (current && m_producer) {
        bin()->openProducer(m_id, producer());
    }
}

QDomElement ProjectClip::toXml(QDomDocument& document)
{
    QDomElement prod = document.createElement("producer");
    document.appendChild(prod);
    QDomElement prop = document.createElement("property");
    prop.setAttribute("name", "resource");
    QString path = m_properties.value("proxy");
    if (path.length() < 2) {
        // No proxy
        path = m_url.toLocalFile();
    }
    QDomText value = document.createTextNode(path);
    prop.appendChild(value);
    prod.appendChild(prop);
    prod.setAttribute("id", m_id);
    return prod;
}

QPixmap ProjectClip::thumbnail(bool force)
{
/*    if ((force || m_thumbnail.isNull()) && m_baseProducer) {
	int width = 80 * bin()->project()->displayRatio();
	if (width % 2 == 1) width++;
	bin()->project()->monitorManager()->requestThumbnails(m_id, QList <int>() << 0);
    }
    */
    return m_thumbnail;
}

void ProjectClip::setThumbnail(QImage img)
{
    m_thumbnail = roundedPixmap(QPixmap::fromImage(img));
    if (hasProxy() && !m_thumbnail.isNull()) {
        // Overlay proxy icon
        QPainter p(&m_thumbnail);
        QColor c(220, 220, 10, 200);
        QRect r(0, 0, m_thumbnail.height() / 2.5, m_thumbnail.height() / 2.5);
        p.fillRect(r, c);
        QFont font = p.font();
        font.setPixelSize(r.height());
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(r, Qt::AlignCenter, i18nc("The first letter of Proxy, used as abbreviation", "P"));
    }
    bin()->emitItemUpdated(this);
}

void ProjectClip::setProducer(Mlt::Producer *producer, bool replaceProducer)
{
    if (!replaceProducer && m_producer) {
        qDebug()<<"// RECIEVED PRODUCER BUT WE ALREADY HAVE ONE\n----------";
        return;
    }
    if (m_producer) {
        delete m_producer;
    }
    m_producer = producer;
    m_duration = producer->get_length_time(mlt_time_smpte_df);
    bin()->emitItemUpdated(this);
    getFileHash();
}

Mlt::Producer *ProjectClip::producer()
{
    if (!m_producer) return NULL;
    return new Mlt::Producer(*m_producer);
}

bool ProjectClip::hasProducer() const
{
    return m_producer!= NULL;
}

QMap <QString, QString> ProjectClip::properties()
{
    //TODO: move into its own class that creates its own widget (reuse clipproperties)
    QMap <QString, QString> result;
    if (m_producer) {
        mlt_properties props = m_producer->get_properties();
        QString key = "video_index";
        int video_index = mlt_properties_get_int(props, key.toUtf8().constData());
        QString codecKey = "meta.media." + QString::number(video_index) + ".codec.long_name";
        result.insert(i18n("Video codec"), mlt_properties_get(props, codecKey.toUtf8().constData()));
        key = "audio_index";
        int audio_index = mlt_properties_get_int(props, key.toUtf8().constData());
        codecKey = "meta.media." + QString::number(audio_index) + ".codec.long_name";
        result.insert(i18n("Audio codec"), mlt_properties_get(props, codecKey.toUtf8().constData()));
    }
    return result;
}

void ProjectClip::setZone(const QPoint &zone)
{
    m_zone = zone;
}

QPoint ProjectClip::zone() const
{
    return m_zone;
}

void ProjectClip::addMarker(int position)
{
    m_markers << position;
    //bin()->markersUpdated(m_id, m_markers);
}

void ProjectClip::removeMarker(int position)
{
    m_markers.removeAll(position);
    //bin()->markersUpdated(m_id, m_markers);
}

void ProjectClip::setProducerProperty(const char *name, int data)
{
    if (m_producer) {
	m_producer->set(name, data);
	//TODO: also set property on all track producers
    }
}

void ProjectClip::setProducerProperty(const char *name, double data)
{
    if (m_producer) {
	m_producer->set(name, data);
	//TODO: also set property on all track producers
    }
}

void ProjectClip::setProducerProperty(const char *name, const char *data)
{
    if (m_producer) {
	m_producer->set(name, data);
	//TODO: also set property on all track producers
    }
}


QString ProjectClip::getProducerProperty(const QString &key)
{
    QString value;
    if (m_producer) {
	value = m_producer->get(key.toUtf8().constData());
    }
    return value;
}

const QString ProjectClip::hash()
{
    if (!m_properties.contains("file_hash")) getFileHash();
    return m_properties.value("file_hash");
}

void ProjectClip::getFileHash()
{
    if (clipType() == SlideShow) return;    
    QFile file(m_url.toLocalFile());
    qDebug()<<"// CREATE FILE HASH FOR CLIP";
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        QByteArray fileData;
        QByteArray fileHash;
        ////qDebug() << "SETTING HASH of" << value;
        m_properties.insert("file_size", QString::number(file.size()));
        /*
               * 1 MB = 1 second per 450 files (or faster)
               * 10 MB = 9 seconds per 450 files (or faster)
               */
        if (file.size() > 2000000) {
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

const QString ProjectClip::getProperty(const QString &key) const
{
    return m_properties.value(key);
}

bool ProjectClip::hasProxy() const
{
    QString proxy = m_properties.value("proxy");
    if (proxy.isEmpty() || proxy == "-") return false;
    return true;
}

void ProjectClip::setProperties(QMap <QString, QString> properties)
{
    QMapIterator<QString, QString> i(properties);
    bool refreshProducer = false;
    QStringList keys;
    keys << "luma_duration" << "luma_file" << "fade" << "ttl" << "softness" << "crop" << "animation";
    QString oldProxy = m_properties.value("proxy");
    while (i.hasNext()) {
        i.next();
        setProducerProperty(i.key().toUtf8().data(), i.value().toUtf8().data());
	m_properties.insert(i.key(), i.value());
        if (clipType() == SlideShow && keys.contains(i.key())) refreshProducer = true;
    }
    if (properties.contains("proxy")) {
	qDebug()<<"/// CLIP UPDATE, ASK PROXY";
        QString value = properties.value("proxy");
        // If value is "-", that means user manually disabled proxy on this clip
        if (value.isEmpty() || value == "-") {
            // reset proxy
            if (bin()->hasPendingJob(m_id, AbstractClipJob::PROXYJOB)) {
                bin()->discardJobs(m_id, AbstractClipJob::PROXYJOB);
            }
            else reloadProducer();
        }
        else {
	    bin()->startJob(m_id, AbstractClipJob::PROXYJOB);
        }
    }
    //if (refreshProducer) slotRefreshProducer();
}

void ProjectClip::setJobStatus(AbstractClipJob::JOBTYPE jobType, ClipJobStatus status, int progress, const QString &statusMessage)
{
    m_jobType = jobType;
    if (progress > 0) {
	m_jobProgress = progress;
    }
    else {
	m_jobProgress = status;
	if ((status == JobAborted || status == JobCrashed  || status == JobDone) || !statusMessage.isEmpty()) {
	    m_jobMessage = statusMessage;
	}
    }
    bin()->emitItemUpdated(this);
}
