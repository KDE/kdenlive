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

#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "mltcontroller/clipcontroller.h"

#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QTimer>
#include <QWheelEvent>

#include "klocalizedstring.h"

MarkerDialog::MarkerDialog(ClipController *clip, const CommentedTime &t, const Timecode &tc, const QString &caption, QWidget *parent)
    : QDialog(parent)
    , m_clip(clip)
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
        m_in->setRange(0, m_clip->getFramePlaytime());
        m_previewTimer->setInterval(100);
        connect(m_previewTimer, &QTimer::timeout, this, &MarkerDialog::slotUpdateThumb);
        int width = 200 * pCore->getCurrentDar();
        QPixmap p(width, 200);
        p.fill(Qt::transparent);
        switch (m_clip->clipType()) {
        case ClipType::Video:
        case ClipType::AV:
        case ClipType::SlideShow:
        case ClipType::Playlist:
            QTimer::singleShot(0, this, &MarkerDialog::slotUpdateThumb);
            connect(this, &MarkerDialog::updateThumb, m_previewTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
            break;
        case ClipType::Image:
        case ClipType::Text:
        case ClipType::QText:
        case ClipType::Color:
            QTimer::singleShot(0, this, &MarkerDialog::slotUpdateThumb);
            // p = m_clip->pixmap(m_in->getValue(), width, height);
            break;
        // UNKNOWN, AUDIO, VIRTUAL:
        default:
            p.fill(Qt::black);
        }

        if (!p.isNull()) {
            clip_thumb->setScaledContents(true);
            clip_thumb->setPixmap(p);
        }
        connect(m_in, &TimecodeDisplay::timeCodeEditingFinished, this, &MarkerDialog::updateThumb);
    } else {
        clip_thumb->setHidden(true);
        label_category->setHidden(true);
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
    const QPixmap p = m_clip->pixmap(pos);
    if (!p.isNull()) {
        clip_thumb->setFixedSize(p.width(), p.height());
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
