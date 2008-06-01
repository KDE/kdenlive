/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include <QDir>

#include <KStandardDirs>
#include <KDebug>
#include <KFileItem>

#include "kdenlivesettings.h"
#include "clipproperties.h"
#include "kthumb.h"

#define VIDEOTAB 0
#define AUDIOTAB 1
#define COLORTAB 2
#define SLIDETAB 3
#define ADVANCEDTAB 4

#define TYPE_JPEG 0
#define TYPE_PNG 1
#define TYPE_BMP 2
#define TYPE_GIF 3

ClipProperties::ClipProperties(DocClipBase *clip, Timecode tc, double fps, QWidget * parent): QDialog(parent), m_tc(tc), m_clip(clip), m_fps(fps), m_clipNeedsRefresh(false), m_count(0) {
    setFont(KGlobalSettings::toolBarFont());
    m_view.setupUi(this);
    KUrl url = m_clip->fileURL();
    m_view.clip_path->setText(url.path());
    m_view.clip_description->setText(m_clip->description());
    QMap <QString, QString> props = m_clip->properties();

    if (props.contains("audiocodec"))
        m_view.clip_acodec->setText(props.value("audiocodec"));
    if (props.contains("frequency"))
        m_view.clip_frequency->setText(props.value("frequency"));
    if (props.contains("channels"))
        m_view.clip_channels->setText(props.value("channels"));

    CLIPTYPE t = m_clip->clipType();
    if (t == COLOR) {
        m_view.clip_path->setEnabled(false);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
        m_view.clip_color->setColor(QColor("#" + props.value("colour").right(8).left(6)));
    } else if (t == SLIDESHOW) {
        m_view.clip_path->setText(url.directory());
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        QStringList types;
        types << "JPG" << "PNG" << "BMP" << "GIF";
        m_view.image_type->addItems(types);
        m_view.slide_loop->setChecked(props.value("loop").toInt());
        QString path = props.value("resource");
        if (path.endsWith("png")) m_view.image_type->setCurrentIndex(TYPE_PNG);
        else if (path.endsWith("bmp")) m_view.image_type->setCurrentIndex(TYPE_BMP);
        else if (path.endsWith("gif")) m_view.image_type->setCurrentIndex(TYPE_GIF);
        m_view.slide_duration->setText(tc.getTimecodeFromFrames(props.value("ttl").toInt()));
        parseFolder();
        connect(m_view.image_type, SIGNAL(currentIndexChanged(int)), this, SLOT(parseFolder()));
    } else if (t != AUDIO) {
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        if (props.contains("frame_size"))
            m_view.clip_size->setText(props.value("frame_size"));
        if (props.contains("videocodec"))
            m_view.clip_vcodec->setText(props.value("videocodec"));
        if (props.contains("fps"))
            m_view.clip_fps->setText(props.value("fps"));
        if (props.contains("aspect_ratio"))
            m_view.clip_ratio->setText(props.value("aspect_ratio"));

        QPixmap pix = m_clip->thumbProducer()->getImage(url, 240, 180);
        m_view.clip_thumb->setPixmap(pix);
        if (t == IMAGE || t == VIDEO) m_view.tabWidget->removeTab(AUDIOTAB);
    } else {
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
    }
    if (t != IMAGE && t != COLOR && t != TEXT) m_view.clip_duration->setReadOnly(true);

    KFileItem f(KFileItem::Unknown, KFileItem::Unknown, url, true);
    m_view.clip_filesize->setText(KIO::convertSize(f.size()));
    m_view.clip_duration->setText(tc.getTimecode(m_clip->duration(), m_fps));

    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();

    for (uint count = 0; count < marks.count(); ++count) {
	QString time = m_tc.getTimecode(marks[count].time(), m_tc.fps());
	QStringList itemtext;
	itemtext << time << marks[count].comment();
	(void) new QTreeWidgetItem(m_view.markers_list, itemtext);
    }
    

    adjustSize();
}

int ClipProperties::clipId() const {
    return m_clip->getId();
}


QMap <QString, QString> ClipProperties::properties() {
    QMap <QString, QString> props;
    props["description"] = m_view.clip_description->text();
    CLIPTYPE t = m_clip->clipType();
    if (t == COLOR) {
        QMap <QString, QString> old_props = m_clip->properties();
        QString new_color = m_view.clip_color->color().name();
        if (new_color != QString("#" + old_props.value("colour").right(8).left(6))) {
            m_clipNeedsRefresh = true;
            props["colour"] = "0x" + new_color.right(6) + "ff";
        }
    } else if (t == SLIDESHOW) {
        props["loop"] = QString::number((int) m_view.slide_loop->isChecked());
        QMap <QString, QString> old_props = m_clip->properties();
        QString extension;
        switch (m_view.image_type->currentIndex()) {
        case TYPE_PNG:
            extension = "/.all.png";
            break;
        case TYPE_BMP:
            extension = "/.all.bmp";
            break;
        case TYPE_GIF:
            extension = "/.all.gif";
            break;
        default:
            extension = "/.all.jpg";
            break;
        }
        QString new_path = m_view.clip_path->text() + extension;
        if (new_path != old_props.value("resource")) {
            m_clipNeedsRefresh = true;
            props["resource"] = new_path;
            kDebug() << "////  SLIDE EDIT, NEW:" << new_path << ", OLD; " << old_props.value("resource");
        }
        int duration = m_tc.getFrameCount(m_view.slide_duration->text(), m_fps);
        if (duration != old_props.value("ttl").toInt()) {
            m_clipNeedsRefresh = true;
            props["ttl"] = QString::number(duration);
            props["out"] = QString::number(duration * m_count);
        }
        if (duration * m_count != old_props.value("out").toInt()) {
            m_clipNeedsRefresh = true;
            props["out"] = QString::number(duration * m_count);
        }

    }
    return props;
}

bool ClipProperties::needsTimelineRefresh() const {
    return m_clipNeedsRefresh;
}

void ClipProperties::parseFolder() {

    QDir dir(m_view.clip_path->text());
    QStringList filters;
    QString extension;
    switch (m_view.image_type->currentIndex()) {
    case TYPE_PNG:
        filters << "*.png";
        extension = "/.all.png";
        break;
    case TYPE_BMP:
        filters << "*.bmp";
        extension = "/.all.bmp";
        break;
    case TYPE_GIF:
        filters << "*.gif";
        extension = "/.all.gif";
        break;
    default:
        filters << "*.jpg" << "*.jpeg";
        extension = "/.all.jpg";
        break;
    }

    dir.setNameFilters(filters);
    QStringList result = dir.entryList(QDir::Files);
    m_count = result.count();
    m_view.slide_info->setText(i18n("%1 images found", m_count));
    QDomElement xml = m_clip->toXML();
    xml.setAttribute("resource", m_view.clip_path->text() + extension);
    QPixmap pix = m_clip->thumbProducer()->getImage(xml, 1, 240, 180);
    QMap <QString, QString> props = m_clip->properties();
    m_view.clip_duration->setText(m_tc.getTimecodeFromFrames(props.value("ttl").toInt() * m_count));
    m_view.clip_thumb->setPixmap(pix);
}

#include "clipproperties.moc"


