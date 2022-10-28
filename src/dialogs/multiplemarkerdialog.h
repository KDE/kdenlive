/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_multiplemarkerdialog_ui.h"

#include "definitions.h"
#include "utils/timecode.h"
#include "widgets/timecodedisplay.h"

class ClipController;

namespace Mlt {
}

/**
 * @class MultipleMarkerDialog
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */
class MultipleMarkerDialog : public QDialog, public Ui::MultipleMarkerDialog_UI
{
    Q_OBJECT

public:
    explicit MultipleMarkerDialog(ClipController *clip, const CommentedTime &t, const QString &caption, QWidget *parent = nullptr);
    ~MultipleMarkerDialog() override;

    CommentedTime startMarker();
    GenTime getInterval() const;
    int getOccurrences() const;

private:
    ClipController *m_clip;
};
