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
#include "projectsubclip.h"
#include "bin.h"
#include "timecode.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "project/projectcommands.h"
#include "mltcontroller/clipcontroller.h"
#include "lib/audio/audioStreamInfo.h"
#include "mltcontroller/clippropertiescontroller.h"

#include <QDomElement>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <KLocalizedString>
#include <KMessageBox>


ProjectClip::ProjectClip(const QString &id, QIcon thumb, ClipController *controller, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, id, parent)
    , audioFrameCache()
    , m_controller(controller)
    , m_gpuProducer(NULL)
    , m_abortAudioThumb(false)
{
    m_clipStatus = StatusReady;
    m_thumbnail = thumb;
    m_name = m_controller->clipName();
    m_duration = m_controller->getStringDuration();
    m_date = m_controller->date;
    m_description = m_controller->description();
    m_type = m_controller->clipType();
    getFileHash();
    setParent(parent);
    bin()->loadSubClips(id, m_controller->getPropertiesFromPrefix("kdenlive:clipzone."));
    if (KdenliveSettings::audiothumbnails()) {
        m_audioThumbsThread = QtConcurrent::run(this, &ProjectClip::slotCreateAudioThumbs);
    }
}

ProjectClip::ProjectClip(const QDomElement& description, QIcon thumb, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, description, parent)
    , audioFrameCache()
    , m_controller(NULL)
    , m_gpuProducer(NULL)
    , m_abortAudioThumb(false)
    , m_type(Unknown)
{
    Q_ASSERT(description.hasAttribute("id"));
    m_clipStatus = StatusWaiting;
    m_thumbnail = thumb;
    if (description.hasAttribute("type")) {
        m_type = (ClipType) description.attribute("type").toInt();
    }
    m_temporaryUrl = QUrl::fromLocalFile(getXmlProperty(description, "resource"));
    QString clipName = getXmlProperty(description, "kdenlive:clipname");
    if (!clipName.isEmpty()) {
        m_name = clipName;
    }
    else if (m_temporaryUrl.isValid()) {
        m_name = m_temporaryUrl.fileName();
    }
    else m_name = i18n("Untitled");
    setParent(parent);
}


ProjectClip::~ProjectClip()
{
    // controller is deleted in bincontroller
    abortAudioThumbs();
}

QString ProjectClip::getToolTip() const
{
    return url().toLocalFile();
}

QString ProjectClip::getXmlProperty(const QDomElement &producer, const QString &propertyName, const QString &defaultValue)
{
    QString value = defaultValue;
    QDomNodeList props = producer.elementsByTagName("property");
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().attribute("name") == propertyName) {
            value = props.at(i).firstChild().nodeValue();
            break;
        }
    }
    return value;
}

void ProjectClip::updateAudioThumbnail(QVariantList* audioLevels)
{
    ////qDebug() << "CLIPBASE RECIEDVED AUDIO DATA*********************************************";
    audioFrameCache = audioLevels;
    m_controller->audioThumbCreated = true;
    emit gotAudioData();
}

QList < CommentedTime > ProjectClip::commentedSnapMarkers() const
{
    if (m_controller) return m_controller->commentedSnapMarkers();
    return QList < CommentedTime > ();
}

bool ProjectClip::audioThumbCreated() const
{
    return (m_controller && m_controller->audioThumbCreated);
}

ClipType ProjectClip::clipType() const
{
    return m_type;
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

ProjectSubClip* ProjectClip::getSubClip(int in, int out)
{
    ProjectSubClip *clip;
    for (int i = 0; i < count(); ++i) {
        clip = static_cast<ProjectSubClip *>(at(i))->subClip(in, out);
        if (clip) {
            return clip;
        }
    }
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
    if (m_controller) return m_controller->clipUrl();
    return m_temporaryUrl;
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
    //AbstractProjectItem::setCurrent(current, notify);
    if (current && m_controller) {
        bin()->openProducer(m_controller);
        bin()->editMasterEffect(m_controller);
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

void ProjectClip::setThumbnail(QImage img)
{
    QPixmap thumb = roundedPixmap(QPixmap::fromImage(img));
    if (hasProxy() && !thumb.isNull()) {
        // Overlay proxy icon
        QPainter p(&thumb);
        QColor c(220, 220, 10, 200);
        QRect r(0, 0, thumb.height() / 2.5, thumb.height() / 2.5);
        p.fillRect(r, c);
        QFont font = p.font();
        font.setPixelSize(r.height());
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(r, Qt::AlignCenter, i18nc("The first letter of Proxy, used as abbreviation", "P"));
    }
    m_thumbnail = QIcon(thumb);
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
        m_date = m_controller->date;
        m_description = m_controller->description();
        m_temporaryUrl.clear();
        m_type = m_controller->clipType();
    }
    m_clipStatus = StatusReady;
    bin()->emitItemUpdated(this);
    getFileHash();
    createAudioThumbs();
}

void ProjectClip::createAudioThumbs()
{
    if (KdenliveSettings::audiothumbnails() && !m_audioThumbsThread.isRunning()) {
        m_audioThumbsThread = QtConcurrent::run(this, &ProjectClip::slotCreateAudioThumbs);
    }
}

void ProjectClip::abortAudioThumbs()
{
    if (m_audioThumbsThread.isRunning()) {
        m_abortAudioThumb = true;
        m_audioThumbsThread.waitForFinished();
    }
}

Mlt::Producer *ProjectClip::producer()
{
    if (!m_controller) return NULL;
    return m_controller->masterProducer();
}

ClipController *ProjectClip::controller()
{
    return m_controller;
}

bool ProjectClip::isReady() const
{
    return m_controller != NULL && m_clipStatus == StatusReady;
}

/*void ProjectClip::setZone(const QPoint &zone)
{
    m_zone = zone;
}*/

QPoint ProjectClip::zone() const
{
    int x = getProducerIntProperty("kdenlive:zone_in");
    int y = getProducerIntProperty("kdenlive:zone_out");
    return QPoint(x, y);
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

void ProjectClip::resetProducerProperty(const QString &name)
{
    if (m_controller) {
        m_controller->resetProperty(name);
    }
}

void ProjectClip::setProducerProperty(const QString &name, int data)
{
    if (m_controller) {
	m_controller->setProperty(name, data);
    }
}

void ProjectClip::setProducerProperty(const QString &name, double data)
{
    if (m_controller) {
        m_controller->setProperty(name, data);
    }
}

void ProjectClip::setProducerProperty(const QString &name, const QString &data)
{
    if (m_controller) {
        m_controller->setProperty(name, data);
    }
}

QMap <QString, QString> ProjectClip::currentProperties(const QMap <QString, QString> &props)
{
    QMap <QString, QString> currentProps;
    if (!m_controller) {
        return currentProps;
    }
    QMap<QString, QString>::const_iterator i = props.constBegin();
    while (i != props.constEnd()) {
        currentProps.insert(i.key(), m_controller->property(i.key()));
        ++i;
    }
    return currentProps;
}

QColor ProjectClip::getProducerColorProperty(const QString &key) const
{
    if (m_controller) {
        return m_controller->color_property(key);
    }
    return QColor();
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
    if (m_controller) {
        QString clipHash = m_controller->property("kdenlive:file_hash");
        if (!clipHash.isEmpty()) {
            return clipHash;
        }
    }
    return getFileHash();
}

const QString ProjectClip::getFileHash() const
{
    QByteArray fileData;
    QByteArray fileHash;
    switch (m_type) {
      case SlideShow:
          fileData = m_controller ? m_controller->clipUrl().toLocalFile().toUtf8() : m_temporaryUrl.toLocalFile().toUtf8();
          fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          break;
      case Text:
          fileData = m_controller ? m_controller->property("xmldata").toUtf8() : name().toUtf8();
          fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          break;
      case Color:
          fileData = m_controller ? m_controller->property("resource").toUtf8() : name().toUtf8();
          fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          break;
      default:
          QFile file(m_controller ? m_controller->clipUrl().toLocalFile() : m_temporaryUrl.toLocalFile());
          if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
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
            if (m_controller) m_controller->setProperty("kdenlive:file_size", QString::number(file.size()));
            fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          }
          break;
    }
    if (fileHash.isEmpty()) return QString();
    QString result = fileHash.toHex();
    if (m_controller) {
	m_controller->setProperty("kdenlive:file_hash", result);
    }
    return result;
}

double ProjectClip::getOriginalFps() const
{
    if (!m_controller) return 0;
    return m_controller->originalFps();
}

bool ProjectClip::hasProxy() const
{
    QString proxy = getProducerProperty("kdenlive:proxy");
    if (proxy.isEmpty() || proxy == "-") return false;
    return true;
}

void ProjectClip::setProperties(QMap <QString, QString> properties, bool refreshPanel)
{
    QMapIterator<QString, QString> i(properties);
    QMap <QString, QString> passProperties;
    bool refreshProducer = false;
    bool refreshAnalysis = false;
    bool reload = false;
    // Some properties also need to be passed to track producers
    QStringList timelineProperties;
    timelineProperties << "force_aspect_ratio" << "video_index" << "audio_index" << "full_luma" <<"threads" <<"force_colorspace"<<"force_tff"<<"force_progressive"<<"force_fps";
    QStringList keys;
    keys << "luma_duration" << "luma_file" << "fade" << "ttl" << "softness" << "crop" << "animation";
    while (i.hasNext()) {
        i.next();
        setProducerProperty(i.key(), i.value());
        if (m_type == SlideShow && keys.contains(i.key())) {
            reload = true;
        }
        if (i.key().startsWith("kdenlive:clipanalysis")) refreshAnalysis = true;
        if (timelineProperties.contains(i.key())) {
            passProperties.insert(i.key(), i.value());
        }
    }
    if (properties.contains("kdenlive:proxy")) {
        QString value = properties.value("kdenlive:proxy");
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
            setProducerProperty("kdenlive:originalurl", url().toLocalFile());
            bin()->startJob(m_id, AbstractClipJob::PROXYJOB);
        }
    }
    else if (properties.contains("resource")) {
        // Clip resource changed, update thumbnail
        if (m_type != Color) {
            reloadProducer();
        }
        reload = true;
    }
    if (properties.contains("xmldata") || !passProperties.isEmpty()) {
        refreshProducer = true;
    }
    if (refreshAnalysis) emit refreshAnalysisPanel();
    if (properties.contains("length")) {
        m_duration = m_controller->getStringDuration();
        bin()->emitItemUpdated(this);
    }

    if (properties.contains("kdenlive:clipname")) {
        m_name = properties.value("kdenlive:clipname");
        bin()->emitItemUpdated(this);
    }
    if (refreshPanel) {
        // Some of the clip properties have changed through a command, update properties panel
        emit refreshPropertiesPanel();
    }
    if (refreshProducer || reload) {
        // producer has changed, refresh monitor and thumbnail
        if (reload) reloadProducer(true);
        bin()->refreshClip(m_id);
    }
    if (!passProperties.isEmpty()) {
        bin()->updateTimelineProducers(m_id, passProperties);
    }
}

void ProjectClip::setJobStatus(AbstractClipJob::JOBTYPE jobType, ClipJobStatus status, int progress, const QString &statusMessage)
{
    m_jobType = jobType;
    if (progress > 0) {
        if (m_jobProgress == progress) return;
	m_jobProgress = progress;
    }
    else {
	m_jobProgress = status;
	if ((status == JobAborted || status == JobCrashed  || status == JobDone) || !statusMessage.isEmpty()) {
	    m_jobMessage = statusMessage;
            bin()->emitMessage(statusMessage, OperationCompletedMessage);
	}
    }
    bin()->emitItemUpdated(this);
}


ClipPropertiesController *ProjectClip::buildProperties(QWidget *parent)
{
    ClipPropertiesController *panel = new ClipPropertiesController(bin()->projectTimecode(), m_controller, parent);
    connect(this, SIGNAL(refreshPropertiesPanel()), panel, SLOT(slotReloadProperties()));
    connect(this, SIGNAL(refreshAnalysisPanel()), panel, SLOT(slotFillAnalysisData()));
    return panel;
}

void ProjectClip::updateParentInfo(const QString &folderid, const QString &foldername)
{
    m_controller->setProperty("kdenlive:folderid", folderid);
}

bool ProjectClip::matches(QString condition)
{
    //TODO
    return true;
}

const QString ProjectClip::codec(bool audioCodec) const
{
    if (!m_controller) return QString();
    return m_controller->codec(audioCodec);
}

bool ProjectClip::rename(const QString &name, int column)
{
    QMap <QString, QString> newProperites;
    QMap <QString, QString> oldProperites;
    bool edited = false;
    switch (column) {
      case 0:
        if (m_name == name) return false;
        // Rename clip
        oldProperites.insert("kdenlive:clipname", m_name);
        newProperites.insert("kdenlive:clipname", name);
        m_name = name;
        edited = true;
        break;
      case 2:
        if (m_description == name) return false;
        // Rename clip
        oldProperites.insert("kdenlive:description", m_description);
        newProperites.insert("kdenlive:description", name);
        m_description = name;
        edited = true;
        break;
    }
    if (edited) {
        bin()->slotEditClipCommand(m_id, oldProperites, newProperites);
    }
    return edited;
}

void ProjectClip::addClipMarker(QList <CommentedTime> newMarkers, QUndoCommand *groupCommand)
{
    if (!m_controller) return;
    QList <CommentedTime> oldMarkers;
    for (int i = 0; i < newMarkers.count(); ++i) {
        CommentedTime oldMarker = m_controller->markerAt(newMarkers.at(i).time());
        if (oldMarker == CommentedTime()) {
            oldMarker = newMarkers.at(i);
            oldMarker.setMarkerType(-1);
        }
        oldMarkers << oldMarker;
    }
    (void) new AddMarkerCommand(this, oldMarkers, newMarkers, groupCommand);
}

bool ProjectClip::deleteClipMarkers(QUndoCommand *command)
{
    QList <CommentedTime> markers = commentedSnapMarkers();
    if (markers.isEmpty()) {
        return false;
    }
    QList <CommentedTime> newMarkers;
    for (int i = 0; i < markers.size(); ++i) {
        CommentedTime marker = markers.at(i);
        marker.setMarkerType(-1);
        newMarkers << marker;
    }
    new AddMarkerCommand(this, markers, newMarkers, command);
    return true;
}

void ProjectClip::addMarkers(QList <CommentedTime> &markers)
{
    if (!m_controller) return;
    for (int i = 0; i < markers.count(); ++i) {
      if (markers.at(i).markerType() < 0) m_controller->deleteSnapMarker(markers.at(i).time());
      else m_controller->addSnapMarker(markers.at(i));
    }
    // refresh markers in clip monitor
    bin()->refreshClipMarkers(m_id);
    // refresh markers in timeline clips
    emit refreshClipDisplay();
}

void ProjectClip::addEffect(const ProfileInfo pInfo, QDomElement &effect)
{
    m_controller->addEffect(pInfo, effect);
    bin()->editMasterEffect(m_controller);
    bin()->emitItemUpdated(this);
}

void ProjectClip::removeEffect(const ProfileInfo pInfo, int ix)
{
    m_controller->removeEffect(pInfo, ix);
    bin()->editMasterEffect(m_controller);
    bin()->emitItemUpdated(this);
}

QVariant ProjectClip::data(DataType type) const
{
    switch (type) {
      case AbstractProjectItem::IconOverlay:
            return m_controller != NULL ? (m_controller->hasEffects() ? QVariant("kdenlive-track_has_effect") : QVariant()) : QVariant();
            break;
        default:
	    break;
    }
    return AbstractProjectItem::data(type);
}

void ProjectClip::slotExtractImage(QList <int> frames)
{
    Mlt::Producer *prod = producer();
    if (prod == NULL) return;
    // Check if we are using GPU accel, then we need to use alternate producer
    if (KdenliveSettings::gpu_accel()) {
	if (m_gpuProducer == NULL) {
            QString service = prod->get("mlt_service");
            m_gpuProducer = new Mlt::Producer(*prod->profile(), service.toUtf8().constData(), prod->get("resource"));
            Mlt::Filter scaler(*prod->profile(), "swscale");
	    Mlt::Filter converter(*prod->profile(), "avcolor_space");
	    m_gpuProducer->attach(scaler);
	    m_gpuProducer->attach(converter);
        }
	prod = m_gpuProducer;
    }
    int fullWidth = (int)((double) 150 * prod->profile()->dar() + 0.5);
    QDir thumbFolder(bin()->projectFolder().path() + "/thumbs/");
    for (int i = 0; i < frames.count(); i++) {
        int pos = frames.at(i);
        if (thumbFolder.exists(hash() + '#' + QString::number(pos) + ".png")) {
            emit thumbReady(pos, QImage(thumbFolder.absoluteFilePath(hash() + '#' + QString::number(pos) + ".png")));
            continue;
        }
	int max = prod->get_out();
	if (pos >= max) pos = max - 1;
	prod->seek(pos);
	Mlt::Frame *frame = prod->get_frame();
	if (frame && frame->is_valid()) {
            QImage img = KThumb::getFrame(frame, fullWidth, 150);
            emit thumbReady(frames.at(i), img);
        }
        delete frame;
    }
}

void ProjectClip::slotExtractSubImage(QList <int> frames)
{
    Mlt::Producer *prod = producer();
    if (prod == NULL) return;
    // Check if we are using GPU accel, then we need to use alternate producer
    if (KdenliveSettings::gpu_accel()) {
        if (m_gpuProducer == NULL) {
            QString service = prod->get("mlt_service");
            m_gpuProducer = new Mlt::Producer(*prod->profile(), service.toUtf8().constData(), prod->get("resource"));
            Mlt::Filter scaler(*prod->profile(), "swscale");
            Mlt::Filter converter(*prod->profile(), "avcolor_space");
            m_gpuProducer->attach(scaler);
            m_gpuProducer->attach(converter);
        }
        prod = m_gpuProducer;
    }
    int fullWidth = (int)((double) 150 * prod->profile()->dar() + 0.5);
    QDir thumbFolder(bin()->projectFolder().path() + "/thumbs/");
    for (int i = 0; i < frames.count(); i++) {
        int pos = frames.at(i);
        QString path = thumbFolder.absoluteFilePath(hash() + "#" + QString::number(pos) + ".png");
        QImage img(path);
        if (!img.isNull()) {
            ProjectSubClip *clip;
            for (int i = 0; i < count(); ++i) {
                clip = static_cast<ProjectSubClip *>(at(i));
                if (clip && clip->zone().x() == pos) {
                    clip->setThumbnail(img);
                }
            }
            continue;
        }
        int max = prod->get_out();
        if (pos >= max) pos = max - 1;
        if (pos < 0) pos = 0;
        prod->seek(pos);
        Mlt::Frame *frame = prod->get_frame();
        if (frame && frame->is_valid()) {
            QImage img = KThumb::getFrame(frame, fullWidth, 150);
            if (!img.isNull()) {
                img.save(path);
                ProjectSubClip *clip;
                for (int i = 0; i < count(); ++i) {
                    clip = static_cast<ProjectSubClip *>(at(i));
                    if (clip && clip->zone().x() == pos) {
                        clip->setThumbnail(img);
                    }
                }
            }
        }
        delete frame;
    }
}

int ProjectClip::audioChannels() const
{
    if (!m_controller || !m_controller->audioInfo()) return 0;
    return m_controller->audioInfo()->channels();
}

void ProjectClip::slotCreateAudioThumbs()
{
    Mlt::Producer *prod = producer();
    AudioStreamInfo *audioInfo = m_controller->audioInfo();
    if (audioInfo == NULL) return;
    QString clipHash = hash();
    if (clipHash.isEmpty()) return;
    QString audioPath = bin()->projectFolder().path() + "/thumbs/" + clipHash + "_audio.png";
    double lengthInFrames = prod->get_playtime();
    int frequency = audioInfo->samplingRate();
    if (frequency <= 0) frequency = 48000;
    int channels = audioInfo->channels();
    if (channels <= 0) channels = 2;
    double frame = 0.0;
    QVariantList* audioLevels = new QVariantList;
    QImage image(audioPath);
    if (!image.isNull()) {
        // convert cached image
        int n = image.width() * image.height();
        for (int i = 0; i < n; i++) {
            QRgb p = image.pixel(i / 2, i % channels);
            *audioLevels << qRed(p);
            *audioLevels << qGreen(p);
            *audioLevels << qBlue(p);
            *audioLevels << qAlpha(p);
        }
    }
    if (audioLevels->size() > 0) {
        updateAudioThumbnail(audioLevels);
        return;
    }
    QString service = prod->get("mlt_service");
    if (service == "avformat-novalidate")
        service = "avformat";
    else if (service.startsWith("xml"))
        service = "xml-nogl";
    Mlt::Producer *audioProducer = new Mlt::Producer(*prod->profile(), service.toUtf8().constData(), prod->get("resource"));
    if (!audioProducer->is_valid()) {
        delete audioProducer;
        return;
    }
    Mlt::Filter chans(*prod->profile(), "audiochannels");
    Mlt::Filter converter(*prod->profile(), "audioconvert");
    Mlt::Filter levels(*prod->profile(), "audiolevel");
    audioProducer->attach(chans);
    audioProducer->attach(converter);
    audioProducer->attach(levels);

    audioProducer->set("video_index", "-1");
    int last_val = 0;
    setJobStatus(AbstractClipJob::THUMBJOB, JobWaiting, 0, i18n("Creating audio thumbnails"));
    double framesPerSecond = audioProducer->get_fps();
    mlt_audio_format audioFormat = mlt_audio_s16;
    QStringList keys;
    for (int i = 0; i < channels; i++) {
        keys << "meta.media.audio_level." + QString::number(i);
    }
    for (int z = (int) frame; z < (int)(frame + lengthInFrames) && !m_abortAudioThumb; ++z) {
        int val = (int)((z - frame) / (frame + lengthInFrames) * 100.0);
        if (last_val != val && val > 1) {
            setJobStatus(AbstractClipJob::THUMBJOB, JobWorking, val);
            last_val = val;
        }
        Mlt::Frame *mlt_frame = audioProducer->get_frame();
        if (mlt_frame && mlt_frame->is_valid() && !mlt_frame->get_int("test_audio")) {
            int samples = mlt_sample_calculator(framesPerSecond, frequency, z);
            mlt_frame->get_audio(audioFormat, frequency, channels, samples);
            for (int channel = 0; channel < channels; ++channel) {
                double level = 256 * qMin(mlt_frame->get_double(keys.at(channel).toUtf8().constData()) * 0.9, 1.0);
                *audioLevels << level;
            }
        } else if (!audioLevels->isEmpty()) {
            for (int channel = 0; channel < channels; channel++)
                *audioLevels << audioLevels->last();
        }
        delete mlt_frame;
        if (m_abortAudioThumb) break;
    }

    if (!m_abortAudioThumb && audioLevels->size() > 0) {
        // Put into an image for caching.
        int count = audioLevels->size();
        QImage image((count + 3) / 4, channels, QImage::Format_ARGB32);
        int n = image.width() * image.height();
        for (int i = 0; i < n; i ++) {
            QRgb p; 
            if ((4*i + 3) < count) {
                p = qRgba(audioLevels->at(4*i).toInt(), audioLevels->at(4*i+1).toInt(), audioLevels->at(4*i+2).toInt(), audioLevels->at(4*i+3).toInt());
            } else {
                int last = audioLevels->last().toInt();
                int r = (4*i+0) < count? audioLevels->at(4*i+0).toInt() : last;
                int g = (4*i+1) < count? audioLevels->at(4*i+1).toInt() : last;
                int b = (4*i+2) < count? audioLevels->at(4*i+2).toInt() : last;
                int a = last;
                p = qRgba(r, g, b, a);
            }
            image.setPixel(i / 2, i % channels, p);
        }
        image.save(audioPath);
    }
    delete audioProducer;
    setJobStatus(AbstractClipJob::THUMBJOB, JobDone, 0, i18n("Audio thumbnails done"));
    if (!m_abortAudioThumb) {
        updateAudioThumbnail(audioLevels);
    }
    m_abortAudioThumb = false;
}

bool ProjectClip::isTransparent() const
{
    if (m_type == Text) return true;
    if (m_type == Image && m_controller->int_property("kdenlive:transparency") == 1) return true;
    return false;
}

QStringList ProjectClip::updatedAnalysisData(const QString &name, const QString &data, int offset)
{
    if (data.isEmpty()) {
        // Remove data
        return QStringList() << QString("kdenlive:clipanalysis." + name) << QString();
        //m_controller->resetProperty("kdenlive:clipanalysis." + name);
    }
    else {
        QString current = m_controller->property("kdenlive:clipanalysis." + name);
        if (!current.isEmpty()) {
            if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("Clip already contains analysis data %1", name), QString(), KGuiItem(i18n("Merge")), KGuiItem(i18n("Add"))) == KMessageBox::Yes) {
                // Merge data
                Mlt::Profile *profile = m_controller->profile();
                Mlt::Geometry geometry(current.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
                Mlt::Geometry newGeometry(data.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
                Mlt::GeometryItem item;
                int pos = 0;
                while (!newGeometry.next_key(&item, pos)) {
                    pos = item.frame();
                    item.frame(pos + offset);
                    pos++;
                    geometry.insert(item);
                }
                return QStringList() << QString("kdenlive:clipanalysis." + name) << geometry.serialise();
                //m_controller->setProperty("kdenlive:clipanalysis." + name, geometry.serialise());
            }
            else {
                // Add data with another name
                int i = 1;
                QString data = m_controller->property("kdenlive:clipanalysis." + name + ' ' + QString::number(i));
                while (!data.isEmpty()) {
                    ++i;
                    data = m_controller->property("kdenlive:clipanalysis." + name + ' ' + QString::number(i));
                }
                return QStringList() << QString("kdenlive:clipanalysis." + name + ' ' + QString::number(i)) << geometryWithOffset(data, offset);
                //m_controller->setProperty("kdenlive:clipanalysis." + name + ' ' + QString::number(i), geometryWithOffset(data, offset));
            }
        }
        else {
            return QStringList() << QString("kdenlive:clipanalysis." + name) << geometryWithOffset(data, offset);
            //m_controller->setProperty("kdenlive:clipanalysis." + name, geometryWithOffset(data, offset));
        }
    }
}

QMap <QString, QString> ProjectClip::analysisData(bool withPrefix)
{
    return m_controller->getPropertiesFromPrefix("kdenlive:clipanalysis.", withPrefix);
}

const QString ProjectClip::geometryWithOffset(const QString &data, int offset)
{
    if (offset == 0) return data;
    Mlt::Profile *profile = m_controller->profile();
    Mlt::Geometry geometry(data.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
    Mlt::Geometry newgeometry(NULL, duration().frames(profile->fps()), profile->width(), profile->height());
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
