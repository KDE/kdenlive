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


#include "markerdialog.h"
#include "kthumb.h"
#include "kdenlivesettings.h"

#include <QWheelEvent>
#include <KDebug>


MarkerDialog::MarkerDialog(DocClipBase *clip, CommentedTime t, Timecode tc, const QString &caption, QWidget * parent) :
        QDialog(parent),
        m_producer(NULL),
        m_profile(NULL),
        m_clip(clip),
        m_tc(tc)
{
    setFont(KGlobalSettings::toolBarFont());
    m_fps = m_tc.fps();
    setupUi(this);
    setWindowTitle(caption);
    m_previewTimer = new QTimer(this);

    if (m_clip != NULL) {
        m_previewTimer->setInterval(500);
        connect(m_previewTimer, SIGNAL(timeout()), this, SLOT(slotUpdateThumb()));
        m_profile = new Mlt::Profile((char*) KdenliveSettings::current_profile().data());
        m_dar = m_profile->dar();
        QDomDocument doc;
        QDomElement mlt = doc.createElement("mlt");
        QDomElement play = doc.createElement("mlt");
        doc.appendChild(mlt);
        mlt.appendChild(play);
        play.appendChild(doc.importNode(clip->toXML(), true));
        //char *tmp = doc.toString().toUtf8().data();
        m_producer = new Mlt::Producer(*m_profile, "xml-string", doc.toString().toUtf8().data());
        //delete[] tmp;
        int width = 100.0 * m_dar;
        if (width % 2 == 1) width++;
        QPixmap p(width, 100);
        QString colour = clip->getProperty("colour");
        switch (m_clip->clipType()) {
        case VIDEO:
        case AV:
        case SLIDESHOW:
        case PLAYLIST:
            connect(this, SIGNAL(updateThumb()), m_previewTimer, SLOT(start()));
        case IMAGE:
        case TEXT:
            p = KThumb::getFrame(m_producer, t.time().frames(m_fps), width, 100);
            break;
        case COLOR:
            colour = colour.replace(0, 2, "#");
            p.fill(QColor(colour.left(7)));
            break;
        default:
            p.fill(Qt::black);
        }
        if (!p.isNull()) {
            clip_thumb->setFixedWidth(p.width());
            clip_thumb->setFixedHeight(p.height());
            clip_thumb->setPixmap(p);
        }
        connect(marker_position, SIGNAL(textChanged(const QString &)), this, SIGNAL(updateThumb()));
    } else clip_thumb->setHidden(true);

    marker_position->setText(tc.getTimecode(t.time()));

    marker_comment->setText(t.comment());
    marker_comment->selectAll();
    marker_comment->setFocus();

    connect(position_up, SIGNAL(clicked()), this, SLOT(slotTimeUp()));
    connect(position_down, SIGNAL(clicked()), this, SLOT(slotTimeDown()));

    adjustSize();
}

MarkerDialog::~MarkerDialog()
{
    delete m_previewTimer;
    delete m_producer;
    delete m_profile;
}

void MarkerDialog::slotUpdateThumb()
{
    m_previewTimer->stop();
    int pos = m_tc.getFrameCount(marker_position->text());
    int width = 100.0 * m_dar;
    if (width % 2 == 1) width++;
    QPixmap p = KThumb::getFrame(m_producer, pos, width, 100);
    if (!p.isNull()) clip_thumb->setPixmap(p);
    else kDebug() << "!!!!!!!!!!!  ERROR CREATING THUMB";
}

void MarkerDialog::slotTimeUp()
{
    int duration = m_tc.getFrameCount(marker_position->text());
    if (m_clip && duration >= m_clip->duration().frames(m_fps)) return;
    duration ++;
    marker_position->setText(m_tc.getTimecode(GenTime(duration, m_fps)));
}

void MarkerDialog::slotTimeDown()
{
    int duration = m_tc.getFrameCount(marker_position->text());
    if (duration <= 0) return;
    duration --;
    marker_position->setText(m_tc.getTimecode(GenTime(duration, m_fps)));
}

CommentedTime MarkerDialog::newMarker()
{
    return CommentedTime(GenTime(m_tc.getFrameCount(marker_position->text()), m_fps), marker_comment->text());
}

void MarkerDialog::wheelEvent(QWheelEvent * event)
{
    if (marker_position->underMouse() || clip_thumb->underMouse()) {
        if (event->delta() > 0)
            slotTimeUp();
        else
            slotTimeDown();
    }
}

#include "markerdialog.moc"


