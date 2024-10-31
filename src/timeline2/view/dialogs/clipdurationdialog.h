/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_clipdurationdialog_ui.h"
#include "widgets/timecodedisplay.h"
#include <unordered_set>

class TimelineItemModel;

/** @class ClipDurationDialog
    @brief A dialog for modifying an item's (clip or transition) duration.
    @author Jean-Baptiste Mardelle
 */
class ClipDurationDialog : public QDialog, public Ui::ClipDurationDialog_UI
{
    Q_OBJECT

public:
    explicit ClipDurationDialog(std::shared_ptr<TimelineItemModel> timeline, std::unordered_set<int> clipIds, QWidget *parent = nullptr);
    GenTime startPos() const;
    GenTime cropStart() const;
    GenTime duration() const;

private Q_SLOTS:
    void slotCheckDuration();
    void slotCheckStart();
    void slotCheckCrop();
    void slotCheckEnd();
    void accept() override;

private:
    std::unordered_set<int> m_itemIds;
    double m_fps;
    int m_minPos{-1};
    int m_maxPos{-1};
    std::shared_ptr<TimelineItemModel> m_model;
    ItemInfo m_firstItemInfo;
};
