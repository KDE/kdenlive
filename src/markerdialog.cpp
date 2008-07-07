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


#include <KDebug>

#include "markerdialog.h"
#include "kthumb.h"
#include "kdenlivesettings.h"

MarkerDialog::MarkerDialog(DocClipBase *clip, CommentedTime t, Timecode tc, QWidget * parent): QDialog(parent), m_tc(tc), m_clip(clip), m_marker(t), m_producer(NULL), m_profile(NULL) {
    setFont(KGlobalSettings::toolBarFont());
    m_fps = m_tc.fps();
    m_view.setupUi(this);

    m_previewTimer = new QTimer(this);

    if (m_clip != NULL) {
        m_previewTimer->setInterval(500);
        connect(m_previewTimer, SIGNAL(timeout()), this, SLOT(slotUpdateThumb()));
        m_profile = new Mlt::Profile((char*) KdenliveSettings::current_profile().data());
        m_dar = m_profile->dar();
        QDomDocument doc;
        QDomElement westley = doc.createElement("westley");
        QDomElement play = doc.createElement("playlist");
        doc.appendChild(westley);
        westley.appendChild(play);
        play.appendChild(doc.importNode(clip->toXML(), true));
        //char *tmp = doc.toString().toUtf8().data();
        m_producer = new Mlt::Producer(*m_profile, "westley-xml", doc.toString().toUtf8().data());
        //delete[] tmp;

        QPixmap p((int)(100 * m_dar), 100);
        QString colour = clip->getProperty("colour");
        switch (m_clip->clipType()) {
        case VIDEO:
        case AV:
        case SLIDESHOW:
        case PLAYLIST:
            connect(this, SIGNAL(updateThumb()), m_previewTimer, SLOT(start()));
        case IMAGE:
        case TEXT:
            p = KThumb::getFrame(*m_producer, t.time().frames(m_fps), (int)(100 * m_dar), 100);
            break;
        case COLOR:
            colour = colour.replace(0, 2, "#");
            p.fill(QColor(colour.left(7)));
            break;
        default:
            p.fill(Qt::black);
        }
        if (!p.isNull()) {
            m_view.clip_thumb->setFixedWidth(p.width());
            m_view.clip_thumb->setFixedHeight(p.height());
            m_view.clip_thumb->setPixmap(p);
        }
        connect(m_view.marker_position, SIGNAL(textChanged(const QString &)), this, SIGNAL(updateThumb()));
    } else m_view.clip_thumb->setHidden(true);

    m_view.marker_position->setText(tc.getTimecode(t.time(), m_fps));
    m_view.marker_comment->setText(t.comment());
    connect(m_view.position_up, SIGNAL(clicked()), this, SLOT(slotTimeUp()));
    connect(m_view.position_down, SIGNAL(clicked()), this, SLOT(slotTimeDown()));

    m_view.marker_comment->selectAll();
    m_view.marker_comment->setFocus();

    adjustSize();
}

MarkerDialog::~MarkerDialog() {
    delete m_previewTimer;
    if (m_producer) delete m_producer;
    if (m_profile) delete m_profile;
}

void MarkerDialog::slotUpdateThumb() {
    m_previewTimer->stop();
    int pos = m_tc.getFrameCount(m_view.marker_position->text(), m_fps);
    QPixmap p = KThumb::getFrame(*m_producer, pos, (int)(100 * m_dar), 100);
    if (!p.isNull()) m_view.clip_thumb->setPixmap(p);
    else kDebug() << "!!!!!!!!!!!  ERROR CREATING THUMB";
}

void MarkerDialog::slotTimeUp() {
    int duration = m_tc.getFrameCount(m_view.marker_position->text(), m_fps);
    if (duration >= m_clip->duration().frames(m_fps)) return;
    duration ++;
    m_view.marker_position->setText(m_tc.getTimecode(GenTime(duration, m_fps), m_fps));
}

void MarkerDialog::slotTimeDown() {
    int duration = m_tc.getFrameCount(m_view.marker_position->text(), m_fps);
    if (duration <= 0) return;
    duration --;
    m_view.marker_position->setText(m_tc.getTimecode(GenTime(duration, m_fps), m_fps));
}

CommentedTime MarkerDialog::newMarker() {
    return CommentedTime(GenTime(m_tc.getFrameCount(m_view.marker_position->text(), m_fps), m_fps), m_view.marker_comment->text());
}

#include "markerdialog.moc"


