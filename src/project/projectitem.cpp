/***************************************************************************
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


#include "projectitem.h"

#include "timecode.h"
#include "kdenlivesettings.h"
#include "doc/docclipbase.h"

#include <KDebug>
#include <KLocalizedString>
#include <QIcon>

#include <QFile>

const int DurationRole = Qt::UserRole + 1;
const int JobProgressRole = Qt::UserRole + 5;
const int JobTypeRole = Qt::UserRole + 6;
const int JobStatusMessage = Qt::UserRole + 7;


ProjectItem::ProjectItem(QTreeWidget * parent, DocClipBase *clip, const QSize &pixmapSize) :
        QTreeWidgetItem(parent, ProjectClipType),
        m_clip(clip),
        m_clipId(clip->getId()),
        m_pixmapSet(false)
{
    buildItem(pixmapSize);
}

ProjectItem::ProjectItem(QTreeWidgetItem * parent, DocClipBase *clip, const QSize &pixmapSize) :
        QTreeWidgetItem(parent, ProjectClipType),
        m_clip(clip),
        m_clipId(clip->getId()),
        m_pixmapSet(false)
        
{
    buildItem(pixmapSize);
}

void ProjectItem::buildItem(const QSize &pixmapSize)
{
    // keep in sync with declaration un projectitem.cpp and clipmanager.cpp
    int itemHeight = (treeWidget() ? treeWidget()->iconSize().height() : 38);
    setSizeHint(0, QSize(itemHeight * 3, itemHeight));
    if (m_clip->isPlaceHolder()) setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    else setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
    QString name = m_clip->getProperty("name");
    if (name.isEmpty()) name = QUrl(m_clip->getProperty("resource")).fileName();
    m_clipType = (ClipType) m_clip->getProperty("type").toInt();
    switch(m_clipType) {
	case Audio:
	    setData(0, Qt::DecorationRole, QIcon::fromTheme("audio-x-generic").pixmap(pixmapSize));
	    m_pixmapSet = true;
	    break;
	case Image:
	case SlideShow:
	    setData(0, Qt::DecorationRole, QIcon::fromTheme("image-x-generic").pixmap(pixmapSize));
	    break;
	default:
	    setData(0, Qt::DecorationRole, QIcon::fromTheme("video-x-generic").pixmap(pixmapSize));
    }
    if (m_clipType != Unknown) slotSetToolTip();
    
    setText(0, name);
    setText(1, m_clip->description());
    GenTime duration = m_clip->duration();
    QString durationText;
    if (duration != GenTime()) {
        durationText = Timecode::getEasyTimecode(duration, KdenliveSettings::project_fps());
    }
    if (m_clipType == Playlist) {
        // Check if the playlist xml contains a proxy inside, and inform user
        if (playlistHasProxies(m_clip->fileURL().path())) {
            durationText.prepend(i18n("Contains proxies") + " / ");
        }
    }
    if (!durationText.isEmpty()) setData(0, DurationRole, durationText);
}

ProjectItem::~ProjectItem()
{
}

bool ProjectItem::hasPixmap() const
{
    return m_pixmapSet;
}

void ProjectItem::setPixmap(const QPixmap& p)
{
    m_pixmapSet = true;
    setData(0, Qt::DecorationRole, p);
}

int ProjectItem::numReferences() const
{
    if (!m_clip) return 0;
    return m_clip->numReferences();
}

const QString &ProjectItem::clipId() const
{
    return m_clipId;
}

ClipType ProjectItem::clipType() const
{
    return m_clipType;
}

int ProjectItem::clipMaxDuration() const
{
    return m_clip->getProperty("duration").toInt();
}

QDomElement ProjectItem::toXml() const
{
    return m_clip->toXML();
}

const QUrl ProjectItem::clipUrl() const
{
    if (m_clipType != Color && m_clipType != Virtual && m_clipType != Unknown)
        return QUrl(m_clip->getProperty("resource"));
    else return QUrl();
}

void ProjectItem::changeDuration(int frames)
{
    QString itemdata = data(0, DurationRole).toString();
    if (itemdata.contains('/')) itemdata = itemdata.section('/', 0, 0) + "/ ";
    else itemdata.clear();
    setData(0, DurationRole, itemdata + Timecode::getEasyTimecode(GenTime(frames, KdenliveSettings::project_fps()), KdenliveSettings::project_fps()));
}

void ProjectItem::setProperties(const QMap<QString, QString> &props)
{
    if (m_clip == NULL) return;
    m_clip->setProperties(props);
}

QString ProjectItem::getClipHash() const
{
    if (m_clip == NULL) return QString();
    return m_clip->getClipHash();
}

void ProjectItem::setProperty(const QString &key, const QString &value)
{
    if (m_clip == NULL) return;
    m_clip->setProperty(key, value);
}

void ProjectItem::clearProperty(const QString &key)
{
    if (m_clip == NULL) return;
    m_clip->clearProperty(key);
}

DocClipBase *ProjectItem::referencedClip()
{
    return m_clip;
}

void ProjectItem::slotSetToolTip()
{
    QString tip;
    if (m_clip->isPlaceHolder()) tip.append(i18n("Missing") + " | ");
    QString jobInfo = data(0, JobStatusMessage).toString();
    if (!jobInfo.isEmpty()) {
        tip.append(jobInfo + " | ");
    }
    if (hasProxy() && data(0, JobTypeRole).toInt() != PROXYJOB) {
        tip.append(i18n("Proxy clip") + " | ");
    }
    tip.append(m_clip->shortInfo());
    setToolTip(0, tip);
}


void ProjectItem::setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata)
{
    if (m_clip == NULL) return;

    QString prefix;

    m_clip->setProperties(attributes);
    m_clip->setMetadata(metadata);
    
    if (m_clipType == Unknown) {
        QString cliptype = attributes.value("type");
        if (cliptype == "audio") m_clipType = Audio;
        else if (cliptype == "video") m_clipType = Video;
        else if (cliptype == "av") m_clipType = AV;
        else if (cliptype == "playlist") m_clipType = Playlist;
        else m_clipType = AV;

        m_clip->setClipType(m_clipType);
        slotSetToolTip();
        if (m_clipType == Playlist) {
            // Check if the playlist xml contains a proxy inside, and inform user
            if (playlistHasProxies(m_clip->fileURL().path())) {
                prefix = i18n("Contains proxies") + " / ";
            }
        }
    }
    else if (attributes.contains("frame_size")) slotSetToolTip();
    
    if (attributes.contains("duration")) {
        GenTime duration = GenTime(attributes.value("duration").toInt(), KdenliveSettings::project_fps());
        QString itemdata = data(0, DurationRole).toString();
        if (itemdata.contains('/')) itemdata = itemdata.section('/', 0, 0) + "/ ";
        else itemdata.clear();
        if (prefix.isEmpty()) prefix = itemdata;
        setData(0, DurationRole, prefix + Timecode::getEasyTimecode(duration, KdenliveSettings::project_fps()));
        m_clip->setDuration(duration);
    } else  {
        // No duration known, use an arbitrary one until it is.
    }

    if (m_clip->description().isEmpty()) {
        if (metadata.contains("description")) {
            m_clip->setProperty("description", metadata.value("description"));
            setText(1, m_clip->description());
        } else if (metadata.contains("comment")) {
            m_clip->setProperty("description", metadata.value("comment"));
            setText(1, m_clip->description());
        }
    }
}

void ProjectItem::setJobStatus(JOBTYPE jobType, ClipJobStatus status, int progress, const QString &statusMessage)
{
    setData(0, JobTypeRole, jobType);
    if (progress > 0) setData(0, JobProgressRole, qMin(100, progress));
    else {
        setData(0, JobProgressRole, status);
        if ((status == JobAborted || status == JobCrashed  || status == JobDone) || !statusMessage.isEmpty())
            setData(0, JobStatusMessage, statusMessage);
        slotSetToolTip();
    }
}

void ProjectItem::setConditionalJobStatus(ClipJobStatus status, JOBTYPE requestedJobType)
{
    if (data(0, JobTypeRole).toInt() == requestedJobType) {
        setData(0, JobProgressRole, status);
    }
}

bool ProjectItem::hasProxy() const
{
    if (m_clip == NULL) return false;
    if (m_clip->getProperty("proxy").size() < 2 || data(0, JobProgressRole).toInt() == JobCrashed) return false;
    return true;
}

bool ProjectItem::isProxyReady() const
{
     return (data(0, JobProgressRole).toInt() == JobDone);
}

bool ProjectItem::isJobRunning() const
{
    int s = data(0, JobProgressRole).toInt();
    if (s == JobWaiting || s == JobWorking || s > 0) return true;
    return false;
}

bool ProjectItem::isProxyRunning() const
{
    int s = data(0, JobProgressRole).toInt();
    if ((s == JobWorking || s > 0) && data(0, JobTypeRole).toInt() == (int) PROXYJOB) return true;
    return false;
}

bool ProjectItem::playlistHasProxies(const QString& path)
{
    kDebug()<<"// CHECKING FOR PROXIES";
    QFile file(path);
    QDomDocument doc;
    if (!file.open(QIODevice::ReadOnly))
    return false;
    if (!doc.setContent(&file)) {
        file.close();
        return false;
    }
    file.close();
    QString root = doc.documentElement().attribute("root");
    QDomNodeList kdenliveProducers = doc.elementsByTagName("kdenlive_producer");
    for (int i = 0; i < kdenliveProducers.count(); ++i) {
        QString proxy = kdenliveProducers.at(i).toElement().attribute("proxy");
        if (!proxy.isEmpty() && proxy != "-") return true;
    }
    return false;
}



