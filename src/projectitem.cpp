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


#include <QMouseEvent>
#include <QStylePainter>
#include <QLabel>
#include <QLayout>

#include <KDebug>
#include <KLocale>
#include <KIcon>


#include "projectitem.h"
#include "timecode.h"
#include "kdenlivesettings.h"
#include "docclipbase.h"

const int NameRole = Qt::UserRole;
const int DurationRole = NameRole + 1;
const int UsageRole = NameRole + 2;


// folder
ProjectItem::ProjectItem(QTreeWidget * parent, const QStringList & strings, const QString &clipId)
        : QTreeWidgetItem(parent, strings), m_clipType(FOLDER), m_clipId(clipId), m_clip(NULL), m_groupname(strings.at(1)) {
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    setIcon(0, KIcon("folder"));
    setToolTip(1, "<b>" + i18n("Folder"));
    //kDebug() << "Constructed as folder, with clipId: " << m_clipId << ", and groupname: " << m_groupname;
}

ProjectItem::ProjectItem(QTreeWidget * parent, DocClipBase *clip)
        : QTreeWidgetItem(parent) {
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    m_clip = clip;
    m_clipId = clip->getId();
    QString name = m_clip->getProperty("name");
    if (name.isEmpty()) name = KUrl(m_clip->getProperty("resource")).fileName();
    m_clipType = (CLIPTYPE) m_clip->getProperty("type").toInt();
    if (m_clipType != UNKNOWN) slotSetToolTip();
    setText(1, name);
    setText(2, m_clip->description());
    if ((m_clip->clipType() == AV || m_clip->clipType() == AUDIO) && KdenliveSettings::audiothumbnails()) m_clip->askForAudioThumbs();
    //setFlags(Qt::NoItemFlags);
    //kDebug() << "Constructed with clipId: " << m_clipId;
}

ProjectItem::ProjectItem(QTreeWidgetItem * parent, DocClipBase *clip)
        : QTreeWidgetItem(parent) {
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    m_clip = clip;
    m_clipId = clip->getId();
    QString name = m_clip->getProperty("name");
    if (name.isEmpty()) name = KUrl(m_clip->getProperty("resource")).fileName();
    m_clipType = (CLIPTYPE) m_clip->getProperty("type").toInt();
    setText(1, name);
    setText(2, m_clip->description());
    if ((m_clip->clipType() == AV || m_clip->clipType() == AUDIO) && KdenliveSettings::audiothumbnails()) m_clip->askForAudioThumbs();
    //setFlags(Qt::NoItemFlags);
    //kDebug() << "Constructed with clipId: " << m_clipId;
}


ProjectItem::~ProjectItem() {
}

int ProjectItem::numReferences() const {
    if (!m_clip) return 0;
    return m_clip->numReferences();
}

const QString &ProjectItem::clipId() const {
    return m_clipId;
}

CLIPTYPE ProjectItem::clipType() const {
    return m_clipType;
}

int ProjectItem::clipMaxDuration() const {
    return m_clip->getProperty("duration").toInt();
}

bool ProjectItem::isGroup() const {
    return m_clipType == FOLDER;
}

QStringList ProjectItem::names() const {
    QStringList result;
    result.append(text(0));
    result.append(text(1));
    result.append(text(2));
    return result;
}

QDomElement ProjectItem::toXml() const {
    return m_clip->toXML();
}

const KUrl ProjectItem::clipUrl() const {
    if (m_clipType != COLOR && m_clipType != VIRTUAL && m_clipType != UNKNOWN && m_clipType != FOLDER)
        return KUrl(m_clip->getProperty("resource"));
    else return KUrl();
}

void ProjectItem::changeDuration(int frames) {
    setData(1, DurationRole, Timecode::getEasyTimecode(GenTime(frames, KdenliveSettings::project_fps()), KdenliveSettings::project_fps()));
}

void ProjectItem::setProperties(QMap <QString, QString> props) {
    if (m_clip == NULL) return;
    m_clip->setProperties(props);
}

QString ProjectItem::getClipHash() const {
    if (m_clip == NULL) return QString();
    return m_clip->getClipHash();
}

void ProjectItem::setProperty(const QString &key, const QString &value) {
    if (m_clip == NULL) return;
    m_clip->setProperty(key, value);
}

void ProjectItem::clearProperty(const QString &key) {
    if (m_clip == NULL) return;
    m_clip->clearProperty(key);
}

const QString ProjectItem::groupName() const {
    return m_groupname;
}

void ProjectItem::setGroupName(const QString name) {
    m_groupname = name;
    setText(1, name);
}

DocClipBase *ProjectItem::referencedClip() {
    return m_clip;
}

void ProjectItem::slotSetToolTip() {
    QString tip = "<b>";
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
        tip.append(i18n("Text clip"));
        break;
    case SLIDESHOW:
        tip.append(i18n("Slideshow clip"));
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

    setToolTip(1, tip);
}


void ProjectItem::setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata) {
    if (m_clip == NULL) return;
    //setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
    if (attributes.contains("duration")) {
        //if (m_clipType == AUDIO || m_clipType == VIDEO || m_clipType == AV)
        //m_clip->setProperty("duration", attributes["duration"]);
        GenTime duration = GenTime(attributes["duration"].toInt(), KdenliveSettings::project_fps());
        setData(1, DurationRole, Timecode::getEasyTimecode(duration, KdenliveSettings::project_fps()));
        m_clip->setDuration(duration);
        //kDebug() << "//// LOADED CLIP, DURATIONÂ SET TO: " << duration.frames(KdenliveSettings::project_fps());
    } else  {
        // No duration known, use an arbitrary one until it is.
    }


    //extend attributes -reh

    if (m_clipType == UNKNOWN) {
        if (attributes.contains("type")) {
            if (attributes["type"] == "audio")
                m_clipType = AUDIO;
            else if (attributes["type"] == "video")
                m_clipType = VIDEO;
            else if (attributes["type"] == "av")
                m_clipType = AV;
            else if (attributes["type"] == "playlist")
                m_clipType = PLAYLIST;
        } else {
            m_clipType = AV;
        }
        m_clip->setClipType(m_clipType);
        slotSetToolTip();
    }
    m_clip->setProperties(attributes);

    if ((m_clipType == AV || m_clipType == AUDIO) && KdenliveSettings::audiothumbnails()) m_clip->askForAudioThumbs();

    if (m_clip->description().isEmpty()) {
        if (metadata.contains("description")) {
            m_clip->setProperty("description", metadata["description"]);
            setText(2, m_clip->description());
        } else if (metadata.contains("comment")) {
            m_clip->setProperty("description", metadata["comment"]);
            setText(2, m_clip->description());
        }
    }
}

