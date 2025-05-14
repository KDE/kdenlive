/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_markerdialog_ui.h"

#include "definitions.h"
#include "utils/timecode.h"
#include "widgets/timecodedisplay.h"

#include <QFutureWatcher>

class ProjectClip;

namespace Mlt {
}

/**
 * @class MarkerDialog
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */
class MarkerDialog : public QDialog, public Ui::MarkerDialog_UI
{
    Q_OBJECT

public:
    explicit MarkerDialog(ProjectClip *clip, const CommentedTime &t, const QString &caption, bool allowMultipleMarksers = false, QWidget *parent = nullptr);
    ~MarkerDialog() override;

    CommentedTime newMarker();
    QImage markerImage() const;
    bool addMultiMarker() const;
    GenTime getInterval() const;
    int getOccurrences() const;
    /** @brief cache marker thumbnail */
    void cacheThumbnail();

private Q_SLOTS:
    void slotUpdateThumb();

private:
    ProjectClip *m_clip;
    int m_position{0};
    QTimer *m_previewTimer;
    QFuture<QImage> m_future;
    QFutureWatcher<QImage> m_watcher;

Q_SIGNALS:
    void updateThumb();
};
