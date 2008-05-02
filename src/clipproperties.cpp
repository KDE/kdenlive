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

ClipProperties::ClipProperties(DocClipBase *clip, Timecode tc, double fps, QWidget * parent): QDialog(parent), m_tc(tc), m_clip(clip), m_fps(fps) {
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
    }
    else if (t == SLIDESHOW) {
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
    }
    else if (t != AUDIO) {
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
    adjustSize();
}

int ClipProperties::clipId() {
    return m_clip->getId();
}


QMap <QString, QString> ClipProperties::properties() {
    QMap <QString, QString> props;
    props["description"] = m_view.clip_description->text();
    CLIPTYPE t = m_clip->clipType();
    if (t == SLIDESHOW) {
	props["loop"] = QString::number((int) m_view.slide_loop->isChecked());
    }
    else if (t == COLOR) {
	QMap <QString, QString> old_props = m_clip->properties();
	QString new_color = m_view.clip_color->color().name();
	if (new_color != QString("#" + old_props.value("colour").right(8).left(6)))
	    props["colour"] = "0x" + new_color.right(6) + "ff";
    }
    return props;
}

#include "clipproperties.moc"


