/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "markerdialog.h"

#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "mltcontroller/clipcontroller.h"
#include "project/projectmanager.h"

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
    static std::array<QColor, 9> markerTypes = pCore->projectManager()->getGuideModel()->markerTypes;
    QPixmap pixmap(32, 32);
    for (uint i = 0; i < markerTypes.size(); ++i) {
        pixmap.fill(markerTypes[size_t(i)]);
        QIcon colorIcon(pixmap);
        marker_type->addItem(colorIcon, i18n("Category %1", i));
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
        int width = int(200 * pCore->getCurrentDar());
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
    return clip_thumb->pixmap(Qt::ReturnByValue).toImage();
}

CommentedTime MarkerDialog::newMarker()
{
    KdenliveSettings::setDefault_marker_type(marker_type->currentIndex());
    return CommentedTime(m_in->gentime(), marker_comment->text(), marker_type->currentIndex());
}
