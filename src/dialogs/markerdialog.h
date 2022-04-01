/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_markerdialog_ui.h"

#include "definitions.h"
#include "utils/timecode.h"
#include "widgets/timecodedisplay.h"

class ClipController;

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
    explicit MarkerDialog(ClipController *clip, const CommentedTime &t, const Timecode &tc, const QString &caption, QWidget *parent = nullptr);
    ~MarkerDialog() override;

    CommentedTime newMarker();
    QImage markerImage() const;

private slots:
    void slotUpdateThumb();

private:
    ClipController *m_clip;
    TimecodeDisplay *m_in;
    QTimer *m_previewTimer;

signals:
    void updateThumb();
};
