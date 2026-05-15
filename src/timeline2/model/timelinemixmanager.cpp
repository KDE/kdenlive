/*
SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinemixmanager.hpp"
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "trackmodel.hpp"
#include "undohelper.hpp"

TimelineMixManager::TimelineMixManager(std::shared_ptr<TimelineItemModel> timeline, QObject *parent)
    : QObject(parent)
    , m_timeline(timeline)
{
}

bool TimelineMixManager::plantMix(int tid, Mlt::Transition *t)
{
    if (m_timeline->getTrackById_const(tid)->hasClipStart(t->get_in())) {
        int a_track = t->get_a_track();
        int b_track = t->get_b_track();
        m_timeline->getTrackById_const(tid)->getTrackService()->plant_transition(*t, a_track, b_track);
        return m_timeline->getTrackById_const(tid)->loadMix(t);
    } else {
        qDebug() << "=== INVALID MIX FOUND AT: " << t->get_in() << " - " << t->get("mlt_service");
        return false;
    }
}

bool TimelineMixManager::resizeStartMix(int cid, int duration, bool singleResize)
{
    Q_ASSERT(m_timeline->isClip(cid));
    int tid = m_timeline->m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        std::pair<MixInfo, MixInfo> mixData = m_timeline->getTrackById_const(tid)->getMixInfo(cid);
        if (mixData.first.firstClipId > -1) {
            int clipToResize = mixData.first.firstClipId;
            Q_ASSERT(m_timeline->isClip(clipToResize));
            duration = qMin(duration, m_timeline->m_allClips.at(cid)->getPlaytime());
            int updatedDuration = m_timeline->m_allClips.at(cid)->getPosition() + duration - m_timeline->m_allClips[clipToResize]->getPosition();
            int result = m_timeline->requestItemResize(clipToResize, updatedDuration, true, true, 0, singleResize);
            return result > -1;
        }
    }
    return false;
}

int TimelineMixManager::getMixDuration(int cid)
{
    Q_ASSERT(m_timeline->isClip(cid));
    int tid = m_timeline->m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        if (m_timeline->getTrackById_const(tid)->hasStartMix(cid)) {
            return m_timeline->getTrackById_const(tid)->getMixDuration(cid);
        } else {
            // Mix is not yet inserted in m_timeline
            std::pair<int, int> mixInOut = getMixInOut(cid);
            return mixInOut.second - mixInOut.first;
        }
    }
    return 0;
}

std::pair<int, int> TimelineMixManager::getMixInOut(int cid)
{
    Q_ASSERT(m_timeline->isClip(cid));
    int tid = m_timeline->m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        MixInfo mixData = m_timeline->getTrackById_const(tid)->getMixInfo(cid).first;
        if (mixData.firstClipId > -1) {
            return {mixData.secondClipInOut.first, mixData.firstClipInOut.second};
        }
    }
    return {-1, -1};
}

int TimelineMixManager::getMixCutPos(int cid)
{
    Q_ASSERT(m_timeline->isClip(cid));
    return m_timeline->m_allClips.at(cid)->getMixCutPosition();
}

MixAlignment TimelineMixManager::getMixAlign(int cid)
{
    Q_ASSERT(m_timeline->isClip(cid));
    int tid = m_timeline->m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        int mixDuration = m_timeline->m_allClips.at(cid)->getMixDuration();
        int mixCutPos = m_timeline->m_allClips.at(cid)->getMixCutPosition();
        if (mixCutPos == 0) {
            return MixAlignment::AlignRight;
        } else if (mixCutPos == mixDuration) {
            return MixAlignment::AlignLeft;
        } else if (mixCutPos == mixDuration - mixDuration / 2) {
            return MixAlignment::AlignCenter;
        }
    }
    return MixAlignment::AlignNone;
}

void TimelineMixManager::requestResizeMix(int cid, int duration, MixAlignment align, int leftFrames)
{
    Q_ASSERT(m_timeline->isClip(cid));
    int tid = m_timeline->m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        MixInfo mixData = m_timeline->getTrackById_const(tid)->getMixInfo(cid).first;
        int clipToResize = mixData.firstClipId;
        if (clipToResize > -1) {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            // The mix cut position should never change through a resize operation
            int cutPos = m_timeline->m_allClips.at(clipToResize)->getPosition() + m_timeline->m_allClips.at(clipToResize)->getPlaytime() -
                         m_timeline->m_allClips.at(cid)->getMixCutPosition();
            int maxLengthLeft = m_timeline->m_allClips.at(clipToResize)->getMaxDuration();
            // Maximum space for expanding the right clip part
            int leftMax = maxLengthLeft > -1 ? (maxLengthLeft - 1 - m_timeline->m_allClips.at(clipToResize)->getOut()) : -1;
            // Maximum space available on the right clip
            int availableLeft = m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() -
                                (m_timeline->m_allClips.at(clipToResize)->getPosition() + m_timeline->m_allClips.at(clipToResize)->getPlaytime());
            if (leftMax == -1) {
                leftMax = availableLeft;
            } else {
                leftMax = qMin(leftMax, availableLeft);
            }

            int maxLengthRight = m_timeline->m_allClips.at(cid)->getMaxDuration();
            // maximum space to resize clip on the left
            int availableRight = m_timeline->m_allClips.at(cid)->getPosition() - m_timeline->m_allClips.at(clipToResize)->getPosition();
            int rightMax = maxLengthRight > -1 ? (m_timeline->m_allClips.at(cid)->getIn()) : -1;
            if (rightMax == -1) {
                rightMax = availableRight;
            } else {
                rightMax = qMin(rightMax, availableRight);
            }
            Fun adjust_mix_undo = [this, tid, cid, clipToResize, prevCut = m_timeline->m_allClips.at(cid)->getMixCutPosition(),
                                   prevDuration = m_timeline->m_allClips.at(cid)->getMixDuration()]() {
                m_timeline->getTrackById_const(tid)->setMixDuration(cid, prevDuration, prevCut);
                QModelIndex ix = m_timeline->makeClipIndexFromID(cid);
                Q_EMIT m_timeline->dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                QModelIndex ix2 = m_timeline->makeClipIndexFromID(clipToResize);
                Q_EMIT m_timeline->dataChanged(ix2, ix2, {TimelineModel::MixEndDurationRole});
                return true;
            };
            if (align == MixAlignment::AlignLeft) {
                // Adjust left clip
                int updatedDurationLeft = cutPos + duration - m_timeline->m_allClips.at(clipToResize)->getPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_timeline->m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                // Adjust right clip
                int updatedDurationRight = m_timeline->m_allClips.at(cid)->getPlaytime();
                if (cutPos != m_timeline->m_allClips.at(cid)->getPosition()) {
                    updatedDurationRight = m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() - cutPos;
                    if (rightMax > -1) {
                        updatedDurationRight = qMin(updatedDurationRight, m_timeline->m_allClips.at(cid)->getPlaytime() + rightMax);
                    }
                }
                int updatedDuration = m_timeline->m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft -
                                      (m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() - updatedDurationRight);
                if (updatedDuration < 1) {
                    //
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    // update mix widget
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                m_timeline->requestItemResize(clipToResize, updatedDurationLeft, true, true, undo, redo);
                if (m_timeline->m_allClips.at(cid)->getPlaytime() != updatedDurationRight) {
                    m_timeline->requestItemResize(cid, updatedDurationRight, false, true, undo, redo);
                }
                int updatedCutPosition = m_timeline->m_allClips.at(cid)->getPosition();
                if (updatedCutPosition != cutPos) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, clipToResize, updatedDuration]() {
                    m_timeline->getTrackById_const(tid)->setMixDuration(cid, updatedDuration, updatedDuration);
                    QModelIndex ix = m_timeline->makeClipIndexFromID(cid);
                    Q_EMIT m_timeline->dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    QModelIndex ix2 = m_timeline->makeClipIndexFromID(clipToResize);
                    Q_EMIT m_timeline->dataChanged(ix2, ix2, {TimelineModel::MixEndDurationRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            } else if (align == MixAlignment::AlignRight) {
                int updatedDurationRight = m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() - cutPos + duration;
                if (rightMax > -1) {
                    updatedDurationRight = qMin(updatedDurationRight, m_timeline->m_allClips.at(cid)->getPlaytime() + rightMax);
                }
                int updatedDurationLeft = cutPos - m_timeline->m_allClips.at(clipToResize)->getPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_timeline->m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                int updatedDuration = m_timeline->m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft -
                                      (m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() - updatedDurationRight);
                if (updatedDuration < 1) {
                    //
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                m_timeline->requestItemResize(cid, updatedDurationRight, false, true, undo, redo);
                m_timeline->requestItemResize(clipToResize, updatedDurationLeft, true, true, undo, redo);
                int updatedCutPosition = m_timeline->m_allClips.at(clipToResize)->getPosition() + m_timeline->m_allClips.at(clipToResize)->getPlaytime();
                if (updatedCutPosition != cutPos) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, clipToResize, updatedDuration]() {
                    m_timeline->getTrackById_const(tid)->setMixDuration(cid, updatedDuration, 0);
                    QModelIndex ix = m_timeline->makeClipIndexFromID(cid);
                    Q_EMIT m_timeline->dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    QModelIndex ix2 = m_timeline->makeClipIndexFromID(clipToResize);
                    Q_EMIT m_timeline->dataChanged(ix2, ix2, {TimelineModel::MixEndDurationRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            } else if (align == MixAlignment::AlignCenter) {
                int updatedDurationRight =
                    m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() - cutPos + duration / 2;
                if (rightMax > -1) {
                    updatedDurationRight = qMin(updatedDurationRight, m_timeline->m_allClips.at(cid)->getPlaytime() + rightMax);
                }
                int updatedDurationLeft = cutPos + (duration - duration / 2) - m_timeline->m_allClips.at(clipToResize)->getPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_timeline->m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                int updatedDuration = m_timeline->m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft -
                                      (m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() - updatedDurationRight);
                if (updatedDuration < 1) {
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                int deltaLeft = m_timeline->m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft - cutPos;
                int deltaRight =
                    cutPos - (m_timeline->m_allClips.at(cid)->getPosition() + m_timeline->m_allClips.at(cid)->getPlaytime() - updatedDurationRight);

                if (!m_timeline->requestItemResize(cid, updatedDurationRight, false, true, undo, redo)) {
                    qDebug() << ":::: ERROR RESIZING CID1\n\nAAAAAAAAAAAAAAAAAAAA";
                }
                if (deltaLeft > 0) {
                    if (!m_timeline->requestItemResize(clipToResize, updatedDurationLeft, true, true, undo, redo)) {
                        qDebug() << ":::: ERROR RESIZING clipToResize\n\nAAAAAAAAAAAAAAAAAAAA";
                    }
                }
                int mixCutPos = m_timeline->m_allClips.at(clipToResize)->getPosition() + m_timeline->m_allClips.at(clipToResize)->getPlaytime() - cutPos;
                if (mixCutPos > updatedDuration) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                if (qAbs(deltaLeft - deltaRight) > 2) {
                    // Mix not exactly centered
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, clipToResize, updatedDuration, mixCutPos]() {
                    m_timeline->getTrackById_const(tid)->setMixDuration(cid, updatedDuration, mixCutPos);
                    QModelIndex ix = m_timeline->makeClipIndexFromID(cid);
                    Q_EMIT m_timeline->dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    QModelIndex ix2 = m_timeline->makeClipIndexFromID(clipToResize);
                    Q_EMIT m_timeline->dataChanged(ix2, ix2, {TimelineModel::MixEndDurationRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            } else {
                // No alignment specified
                int updatedDurationRight;
                int updatedDurationLeft;
                if (leftFrames > -1) {
                    // A right frame offset was specified
                    updatedDurationLeft = qBound(0, leftFrames, duration);
                    updatedDurationRight = duration - updatedDurationLeft;
                } else {
                    updatedDurationRight = m_timeline->m_allClips.at(cid)->getMixCutPosition();
                    updatedDurationLeft = m_timeline->m_allClips.at(cid)->getMixDuration() - updatedDurationRight;
                    int currentDuration = m_timeline->m_allClips.at(cid)->getMixDuration();
                    if (qAbs(duration - currentDuration) == 1) {
                        if (duration < currentDuration) {
                            // We are reducing the duration
                            if (currentDuration % 2 == 0) {
                                updatedDurationRight--;
                                if (updatedDurationRight < 0) {
                                    updatedDurationRight = 0;
                                    updatedDurationLeft--;
                                }
                            } else {
                                updatedDurationLeft--;
                                if (updatedDurationLeft < 0) {
                                    updatedDurationLeft = 0;
                                    updatedDurationRight--;
                                }
                            }
                        } else {
                            // Increasing duration
                            if (currentDuration % 2 == 0) {
                                updatedDurationRight++;
                            } else {
                                updatedDurationLeft++;
                            }
                        }
                    } else {
                        double ratio = double(duration) / currentDuration;
                        updatedDurationRight *= ratio;
                        updatedDurationLeft = duration - updatedDurationRight;
                    }
                }
                if (updatedDurationLeft + updatedDurationRight < 1) {
                    //
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                updatedDurationLeft -= (m_timeline->m_allClips.at(cid)->getMixDuration() - m_timeline->m_allClips.at(cid)->getMixCutPosition());
                updatedDurationRight -= m_timeline->m_allClips.at(cid)->getMixCutPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_timeline->m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                if (rightMax > -1) {
                    updatedDurationRight = qMin(updatedDurationRight, m_timeline->m_allClips.at(cid)->getPlaytime() + rightMax);
                }
                if (updatedDurationLeft != 0) {
                    int updatedDurL = m_timeline->m_allClips.at(cid)->getPlaytime() + updatedDurationLeft;
                    m_timeline->requestItemResize(cid, updatedDurL, false, true, undo, redo);
                }
                if (updatedDurationRight != 0) {
                    int updatedDurR = m_timeline->m_allClips.at(clipToResize)->getPlaytime() + updatedDurationRight;
                    m_timeline->requestItemResize(clipToResize, updatedDurR, true, true, undo, redo);
                }
                int mixCutPos = m_timeline->m_allClips.at(clipToResize)->getPosition() + m_timeline->m_allClips.at(clipToResize)->getPlaytime() - cutPos;
                int updatedDuration = m_timeline->m_allClips.at(clipToResize)->getPosition() + m_timeline->m_allClips.at(clipToResize)->getPlaytime() -
                                      m_timeline->m_allClips.at(cid)->getPosition();
                if (mixCutPos > updatedDuration) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT m_timeline->selectedMixChanged(cid, m_timeline->getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, clipToResize, updatedDuration, mixCutPos]() {
                    m_timeline->getTrackById_const(tid)->setMixDuration(cid, updatedDuration, mixCutPos);
                    QModelIndex ix = m_timeline->makeClipIndexFromID(cid);
                    Q_EMIT m_timeline->dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    QModelIndex ix2 = m_timeline->makeClipIndexFromID(clipToResize);
                    Q_EMIT m_timeline->dataChanged(ix2, ix2, {TimelineModel::MixEndDurationRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            }
            pCore->pushUndo(undo, redo, i18n("Resize mix"));
        }
    }
}