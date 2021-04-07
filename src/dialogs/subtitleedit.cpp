/***************************************************************************
 *   Copyright (C) 2020 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "subtitleedit.h"
#include "bin/model/subtitlemodel.hpp"
#include "monitor/monitor.h"

#include "core.h"
#include "kdenlivesettings.h"
#include "timecodedisplay.h"

#include "klocalizedstring.h"
#include "QTextEdit"

#include <QEvent>
#include <QKeyEvent>
#include <QToolButton>

ShiftEnterFilter::ShiftEnterFilter(QObject *parent)
    : QObject(parent)
{}

bool ShiftEnterFilter::eventFilter(QObject *obj, QEvent *event)
{
    if(event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast <QKeyEvent*> (event);
        if((keyEvent->modifiers() & Qt::ShiftModifier) && ((keyEvent->key() == Qt::Key_Enter) || (keyEvent->key() == Qt::Key_Return))) {
            emit triggerUpdate();
            return true;
        }
    }
    if (event->type() == QEvent::FocusOut)
    {
        emit triggerUpdate();
        return true;
    }
    return QObject::eventFilter(obj, event);
}


SubtitleEdit::SubtitleEdit(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    buttonApply->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));
    buttonAdd->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    buttonCut->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
    buttonIn->setIcon(QIcon::fromTheme(QStringLiteral("zone-in")));
    buttonOut->setIcon(QIcon::fromTheme(QStringLiteral("zone-out")));
    buttonDelete->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    buttonLock->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-lock")));
    auto *keyFilter = new ShiftEnterFilter(this);
    subText->installEventFilter(keyFilter);
    connect(keyFilter, &ShiftEnterFilter::triggerUpdate, this, &SubtitleEdit::updateSubtitle);
    connect(subText, &KTextEdit::textChanged, [this]() {
        if (m_activeSub > -1) {
            buttonApply->setEnabled(true);
        }
    });
    connect(subText, &KTextEdit::cursorPositionChanged, [this]() {
        if (m_activeSub > -1) {
            buttonCut->setEnabled(true);
        }
    });
    
    m_position = new TimecodeDisplay(pCore->timecode(), this);
    m_endPosition = new TimecodeDisplay(pCore->timecode(), this);
    m_duration = new TimecodeDisplay(pCore->timecode(), this);
    frame_position->setEnabled(false);
    buttonDelete->setEnabled(false);

    position_box->addWidget(m_position);
    auto *spacer = new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    position_box->addSpacerItem(spacer);
    spacer = new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    end_box->addWidget(m_endPosition);
    end_box->addSpacerItem(spacer);
    duration_box->addWidget(m_duration);
    spacer = new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    duration_box->addSpacerItem(spacer);
    connect(m_position, &TimecodeDisplay::timeCodeEditingFinished, [this] (int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, GenTime(value, pCore->getCurrentFps()));
        } else {
            GenTime duration = m_endPos - GenTime(value, pCore->getCurrentFps());
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), false);
        }
    });
    connect(m_endPosition, &TimecodeDisplay::timeCodeEditingFinished, [this] (int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, GenTime(value, pCore->getCurrentFps()) - (m_endPos - m_startPos));
        } else {
            GenTime duration = GenTime(value, pCore->getCurrentFps()) - m_startPos;
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), true);
        }
    });
    connect(m_duration, &TimecodeDisplay::timeCodeEditingFinished, [this] (int value) {
        updateSubtitle();
        m_model->requestResize(m_activeSub, value, true);
    });
    connect(buttonAdd, &QToolButton::clicked, this, [this]() {
        emit addSubtitle(subText->toPlainText());
    });
    connect(buttonCut, &QToolButton::clicked, this, [this]() {
        if (m_activeSub > -1 && subText->hasFocus()) {
            int pos = subText->textCursor().position();
            updateSubtitle();
            emit cutSubtitle(m_activeSub, pos);
        }
    });
    connect(buttonApply, &QToolButton::clicked, this, &SubtitleEdit::updateSubtitle);
    connect(buttonPrev, &QToolButton::clicked, this, &SubtitleEdit::goToPrevious);
    connect(buttonNext, &QToolButton::clicked, this, &SubtitleEdit::goToNext);
    connect(buttonIn, &QToolButton::clicked, []() {
        pCore->triggerAction(QStringLiteral("resize_timeline_clip_start"));
    });
    connect(buttonOut, &QToolButton::clicked, []() {
        pCore->triggerAction(QStringLiteral("resize_timeline_clip_end"));
    });
    connect(buttonDelete, &QToolButton::clicked, []() {
        pCore->triggerAction(QStringLiteral("delete_timeline_clip"));
    });
    buttonNext->setToolTip(i18n("Go to next subtitle"));
    buttonPrev->setToolTip(i18n("Go to previous subtitle"));
    buttonAdd->setToolTip(i18n("Add subtitle"));
    buttonCut->setToolTip(i18n("Split subtitle at cursor position"));
    buttonApply->setToolTip(i18n("Update subtitle text"));
    buttonDelete->setToolTip(i18n("Delete subtitle"));
}

void SubtitleEdit::setModel(std::shared_ptr<SubtitleModel> model)
{
    m_model = model;
    m_activeSub = -1;
    buttonApply->setEnabled(false);
    buttonCut->setEnabled(false);
    if (m_model == nullptr) {
        QSignalBlocker bk(subText);
        subText->clear();
    } else {
        connect(m_model.get(), &SubtitleModel::dataChanged, [this](const QModelIndex &start, const QModelIndex &, const QVector <int>&roles) {
            if (m_activeSub > -1 && start.row() == m_model->getRowForId(m_activeSub)) {
                if (roles.contains(SubtitleModel::SubtitleRole) || roles.contains(SubtitleModel::StartFrameRole) || roles.contains(SubtitleModel::EndFrameRole)) {
                    setActiveSubtitle(m_activeSub);
                }
            }
        });
    }
}

void SubtitleEdit::updateSubtitle()
{
    if (!buttonApply->isEnabled()) {
        return;
    }
    buttonApply->setEnabled(false);
    if (m_activeSub > -1 && m_model) {
        QString txt = subText->toPlainText().trimmed();
        txt.replace(QLatin1String("\n\n"), QStringLiteral("\n"));
        m_model->setText(m_activeSub, txt);
    }
}

void SubtitleEdit::setActiveSubtitle(int id)
{
    m_activeSub = id;
    buttonApply->setEnabled(false);
    buttonCut->setEnabled(false);
    if (m_model && id > -1) {
        subText->setEnabled(true);
        QSignalBlocker bk(subText);
        frame_position->setEnabled(true);
        buttonDelete->setEnabled(true);
        QSignalBlocker bk2(m_position);
        QSignalBlocker bk3(m_endPosition);
        QSignalBlocker bk4(m_duration);
        subText->setPlainText(m_model->getText(id));
        m_startPos = m_model->getStartPosForId(id);
        GenTime duration = GenTime(m_model->getSubtitlePlaytime(id), pCore->getCurrentFps());
        m_endPos = m_startPos + duration;
        m_position->setValue(m_startPos);
        m_endPosition->setValue(m_endPos);
        m_duration->setValue(duration);
        m_position->setEnabled(true);
        m_endPosition->setEnabled(true);
        m_duration->setEnabled(true);
    } else {
        m_position->setEnabled(false);
        m_endPosition->setEnabled(false);
        m_duration->setEnabled(false);
        frame_position->setEnabled(false);
        buttonDelete->setEnabled(false);
        QSignalBlocker bk(subText);
        subText->clear();
    }
}

void SubtitleEdit::goToPrevious()
{
    if (m_model) {
        int id = -1;
        if (m_activeSub > -1) {
            id = m_model->getPreviousSub(m_activeSub);
        } else {
            // Start from timeline cursor position
            int cursorPos = pCore->getTimelinePosition();
            std::unordered_set<int> sids = m_model->getItemsInRange(cursorPos, cursorPos);
            if (sids.empty()) {
                sids = m_model->getItemsInRange(0, cursorPos);
                for (int s : sids) {
                    if (id == -1 || m_model->getSubtitleEnd(s) > m_model->getSubtitleEnd(id)) {
                        id = s;
                    }
                }
            } else {
                id = m_model->getPreviousSub(*sids.begin());
            }
        }
        if (id > -1) {
            if (buttonApply->isEnabled()) {
                updateSubtitle();
            }
            GenTime prev = m_model->getStartPosForId(id);
            pCore->getMonitor(Kdenlive::ProjectMonitor)->requestSeek(prev.frames(pCore->getCurrentFps()));
            pCore->selectTimelineItem(id);
        }
    }
}

void SubtitleEdit::goToNext()
{
    if (m_model) {
        int id = -1;
        if (m_activeSub > -1) {
             id = m_model->getNextSub(m_activeSub);
        } else {
            // Start from timeline cursor position
            int cursorPos = pCore->getTimelinePosition();
            std::unordered_set<int> sids = m_model->getItemsInRange(cursorPos, cursorPos);
            if (sids.empty()) {
                sids = m_model->getItemsInRange(cursorPos, -1);
                for (int s : sids) {
                    if (id == -1 || m_model->getStartPosForId(s) < m_model->getStartPosForId(id)) {
                        id = s;
                    }
                }
            } else {
                id = m_model->getNextSub(*sids.begin());
            }
        }
        if (id > -1) {
            if (buttonApply->isEnabled()) {
                updateSubtitle();
            }
            GenTime prev = m_model->getStartPosForId(id);
            pCore->getMonitor(Kdenlive::ProjectMonitor)->requestSeek(prev.frames(pCore->getCurrentFps()));
            pCore->selectTimelineItem(id);
        }
    }
}
