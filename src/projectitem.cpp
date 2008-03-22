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


ProjectItem::ProjectItem(QTreeWidget * parent, const QStringList & strings, QDomElement xml, int clipId)
        : QTreeWidgetItem(parent, strings, QTreeWidgetItem::UserType), m_clipType(UNKNOWN), m_clipId(clipId) {
    m_element = xml.cloneNode().toElement();
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    if (!m_element.isNull()) {
        m_element.setAttribute("id", clipId);
        QString cType = m_element.attribute("type", QString::null);
        if (!cType.isEmpty()) {
            m_clipType = (CLIPTYPE) cType.toInt();
            slotSetToolTip();
        }

        if (m_clipType == COLOR || m_clipType == IMAGE) m_element.setAttribute("duration", MAXCLIPDURATION);
        else if (m_element.attribute("duration").isEmpty() && !m_element.attribute("out").isEmpty()) {
            m_element.setAttribute("duration", m_element.attribute("out").toInt() - m_element.attribute("in").toInt());
        }
    }
}

ProjectItem::ProjectItem(QTreeWidgetItem * parent, const QStringList & strings, QDomElement xml, int clipId)
        : QTreeWidgetItem(parent, strings, QTreeWidgetItem::UserType), m_clipType(UNKNOWN), m_clipId(clipId) {
    m_element = xml.cloneNode().toElement();
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    if (!m_element.isNull()) {
        m_element.setAttribute("id", clipId);
        QString cType = m_element.attribute("type", QString::null);
        if (!cType.isEmpty()) {
            m_clipType = (CLIPTYPE) cType.toInt();
            slotSetToolTip();
        }
    }
}

// folder
ProjectItem::ProjectItem(QTreeWidget * parent, const QStringList & strings, int clipId)
        : QTreeWidgetItem(parent, strings, QTreeWidgetItem::UserType), m_element(QDomElement()), m_clipType(FOLDER), m_groupName(strings.at(1)), m_clipId(clipId), m_clip(NULL) {
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    setIcon(0, KIcon("folder"));
}

ProjectItem::ProjectItem(QTreeWidget * parent, DocClipBase *clip)
        : QTreeWidgetItem(parent, QStringList(), QTreeWidgetItem::UserType) {
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    m_clip = clip;
    m_element = clip->toXML();
    m_clipId = clip->getId();
    QString name = m_element.attribute("name");
    if (name.isEmpty()) name = KUrl(m_element.attribute("resource")).fileName();
    m_clipType = (CLIPTYPE) m_element.attribute("type").toInt();
    setText(1, name);
    kDebug() << "PROJECT ITE;. ADDING LCIP: " << m_clipId;
}

ProjectItem::ProjectItem(QTreeWidgetItem * parent, DocClipBase *clip)
        : QTreeWidgetItem(parent, QStringList(), QTreeWidgetItem::UserType) {
    setSizeHint(0, QSize(65, 45));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    m_clip = clip;
    m_element = clip->toXML();
    m_clipId = clip->getId();
    QString name = m_element.attribute("name");
    if (name.isEmpty()) name = KUrl(m_element.attribute("resource")).fileName();
    m_clipType = (CLIPTYPE) m_element.attribute("type").toInt();
    setText(1, name);
    kDebug() << "PROJECT ITE;. ADDING LCIP: " << m_clipId;
}


ProjectItem::~ProjectItem() {
}

int ProjectItem::numReferences() const {
    if (!m_clip) return 0;
    return m_clip->numReferences();
}

int ProjectItem::clipId() const {
    return m_clipId;
}

CLIPTYPE ProjectItem::clipType() const {
    return m_clipType;
}

int ProjectItem::clipMaxDuration() const {
    return m_element.attribute("duration").toInt();
}

bool ProjectItem::isGroup() const {
    return m_clipType == FOLDER;
}

const QString ProjectItem::groupName() const {
    return m_groupName;
}

void ProjectItem::setGroupName(const QString name) {
    m_groupName = name;
}

void ProjectItem::setGroup(const QString name, const QString id) {
    if (m_clip) m_clip->setGroup(name, id);
}

QStringList ProjectItem::names() const {
    QStringList result;
    result.append(text(0));
    result.append(text(1));
    result.append(text(2));
    return result;
}

QDomElement ProjectItem::toXml() const {
    return m_element;
}

const KUrl ProjectItem::clipUrl() const {
    if (m_clipType != COLOR && m_clipType != VIRTUAL && m_clipType != UNKNOWN)
        return KUrl(m_element.attribute("resource"));
    else return KUrl();
}


void ProjectItem::slotSetToolTip() {
    QString tip = "<qt><b>";
    switch (m_clipType) {
    case 1:
        tip.append(i18n("Audio clip"));
        break;
    case 2:
        tip.append(i18n("Mute video clip"));
        break;
    case 3:
        tip.append(i18n("Video clip"));
        break;
    case 4:
        tip.append(i18n("Color clip"));
        setData(1, DurationRole, Timecode::getEasyTimecode(GenTime(m_element.attribute("out", "250").toInt(), 25), 25));
        break;
    case 5:
        tip.append(i18n("Image clip"));
        break;
    case 6:
        tip.append(i18n("Text clip"));
        break;
    case 7:
        tip.append(i18n("Slideshow clip"));
        break;
    case 8:
        tip.append(i18n("Virtual clip"));
        break;
    case 9:
        tip.append(i18n("Playlist clip"));
        break;
    default:
        tip.append(i18n("Unknown clip"));
        break;
    }

    setToolTip(1, tip);
}

void ProjectItem::setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata) {
    if (attributes.contains("duration")) {
        if (m_clipType == AUDIO || m_clipType == VIDEO || m_clipType == AV) m_element.setAttribute("duration", attributes["duration"].toInt());
        m_duration = GenTime(attributes["duration"].toInt(), 25);
        setData(1, DurationRole, Timecode::getEasyTimecode(m_duration, 25));
        m_durationKnown = true;
        m_clip->setDuration(m_duration);
        kDebug() << "//// LOADED CLIP, DURATION SET TO: " << m_duration.frames(25);
    } else {
        // No duration known, use an arbitrary one until it is.
        m_duration = GenTime(0.0);
        m_durationKnown = false;
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
    }
    slotSetToolTip();
    if (m_element.isNull()) {
        QDomDocument doc;
        m_element = doc.createElement("producer");
    }
    if (m_element.attribute("duration") == QString::null) m_element.setAttribute("duration", attributes["duration"].toInt());
    m_element.setAttribute("resource", attributes["filename"]);
    m_element.setAttribute("type", (int) m_clipType);

    if (KdenliveSettings::audiothumbnails()) m_clip->slotRequestAudioThumbs();
    /*
     if (attributes.contains("height")) {
         m_height = attributes["height"].toInt();
     } else {
         m_height = 0;
     }
     if (attributes.contains("width")) {
         m_width = attributes["width"].toInt();
     } else {
         m_width = 0;
     }
     //decoder name
     if (attributes.contains("name")) {
         m_decompressor = attributes["name"];
     } else {
         m_decompressor = "n/a";
     }
     //video type ntsc/pal
     if (attributes.contains("system")) {
         m_system = attributes["system"];
     } else {
         m_system = "n/a";
     }
     if (attributes.contains("fps")) {
         m_framesPerSecond = attributes["fps"].toInt();
     } else {
         // No frame rate known.
         m_framesPerSecond = 0;
     }
     //audio attributes -reh
     if (attributes.contains("channels")) {
         m_channels = attributes["channels"].toInt();
     } else {
         m_channels = 0;
     }
     if (attributes.contains("format")) {
         m_format = attributes["format"];
     } else {
         m_format = "n/a";
     }
     if (attributes.contains("frequency")) {
         m_frequency = attributes["frequency"].toInt();
     } else {
         m_frequency = 0;
     }
     if (attributes.contains("videocodec")) {
         m_videoCodec = attributes["videocodec"];
     }
     if (attributes.contains("audiocodec")) {
         m_audioCodec = attributes["audiocodec"];
     }

     m_metadata = metadata;

     if (m_metadata.contains("description")) {
         setDescription (m_metadata["description"]);
     }
     else if (m_metadata.contains("comment")) {
         setDescription (m_metadata["comment"]);
     }
    */

}

#include "projectitem.moc"
