/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "multiplemarkerdialog.h"

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

MultipleMarkerDialog::MultipleMarkerDialog(ClipController *clip, const CommentedTime &t, const QString &caption, QWidget *parent)
    : QDialog(parent)
    , m_clip(clip)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(caption);

    // Set  up categories
    marker_category->setCurrentCategory(t.markerType());

    m_in->setValue(t.time());

    if (m_clip != nullptr) {
        m_in->setRange(0, m_clip->getFramePlaytime());
    }
    interval->setValue(GenTime(KdenliveSettings::multipleguidesinterval()));
    marker_comment->setText(t.comment());
    marker_comment->selectAll();
    marker_comment->setFocus();
    adjustSize();
}

MultipleMarkerDialog::~MultipleMarkerDialog() {}

CommentedTime MultipleMarkerDialog::startMarker()
{
    KdenliveSettings::setDefault_marker_type(marker_category->currentCategory());
    return CommentedTime(m_in->gentime(), marker_comment->text(), marker_category->currentCategory());
}

GenTime MultipleMarkerDialog::getInterval() const
{
    return interval->gentime();
}

int MultipleMarkerDialog::getOccurrences() const
{
    return occurrences->value();
}
