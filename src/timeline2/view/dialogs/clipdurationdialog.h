/*
 *   SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#ifndef CLIPDURATIONDIALOG_H
#define CLIPDURATIONDIALOG_H

#include "timecodedisplay.h"
#include "ui_clipdurationdialog_ui.h"

/** @class ClipDurationDialog
    @brief A dialog for modifying an item's (clip or transition) duration.
    @author Jean-Baptiste Mardelle
 */
class ClipDurationDialog : public QDialog, public Ui::ClipDurationDialog_UI
{
    Q_OBJECT

public:
    explicit ClipDurationDialog(int clipId, const Timecode &tc, int pos, int minpos, int in, int out, int length, int maxpos, QWidget *parent = nullptr);
    ~ClipDurationDialog() override;
    GenTime startPos() const;
    GenTime cropStart() const;
    GenTime duration() const;

private slots:
    void slotCheckDuration();
    void slotCheckStart();
    void slotCheckCrop();
    void slotCheckEnd();

private:
    int m_clipId;
    TimecodeDisplay *m_pos;
    TimecodeDisplay *m_dur;
    TimecodeDisplay *m_cropStart;
    TimecodeDisplay *m_cropEnd;
    GenTime m_min;
    GenTime m_max;
    GenTime m_crop;
    GenTime m_length;
};

#endif
