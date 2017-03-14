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

#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "mltcontroller/clipcontroller.h"

#include <QWheelEvent>
#include "kdenlive_debug.h"
#include <QTimer>
#include <QFontDatabase>

#include "klocalizedstring.h"

MarkerDialog::MarkerDialog(ClipController *clip, const CommentedTime &t, const Timecode &tc, const QString &caption, QWidget *parent)
    : QDialog(parent)
    , m_clip(clip)
    , m_dar(4.0 / 3.0)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
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

    if (m_clip != nullptr) {
        m_in->setRange(0, m_clip->getPlaytime().frames(tc.fps()));
        m_previewTimer->setInterval(500);
        connect(m_previewTimer, &QTimer::timeout, this, &MarkerDialog::slotUpdateThumb);
        m_dar = m_clip->dar();
        int width = Kdenlive::DefaultThumbHeight * m_dar;
        QPixmap p(width, Kdenlive::DefaultThumbHeight);
        p.fill(Qt::transparent);
        QString colour = clip->property(QStringLiteral("colour"));

        switch (m_clip->clipType()) {
        case Video:
        case AV:
        case SlideShow:
        case Playlist:
            m_previewTimer->start();
            connect(this, SIGNAL(updateThumb()), m_previewTimer, SLOT(start()));
            break;
        case Image:
        case Text:
        case QText:
            m_previewTimer->start();
            //p = m_clip->pixmap(m_in->getValue(), width, height);
            break;
        case Color:
            colour = colour.replace(0, 2, QLatin1Char('#'));
            p.fill(QColor(colour.left(7)));
            break;
        //UNKNOWN, AUDIO, VIRTUAL:
        default:
            p.fill(Qt::black);
        }

        if (!p.isNull()) {
            clip_thumb->setFixedWidth(p.width());
            clip_thumb->setFixedHeight(p.height());
            clip_thumb->setPixmap(p);
        }
        connect(m_in, &TimecodeDisplay::timeCodeEditingFinished, this, &MarkerDialog::updateThumb);
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
}

void MarkerDialog::slotUpdateThumb()
{
    m_previewTimer->stop();
    int pos = m_in->getValue();
    int width = Kdenlive::DefaultThumbHeight * m_dar;
    /*m_image = KThumb::getFrame(m_producer, pos, swidth, width, 100);
    const QPixmap p = QPixmap::fromImage(m_image);*/
    const QPixmap p = m_clip->pixmap(pos, width, Kdenlive::DefaultThumbHeight);
    if (!p.isNull()) {
        clip_thumb->setPixmap(p);
    } else {
        qCDebug(KDENLIVE_LOG) << "!!!!!!!!!!!  ERROR CREATING THUMB";
    }
}

QImage MarkerDialog::markerImage() const
{
    return clip_thumb->pixmap()->toImage();
}

CommentedTime MarkerDialog::newMarker()
{
    KdenliveSettings::setDefault_marker_type(marker_type->currentIndex());
    return CommentedTime(m_in->gentime(), marker_comment->text(), marker_type->currentIndex());
}

