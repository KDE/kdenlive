/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "markerdialog.h"

#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include "utils/thumbnailcache.hpp"

#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QPushButton>
#include <QTimer>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrentRun>

#include "klocalizedstring.h"

MarkerDialog::MarkerDialog(ProjectClip *clip, const CommentedTime &t, const QString &caption, bool allowMultipleMarksers, QWidget *parent)
    : QDialog(parent)
    , m_clip(clip)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(caption);

    // Set  up categories
    marker_category->setCurrentCategory(t.markerType());
    clip_thumb->setFixedSize(pCore->thumbProfile().width(), pCore->thumbProfile().height());

    m_in->setValue(t.time());
    if (!allowMultipleMarksers) {
        multimarker_box->setVisible(false);
    }
    interval->setValue(GenTime(KdenliveSettings::multipleguidesinterval()));
    m_previewTimer = new QTimer(this);
    connect(&m_watcher, &QFutureWatcherBase::finished, this, [this] {
        const QImage thumb = m_watcher.result();
        if (!thumb.isNull()) {
            clip_thumb->setFixedSize(thumb.size());
            clip_thumb->setPixmap(QPixmap::fromImage(thumb));
        } else {
            qCDebug(KDENLIVE_LOG) << "!!!!!!!!!!!  ERROR CREATING THUMB";
        }
    });

    if (m_clip != nullptr) {
        m_in->setRange(0, m_clip->getFramePlaytime());
        m_previewTimer->setInterval(100);
        connect(m_previewTimer, &QTimer::timeout, this, &MarkerDialog::slotUpdateThumb);
        int width = int(200 * pCore->getCurrentDar());
        QPixmap p(width, 200);
        p.fill(Qt::transparent);
        switch (m_clip->clipType()) {
        case ClipType::Video:
        case ClipType::AV:
        case ClipType::SlideShow:
        case ClipType::Playlist:
        case ClipType::Timeline:
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
        m_in->setFrameOffset(pCore->currentTimelineOffset());
        clip_thumb->setHidden(true);
        label_category->setHidden(true);
    }

    marker_comment->setText(t.comment());
    marker_comment->selectAll();
    marker_comment->setFocus();

    buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(marker_category->count() > 0);
    adjustSize();
}

MarkerDialog::~MarkerDialog()
{
    delete m_previewTimer;
}

void MarkerDialog::slotUpdateThumb()
{
    m_previewTimer->stop();
    m_position = m_in->getValue();
    if (m_future.isRunning()) {
        m_future.cancel();
    }
    m_future = QtConcurrent::run(&ProjectClip::fetchPixmap, m_clip, m_position);
    m_watcher.setFuture(m_future);
}

QImage MarkerDialog::markerImage() const
{
    return clip_thumb->pixmap(Qt::ReturnByValue).toImage();
}

CommentedTime MarkerDialog::newMarker()
{
    KdenliveSettings::setDefault_marker_type(marker_category->currentCategory());
    return CommentedTime(m_in->gentime(), marker_comment->text(), marker_category->currentCategory());
}

void MarkerDialog::cacheThumbnail()
{
    if (!KdenliveSettings::guidesShowThumbs()) {
        return;
    }
    const QImage pix = markerImage();
    if (m_clip && !pix.isNull()) {
        ThumbnailCache::get()->storeThumbnail(m_clip->clipId(), m_position, pix, true);
    }
}

GenTime MarkerDialog::getInterval() const
{
    return interval->gentime();
}

int MarkerDialog::getOccurrences() const
{
    return occurrences->value();
}

bool MarkerDialog::addMultiMarker() const
{
    return multimarker_box->isExpanded();
}
