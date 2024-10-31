/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipdurationdialog.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include <QFontDatabase>

#include <QWheelEvent>

ClipDurationDialog::ClipDurationDialog(std::shared_ptr<TimelineItemModel> timeline, std::unordered_set<int> clipIds, QWidget *parent)
    : QDialog(parent)
    , m_itemIds(clipIds)
    , m_fps(pCore->getCurrentFps())
    , m_model(timeline)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);

    // Calculate bounds for the resize
    int pos = -1;
    int duration = -1;
    int maxLength = -1;
    int itemIn = -1;
    int minimumOffset = -1;
    int maximumOffset = INT_MAX;
    // Use first item in timeline as reference item
    bool groupedSameClip = true;
    for (auto &id : clipIds) {
        if (!m_model->isItem(id)) {
            continue;
        }
        ItemInfo currentInfo = m_model->getItemInfo(id);
        int currentPos = currentInfo.position;
        if (pos == -1) {
            pos = currentPos;
            m_firstItemInfo = currentInfo;
        } else if (currentPos < pos) {
            groupedSameClip = false;
            pos = currentPos;
            m_firstItemInfo = currentInfo;
        } else if (currentPos > pos) {
            groupedSameClip = false;
        } else if (currentInfo.playTime != m_firstItemInfo.playTime || currentInfo.cropStart != m_firstItemInfo.cropStart) {
            groupedSameClip = false;
        }
        if (duration == -1) {
            duration = currentInfo.playTime;
        }
        int currentMaxFrame = -1;
        int currentMinFrame = -1;
        if (m_model->isClip(id)) {
            int currentIn = currentInfo.cropStart;
            if (itemIn == -1) {
                itemIn = currentIn;
            } else if (itemIn > -1 && itemIn != currentIn) {
                // Disable crop if the clips have different in points
                itemIn = -2;
            }
            const QString binId = m_model->getClipBinId(id);
            std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
            if (clip && clip->hasLimitedDuration()) {
                int currentMaxLength = clip->getProducerDuration();
                if (maxLength == -1 || currentMaxLength < maxLength) {
                    maxLength = currentMaxLength;
                }
            } else {
                itemIn = -2;
            }
            // Check if there is another selected clip appended
            int nextPos = currentPos + currentInfo.playTime;
            int nextId = m_model->getTrackById(currentInfo.trackId)->getClipByStartPosition(nextPos);
            if (nextId > -1 && clipIds.find(nextId) != clipIds.end()) {
                // Next clip is also selected, ignore max frame
            } else {
                currentMaxFrame = m_model->getTrackById(currentInfo.trackId)->getBlankSizeNearClip(id, true);
            }
            if (currentPos > 0) {
                int prevId = m_model->getTrackById(currentInfo.trackId)->getClipByPosition(currentPos - 1);
                if (prevId > -1 && clipIds.find(prevId) != clipIds.end()) {
                    // Previous clip is selected, ignore
                } else {
                    currentMinFrame = qMax(0, m_model->getTrackById(currentInfo.trackId)->getBlankSizeNearClip(id, false));
                }
            }
        } else if (m_model->isComposition(id)) {
            currentMaxFrame = m_model->getTrackById(currentInfo.trackId)->getBlankSizeNearComposition(id, true);
            currentMinFrame = qMax(0, m_model->getTrackById(currentInfo.trackId)->getBlankSizeNearComposition(id, false));
        } else if (m_model->isSubTitle(id)) {
            // TODO
        }
        if (currentMinFrame > -1) {
            if (minimumOffset == -1 || currentMinFrame < minimumOffset) {
                minimumOffset = currentMinFrame;
            }
        }
        if (currentMaxFrame < INT_MAX && currentMaxFrame < maximumOffset) {
            maximumOffset = currentMaxFrame;
        }
    }

    bool rippleMode = pCore->activeTool() == ToolType::RippleTool;

    QStringList infoMessageStrings;
    if (clipIds.size() > 1 && !groupedSameClip) {
        infoMessageStrings << i18n("Editing duration for %1 clips.", clipIds.size());
    } else {
        duration_for_all->setVisible(false);
    }
    if (rippleMode) {
        infoMessageStrings << i18n("Editing in Ripple mode.");
    }

    if (!infoMessageStrings.isEmpty()) {
        mode_info->setText(infoMessageStrings.join(QLatin1Char(' ')));
    } else {
        mode_info->setVisible(false);
    }

    if (itemIn < 0) {
        // Disable crop adjustement
        m_cropStart->setHidden(true);
        crop_label->hide();
        m_cropEnd->setHidden(true), end_label->hide();
    }

    if (minimumOffset > -1) {
        m_minPos = m_firstItemInfo.position - minimumOffset;
    }
    if (maximumOffset > -1 && maximumOffset < INT_MAX) {
        m_maxPos = maximumOffset + m_firstItemInfo.position + m_firstItemInfo.playTime;
    }
    m_pos->setValue(GenTime(pos, m_fps));
    m_dur->setValue(GenTime(duration, m_fps));
    m_cropStart->setValue(itemIn);
    m_cropEnd->setValue(GenTime(maxLength - (itemIn + duration), m_fps));

    connect(m_pos, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckStart);
    connect(m_dur, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckDuration);
    connect(m_cropStart, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckCrop);
    connect(m_cropEnd, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckEnd);

    adjustSize();
}

void ClipDurationDialog::accept()
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int newPos = m_pos->getValue();
    int newIn = m_cropStart->getValue();
    int newDuration = m_dur->getValue();
    bool result = true;
    int offset = newPos - m_firstItemInfo.position;
    bool moveFirst = offset < 0;
    if (offset == 0 && m_cropStart->isVisible()) {
        // Check if crop start change should cause a clip move before the resize
        if (newIn < m_firstItemInfo.cropStart) {
            offset = -(newIn - m_firstItemInfo.cropStart);
            moveFirst = true;
        }
    }
    bool rippleMode = pCore->activeTool() == ToolType::RippleTool;
    std::unordered_set<int> sel = m_model->getCurrentSelection();
    QList<int> sortedSelection;
    for (auto &s : sel) {
        sortedSelection << s;
    }
    std::unordered_set<int> children;
    for (int i : sel) {
        if (m_model->isGroup(i)) {
            std::unordered_set<int> subs = m_model->m_groups->getLeaves(i);
            children.insert(subs.begin(), subs.end());
        } else {
            children.insert(i);
        }
    }
    if (moveFirst) {
        // First move if required
        // Sort items by position
        std::sort(sortedSelection.begin(), sortedSelection.end(), [&](int a, int b) { return m_model->getItemPosition(a) < m_model->getItemPosition(b); });
        for (int i : sortedSelection) {
            if (m_model->isGroup(i)) {
                std::unordered_set<int> children = m_model->m_groups->getLeaves(i);
                result = m_model->requestGroupMove(*children.begin(), i, 0, offset, true, true, undo, redo);
            } else {
                ItemInfo currentInfo = m_model->getItemInfo(i);
                if (m_model->isClip(i)) {
                    result = m_model->requestClipMove(i, currentInfo.trackId, currentInfo.position + offset, true, true, true, true, undo, redo);
                } else if (m_model->isComposition(i)) {
                    result = m_model->requestCompositionMove(i, currentInfo.trackId, m_model->m_allCompositions[i]->getForcedTrack(),
                                                             currentInfo.position + offset, true, true, undo, redo);
                } else {
                    // TODO: subtitle
                }
            }
        }
        // Update reference
        m_firstItemInfo = m_model->getItemInfo(m_firstItemInfo.itemId);
        if (result) {
            if (newIn != m_firstItemInfo.cropStart && m_cropStart->isVisible()) {
                for (auto &c : children) {
                    ItemInfo currentInfo = m_model->getItemInfo(c);
                    int updatedDuration = currentInfo.playTime + (currentInfo.cropStart - newIn);
                    result = m_model->requestItemResize(c, updatedDuration, false, true, undo, redo);
                }
            }

            if (duration_for_all->isChecked() || !duration_for_all->isVisible()) {
                for (auto &c : children) {
                    bool isClip = m_model->isClip(c);
                    ItemInfo currentInfo = m_model->getItemInfo(c);
                    if (currentInfo.playTime == newDuration) {
                        continue;
                    }
                    // if (newDuration != m_firstItemInfo.playTime + (m_firstItemInfo.cropStart - newIn)) {
                    if (rippleMode && isClip) {
                        result = m_model->requestItemRippleResize(m_model, c, newDuration, true, true, !KdenliveSettings::lockedGuides(), undo, redo);
                    } else {
                        result = result && m_model->requestItemResize(c, newDuration, true, true, undo, redo);
                    }
                }
            }
        }
    } else {
        // perform resize first
        if (newIn != m_firstItemInfo.cropStart && m_cropStart->isVisible()) {
            int updatedDuration = m_firstItemInfo.playTime + (m_firstItemInfo.cropStart - newIn);
            for (auto &c : children) {
                bool isClip = m_model->isClip(c);
                if (rippleMode && isClip) {
                    result = m_model->requestItemRippleResize(m_model, c, updatedDuration, false, true, !KdenliveSettings::lockedGuides(), undo, redo);
                } else {
                    result = m_model->requestItemResize(c, updatedDuration, false, true, undo, redo);
                }
            }
        }
        m_firstItemInfo = m_model->getItemInfo(m_firstItemInfo.itemId);
        if ((newDuration != m_firstItemInfo.playTime && duration_for_all->isChecked()) || !duration_for_all->isVisible()) {
            for (auto &c : children) {
                ItemInfo currentInfo = m_model->getItemInfo(c);
                if (currentInfo.playTime == newDuration) {
                    continue;
                }
                bool isClip = m_model->isClip(c);
                if (rippleMode && isClip) {
                    result = m_model->requestItemRippleResize(m_model, c, newDuration, true, true, !KdenliveSettings::lockedGuides(), undo, redo);
                } else {
                    result = result && m_model->requestItemResize(c, newDuration, true, true, undo, redo);
                }
            }
        }
        if (m_firstItemInfo.position != newPos) {
            int offset = newPos - m_firstItemInfo.position;
            if (offset > 0) {
                std::sort(sortedSelection.begin(), sortedSelection.end(),
                          [&](int a, int b) { return m_model->getItemPosition(a) > m_model->getItemPosition(b); });
            } else {
                std::sort(sortedSelection.begin(), sortedSelection.end(),
                          [&](int a, int b) { return m_model->getItemPosition(a) < m_model->getItemPosition(b); });
            }
            for (int i : sortedSelection) {
                if (m_model->isGroup(i)) {
                    std::unordered_set<int> children = m_model->m_groups->getLeaves(i);
                    result = m_model->requestGroupMove(*children.begin(), i, 0, offset, true, true, undo, redo);
                } else {
                    ItemInfo currentInfo = m_model->getItemInfo(i);
                    if (m_model->isClip(i)) {
                        result = m_model->requestClipMove(i, currentInfo.trackId, currentInfo.position + offset, true, true, true, true, undo, redo);
                    } else if (m_model->isComposition(i)) {
                        result = m_model->requestCompositionMove(i, currentInfo.trackId, m_model->m_allCompositions[i]->getForcedTrack(),
                                                                 currentInfo.position + offset, true, true, undo, redo);
                    } else {
                        // TODO: subtitle
                    }
                }
            }
        }
    }
    if (result) {
        pCore->pushUndo(undo, redo, i18n("Edit item"));
    } else {
        undo();
    }
    QDialog::accept();
}

void ClipDurationDialog::slotCheckStart()
{
    int start = m_pos->getValue();
    int duration = m_dur->getValue();
    if (m_minPos > -1 && start < m_minPos) {
        m_pos->setValue(m_minPos);
    } else if (m_maxPos > -1 && start + duration > m_maxPos) {
        m_pos->setValue(m_maxPos - duration);
    }
}

void ClipDurationDialog::slotCheckDuration()
{
    int start = m_pos->getValue();
    int duration = m_dur->getValue();
    int cropStart = m_cropStart->getValue();
    int maxDuration;

    if (m_firstItemInfo.maxDuration < 0) {
        maxDuration = m_maxPos;
    } else {
        maxDuration = m_maxPos == -1 ? start + m_firstItemInfo.maxDuration - cropStart : qMin(m_maxPos, start + m_firstItemInfo.maxDuration - cropStart);
    }

    if (maxDuration > 0 && start + duration > maxDuration) {
        m_dur->blockSignals(true);
        m_dur->setValue(maxDuration - start);
        m_dur->blockSignals(false);
    }

    m_cropEnd->blockSignals(true);
    m_cropEnd->setValue(m_firstItemInfo.maxDuration - m_dur->getValue() - cropStart);
    m_cropEnd->blockSignals(false);
}

void ClipDurationDialog::slotCheckCrop()
{
    int duration = m_dur->getValue();
    int cropStart = m_cropStart->getValue();

    int diff = cropStart - m_firstItemInfo.cropStart;
    if ((diff > 0 && diff < duration) || diff < 0) {
        duration -= diff;
    } else {
        m_cropStart->setValue(m_firstItemInfo.cropStart);
        return;
    }

    if (m_firstItemInfo.maxDuration > 0 && cropStart + duration > m_firstItemInfo.maxDuration) {
        m_cropStart->setValue(m_firstItemInfo.cropStart);
    } else {
        m_dur->blockSignals(true);
        m_dur->setValue(duration);
        m_dur->blockSignals(false);
    }
}

void ClipDurationDialog::slotCheckEnd()
{
    int cropStart = m_cropStart->getValue();
    int cropEnd = m_cropEnd->getValue();
    int duration = m_firstItemInfo.maxDuration - cropEnd - cropStart;

    if (duration > 0) {
        m_dur->setValue(duration);
        slotCheckDuration();
    } else {
        m_cropEnd->blockSignals(true);
        m_cropEnd->setValue(m_firstItemInfo.maxDuration - m_dur->getValue() - cropStart);
        m_cropEnd->blockSignals(false);
    }
}

GenTime ClipDurationDialog::startPos() const
{
    return m_pos->gentime();
}

GenTime ClipDurationDialog::cropStart() const
{
    return m_cropStart->gentime();
}

GenTime ClipDurationDialog::duration() const
{
    return m_dur->gentime();
}
