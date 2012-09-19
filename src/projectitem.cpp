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
#include "docclipbase.h"

#include <KDebug>
#include <KLocale>
#include <KIcon>

#include <QFile>

const int DurationRole = Qt::UserRole + 1;
const int JobProgressRole = Qt::UserRole + 5;
const int JobTypeRole = Qt::UserRole + 6;
const int JobStatusMessage = Qt::UserRole + 7;
const int itemHeight = 38;

ProjectItem::ProjectItem(QTreeWidget * parent, DocClipBase *clip) :
        QTreeWidgetItem(parent, PROJECTCLIPTYPE),
        m_clip(clip),
        m_clipId(clip->getId())
{
    buildItem();
}

ProjectItem::ProjectItem(QTreeWidgetItem * parent, DocClipBase *clip) :
        QTreeWidgetItem(parent, PROJECTCLIPTYPE),
        m_clip(clip),
        m_clipId(clip->getId())
        
{
    buildItem();
}

void ProjectItem::buildItem()
{
    setSizeHint(0, QSize(itemHeight * 3, itemHeight));
    if (m_clip->isPlaceHolder()) setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    else setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
    QString name = m_clip->getProperty("name");
    if (name.isEmpty()) name = KUrl(m_clip->getProperty("resource")).fileName();
    m_clipType = (CLIPTYPE) m_clip->getProperty("type").toInt();
    setText(0, name);
    setText(1, m_clip->description());
    GenTime duration = m_clip->duration();
    QString durationText;
    if (duration != GenTime()) {
        durationText = Timecode::getEasyTimecode(duration, KdenliveSettings::project_fps());
    }
    if (m_clipType == PLAYLIST) {
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

//static
int ProjectItem::itemDefaultHeight()
{
    return itemHeight;
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

CLIPTYPE ProjectItem::clipType() const
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

const KUrl ProjectItem::clipUrl() const
{
    if (m_clipType != COLOR && m_clipType != VIRTUAL && m_clipType != UNKNOWN)
        return KUrl(m_clip->getProperty("resource"));
    else return KUrl();
}

void ProjectItem::changeDuration(int frames)
{
    QString itemdata = data(0, DurationRole).toString();
    if (itemdata.contains('/')) itemdata = itemdata.section('/', 0, 0) + "/ ";
    else itemdata.clear();
    setData(0, DurationRole, itemdata + Timecode::getEasyTimecode(GenTime(frames, KdenliveSettings::project_fps()), KdenliveSettings::project_fps()));
}

void ProjectItem::setProperties(QMap <QString, QString> props)
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
    tip.append("<b>");
    switch (m_clipType) {
    case AUDIO:
        tip.append(i18n("Audio clip") + "</b><br />" + clipUrl().path());
        break;
    case VIDEO:
        tip.append(i18n("Mute video clip") + "</b><br />" + clipUrl().path());
        break;
    case AV:
        tip.append(i18n("Video clip") + "</b><br />" + clipUrl().path());
        break;
    case COLOR:
        tip.append(i18n("Color clip"));
        break;
    case IMAGE:
        tip.append(i18n("Image clip") + "</b><br />" + clipUrl().path());
        break;
    case TEXT:
        if (!clipUrl().isEmpty() && m_clip->getProperty("xmldata").isEmpty()) tip.append(i18n("Template text clip") + "</b><br />" + clipUrl().path());
        else tip.append(i18n("Text clip") + "</b><br />" + clipUrl().path());
        break;
    case SLIDESHOW:
        tip.append(i18n("Slideshow clip") + "</b><br />" + clipUrl().directory());
        break;
    case VIRTUAL:
        tip.append(i18n("Virtual clip"));
        break;
    case PLAYLIST:
        tip.append(i18n("Playlist clip") + "</b><br />" + clipUrl().path());
        break;
    default:
        tip.append(i18n("Unknown clip"));
        break;
    }
    setToolTip(0, tip);
}


void ProjectItem::setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata)
{
    if (m_clip == NULL) return;

    QString prefix;
    if (m_clipType == UNKNOWN) {
        QString cliptype = attributes.value("type");
        if (cliptype == "audio") m_clipType = AUDIO;
        else if (cliptype == "video") m_clipType = VIDEO;
        else if (cliptype == "av") m_clipType = AV;
        else if (cliptype == "playlist") m_clipType = PLAYLIST;
        else m_clipType = AV;

        m_clip->setClipType(m_clipType);
        slotSetToolTip();
        if (m_clipType == PLAYLIST) {
            // Check if the playlist xml contains a proxy inside, and inform user
            if (playlistHasProxies(m_clip->fileURL().path())) {
                prefix = i18n("Contains proxies") + " / ";
            }
        }
    }
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

    m_clip->setProperties(attributes);
    m_clip->setMetadata(metadata);

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

void ProjectItem::setJobStatus(JOBTYPE jobType, CLIPJOBSTATUS status, int progress, const QString &statusMessage)
{
    setData(0, JobTypeRole, jobType);
    if (progress > 0) setData(0, JobProgressRole, qMin(100, progress));
    else {
        setData(0, JobProgressRole, status);
        if ((status == JOBABORTED || status == JOBCRASHED  || status == JOBDONE) || !statusMessage.isEmpty())
            setData(0, JobStatusMessage, statusMessage);
        slotSetToolTip();
    }
}

void ProjectItem::setConditionalJobStatus(CLIPJOBSTATUS status, JOBTYPE requestedJobType)
{
    if (data(0, JobTypeRole).toInt() == requestedJobType) {
        setData(0, JobProgressRole, status);
    }
}

bool ProjectItem::hasProxy() const
{
    if (m_clip == NULL) return false;
    if (m_clip->getProperty("proxy").size() < 2 || data(0, JobProgressRole).toInt() == JOBCRASHED) return false;
    return true;
}

bool ProjectItem::isProxyReady() const
{
     return (data(0, JobProgressRole).toInt() == JOBDONE);
}

bool ProjectItem::isJobRunning() const
{
    int s = data(0, JobProgressRole).toInt();
    if (s == JOBWAITING || s == JOBWORKING || s > 0) return true;
    return false;
}

bool ProjectItem::isProxyRunning() const
{
    int s = data(0, JobProgressRole).toInt();
    if ((s == JOBWORKING || s > 0) && data(0, JobTypeRole).toInt() == (int) PROXYJOB) return true;
    return false;
}

bool ProjectItem::playlistHasProxies(const QString path)
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
    for (int i = 0; i < kdenliveProducers.count(); i++) {
        QString proxy = kdenliveProducers.at(i).toElement().attribute("proxy");
        if (!proxy.isEmpty() && proxy != "-") return true;
    }
    return false;
}



