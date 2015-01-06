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
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/clippropertiescontroller.h"

#include <QDomElement>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>

#include <KLocalizedString>



ProjectClip::ProjectClip(const QString &id, ClipController *controller, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, id, parent)
    , m_controller(controller)
    , audioFrameCache()
    , m_audioThumbCreated(false)
{
    m_clipStatus = StatusReady;
    m_name = m_controller->clipName();
    m_duration = m_controller->getStringDuration();
    getFileHash();
    setParent(parent);
}

ProjectClip::ProjectClip(const QDomElement& description, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, description, parent)
    , m_controller(NULL)
    , audioFrameCache()
    , m_audioThumbCreated(false)
{
    Q_ASSERT(description.hasAttribute("id"));
    m_clipStatus = StatusWaiting;
    QString resource = getXmlProperty(description, "resource");
    QString clipName = getXmlProperty(description, "kdenlive.clipname");
    if (!clipName.isEmpty()) {
        m_name = clipName;
    }
    else if (!resource.isEmpty()) {
        m_name = QUrl::fromLocalFile(resource).fileName();
    }
    else m_name = i18n("Untitled");
    if (description.hasAttribute("zone"))
	m_zone = QPoint(description.attribute("zone").section(':', 0, 0).toInt(), description.attribute("zone").section(':', 1, 1).toInt());
    setParent(parent);
}


ProjectClip::~ProjectClip()
{
    // controller is deleted in bincontroller
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

void ProjectClip::updateAudioThumbnail(const audioByteArray& data)
{
    ////qDebug() << "CLIPBASE RECIEDVED AUDIO DATA*********************************************";
    audioFrameCache = data;
    m_audioThumbCreated = true;
    emit gotAudioData();
}

QList < CommentedTime > ProjectClip::commentedSnapMarkers() const
{
    if (m_controller) return m_controller->commentedSnapMarkers();
    return QList < CommentedTime > ();
}

bool ProjectClip::audioThumbCreated() const
{
    return m_audioThumbCreated;
}

ClipType ProjectClip::clipType() const
{
    if (m_controller == NULL) return Unknown;
    return m_controller->clipType();
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

/*bool ProjectClip::isValid() const
{
    return m_controller->isValid();
}*/

QUrl ProjectClip::url() const
{
    return m_controller->clipUrl();
}

bool ProjectClip::hasLimitedDuration() const
{
    if (m_controller) {
        return m_controller->hasLimitedDuration();
    }
    return true;
}

GenTime ProjectClip::duration() const
{
    if (m_controller) {
	return m_controller->getPlaytime();
    }
    return GenTime();
}

QString ProjectClip::serializeClip()
{
    /*Mlt::Consumer *consumer = bin()->project()->xmlConsumer();
    consumer->connect(*m_baseProducer);
    consumer->run();
    return QString::fromUtf8(consumer->get("kdenlive_clip"));*/
    return QString();
}

void ProjectClip::reloadProducer(bool thumbnailOnly)
{
    QDomDocument doc;
    QDomElement xml = toXml(doc);
    if (thumbnailOnly) {
        // set a special flag to request thumbnail only
        xml.setAttribute("thumbnailOnly", "1");
    }
    bin()->reloadProducer(m_id, xml);
}

void ProjectClip::setCurrent(bool current, bool notify)
{
    AbstractProjectItem::setCurrent(current, notify);
    if (current) {
        bin()->openProducer(m_controller);
    }
}

QDomElement ProjectClip::toXml(QDomDocument& document)
{
    if (m_controller) {
        m_controller->getProducerXML(document);
        return document.documentElement().firstChildElement("producer");
    }
    return QDomElement();
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

void ProjectClip::setProducer(ClipController *controller, bool replaceProducer)
{
    if (!replaceProducer && m_controller) {
        qDebug()<<"// RECIEVED PRODUCER BUT WE ALREADY HAVE ONE\n----------";
        return;
    }
    if (m_controller) {
        // Replace clip for this controller
        //m_controller->updateProducer(m_id, &(controller->originalProducer()));
        delete controller;
    }
    else {
        // We did not yet have the controller, update info
        m_controller = controller;
        if (m_name.isEmpty()) m_name = m_controller->clipName();
        m_duration = m_controller->getStringDuration();
    }
    m_clipStatus = StatusReady;
    bin()->emitItemUpdated(this);
    getFileHash();
}

Mlt::Producer *ProjectClip::producer()
{
    if (!m_controller) return NULL;
    return m_controller->masterProducer();
}

bool ProjectClip::isReady() const
{
    return m_controller!= NULL;
}

QMap <QString, QString> ProjectClip::properties()
{
    //TODO: move into its own class in ClipController
    QMap <QString, QString> result;
    /*
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
    */
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
    if (m_controller) {
	m_controller->setProperty(name, data);
    }
}

void ProjectClip::setProducerProperty(const char *name, double data)
{
    if (m_controller) {
        m_controller->setProperty(name, data);
    }
}

void ProjectClip::setProducerProperty(const char *name, const char *data)
{
    if (m_controller) {
        m_controller->setProperty(name, data);
    }
}

int ProjectClip::getProducerIntProperty(const QString &key) const
{
    int value = 0;
    if (m_controller) {
        value = m_controller->int_property(key);
    }
    return value;
}

QString ProjectClip::getProducerProperty(const QString &key) const
{
    QString value;
    if (m_controller) {
	value = m_controller->property(key);
    }
    return value;
}

const QString ProjectClip::hash()
{
    if (!m_controller) return QString();
    QString clipHash = m_controller->property("file_hash");
    if (clipHash.isEmpty()) {
        return getFileHash();
    }
    return clipHash;
}

const QString ProjectClip::getFileHash() const
{
    if (clipType() == SlideShow) return QString();
    QFile file(m_controller->clipUrl().toLocalFile());
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        QByteArray fileData;
        QByteArray fileHash;
        ////qDebug() << "SETTING HASH of" << value;
        //m_properties.insert("file_size", QString::number(file.size()));
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
        QString result = fileHash.toHex();
        m_controller->setProperty("file_hash", result);
        return result;
    }
    return QString();
}

bool ProjectClip::hasProxy() const
{
    QString proxy = getProducerProperty("proxy");
    if (proxy.isEmpty() || proxy == "-") return false;
    return true;
}

void ProjectClip::setProperties(QMap <QString, QString> properties, bool refreshPanel)
{
    QMapIterator<QString, QString> i(properties);
    bool refreshProducer = false;
    QStringList keys;
    keys << "luma_duration" << "luma_file" << "fade" << "ttl" << "softness" << "crop" << "animation";
    while (i.hasNext()) {
        i.next();
        setProducerProperty(i.key().toUtf8().data(), i.value().toUtf8().data());
        if (clipType() == SlideShow && keys.contains(i.key())) refreshProducer = true;
    }
    if (properties.contains("proxy")) {
        QString value = properties.value("proxy");
        // If value is "-", that means user manually disabled proxy on this clip
        if (value.isEmpty() || value == "-") {
            // reset proxy
            if (bin()->hasPendingJob(m_id, AbstractClipJob::PROXYJOB)) {
                bin()->discardJobs(m_id, AbstractClipJob::PROXYJOB);
            }
            else {
                reloadProducer();
            }
        }
        else {
            // A proxy was requested, make sure to keep original url
            setProducerProperty("kdenlive_originalUrl", url().toLocalFile().toUtf8().data());
            bin()->startJob(m_id, AbstractClipJob::PROXYJOB);
        }
    }
    else if (properties.contains("resource")) {
        // Clip resource changed, update thumbnail
        reloadProducer(true);
        refreshProducer = true;
    }
    if (refreshPanel) {
        // Some of the clip properties have changed through a command, update properties panel
        emit refreshPropertiesPanel();
    }
    if (refreshProducer) {
        // producer has changed, refresh monitor
        bin()->refreshMonitor(m_id);
    }
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


ClipPropertiesController *ProjectClip::buildProperties(QWidget *parent)
{
    ClipPropertiesController *panel = new ClipPropertiesController(m_id, clipType(), m_controller->properties(), parent);
    connect(this, SIGNAL(refreshPropertiesPanel()), panel, SLOT(slotReloadProperties()));
    return panel;
}

void ProjectClip::updateParentInfo(const QString &folderid, const QString &foldername)
{
    m_controller->setProperty("kdenlive.groupid", folderid);
    m_controller->setProperty("kdenlive.groupname", foldername);
}

