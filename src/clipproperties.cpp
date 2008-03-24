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

ClipProperties::ClipProperties(DocClipBase *clip, Timecode tc, double fps, QWidget * parent): QDialog(parent), m_tc(tc), m_clip(clip), m_fps(fps) {
    setFont(KGlobalSettings::toolBarFont());
    m_view.setupUi(this);
    KUrl url = m_clip->fileURL();
    m_view.clip_path->setText(url.path());
    m_view.clip_description->setText(m_clip->description());
    QMap <QString, QString> props = m_clip->properties();

    if (props.contains("audiocodec"))
        m_view.clip_acodec->setText(props["audiocodec"]);
    if (props.contains("frequency"))
        m_view.clip_frequency->setText(props["frequency"]);
    if (props.contains("channels"))
        m_view.clip_channels->setText(props["channels"]);

    CLIPTYPE t = m_clip->clipType();
    if (t != AUDIO) {
        if (props.contains("frame_size"))
            m_view.clip_size->setText(props["frame_size"]);
        if (props.contains("videocodec"))
            m_view.clip_vcodec->setText(props["videocodec"]);
        if (props.contains("fps"))
            m_view.clip_fps->setText(props["fps"]);
        if (props.contains("aspect_ratio"))
            m_view.clip_ratio->setText(props["aspect_ratio"]);

        QPixmap pix = m_clip->thumbProducer()->getImage(url, 240, 180);
        m_view.clip_thumb->setPixmap(pix);
        if (t == IMAGE || t == VIDEO) m_view.tabWidget->removeTab(1);
    } else {
        m_view.tabWidget->removeTab(0);
        m_view.clip_thumb->setHidden(true);
    }
    if (t != IMAGE && t != COLOR) m_view.clip_duration->setReadOnly(true);

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
    return props;
}

#include "clipproperties.moc"


