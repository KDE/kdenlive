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
#include "core/kdenlivesettings.h"

#include <QWheelEvent>
#include <KDebug>


MarkerDialog::MarkerDialog(DocClipBase *clip, const CommentedTime &t, const Timecode &tc, const QString &caption, QWidget * parent)
    : QDialog(parent)
    , m_producer(NULL)
    , m_profile(NULL)
    , m_clip(clip)
    , m_dar(4.0 / 3.0)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    setWindowTitle(caption);

    // Set  up categories
    for (int i = 0; i < 5; ++i) {
        marker_type->insertItem(i, i18n("Category %1", i));
        marker_type->setItemData(i, CommentedTime::markerColor(i), Qt::DecorationRole);
    }
    marker_type->setCurrentIndex(t.markerType());

    m_in = new TimecodeDisplay(tc, this);
    inputLayout->addWidget(m_in);
    m_in->setValue(t.time());

    m_previewTimer = new QTimer(this);

    if (m_clip != NULL) {
        m_in->setRange(0, m_clip->duration().frames(tc.fps()));
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
        int width = Kdenlive::DefaultThumbHeight * m_dar;
        if (width % 2 == 1) width++;
        QPixmap p(width, 100);
        QString colour = clip->getProperty("colour");
        int swidth = (int) (Kdenlive::DefaultThumbHeight * m_profile->width() / m_profile->height() + 0.5);

        switch (m_clip->clipType()) {
        case VIDEO:
        case AV:
        case SLIDESHOW:
        case PLAYLIST:
            connect(this, SIGNAL(updateThumb()), m_previewTimer, SLOT(start()));
        case IMAGE:
        case TEXT:
            m_image = KThumb::getFrame(m_producer, m_in->getValue(), swidth, width, Kdenlive::DefaultThumbHeight);
            p = QPixmap::fromImage(m_image);
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
        connect(m_in, SIGNAL(timeCodeEditingFinished()), this, SIGNAL(updateThumb()));
    } else {
        clip_thumb->setHidden(true);
        label_category->setHidden(true);
        marker_type->setHidden(true);
    }

    marker_comment->setText(t.comment());
    marker_comment->selectAll();
    marker_comment->setFocus();

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
    int pos = m_in->getValue();
    int width = 100.0 * m_dar;
    int swidth = (int) (100.0 * m_profile->width() / m_profile->height() + 0.5);
    if (width % 2 == 1)
        width++;

    m_image = KThumb::getFrame(m_producer, pos, swidth, width, 100);
    const QPixmap p = QPixmap::fromImage(m_image);
    if (!p.isNull())
        clip_thumb->setPixmap(p);
    else
        kDebug() << "!!!!!!!!!!!  ERROR CREATING THUMB";
}

QImage MarkerDialog::markerImage() const
{
    return m_image;
}

CommentedTime MarkerDialog::newMarker()
{
    KdenliveSettings::setDefault_marker_type(marker_type->currentIndex());
    return CommentedTime(m_in->gentime(), marker_comment->text(), marker_type->currentIndex());
}

#include "markerdialog.moc"


