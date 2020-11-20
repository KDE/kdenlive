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
        QKeyEvent *keyEvent = static_cast <QKeyEvent*> (event);
        if((keyEvent->modifiers() & Qt::ShiftModifier) && ((keyEvent->key() == Qt::Key_Enter) || (keyEvent->key() == Qt::Key_Return))) {
            emit triggerUpdate();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}


SubtitleEdit::SubtitleEdit(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_activeSub(-1)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    buttonApply->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));
    buttonAdd->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    auto *keyFilter = new ShiftEnterFilter(this);
    subText->installEventFilter(keyFilter);
    connect(keyFilter, &ShiftEnterFilter::triggerUpdate, this, &SubtitleEdit::updateSubtitle);
    connect(subText, &QPlainTextEdit::textChanged, [this]() {
        buttonApply->setEnabled(true);
    });
    
    m_position = new TimecodeDisplay(pCore->timecode(), this);
    m_endPosition = new TimecodeDisplay(pCore->timecode(), this);
    m_duration = new TimecodeDisplay(pCore->timecode(), this);
    m_position->setEnabled(false);
    m_endPosition->setEnabled(false);
    m_duration->setEnabled(false);

    position_box->addWidget(m_position);
    end_box->addWidget(m_endPosition);
    duration_box->addWidget(m_duration);
    connect(m_position, &TimecodeDisplay::timeCodeEditingFinished, [this] (int value) {
        if (buttonApply->isEnabled()) {
            updateSubtitle();
        }
        GenTime duration = m_endPos - GenTime(value, pCore->getCurrentFps());
        m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), false);
    });
    connect(m_endPosition, &TimecodeDisplay::timeCodeEditingFinished, [this] (int value) {
        if (buttonApply->isEnabled()) {
            updateSubtitle();
        }
        GenTime duration = GenTime(value, pCore->getCurrentFps()) - m_startPos;
        m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), true);
    });
    connect(m_duration, &TimecodeDisplay::timeCodeEditingFinished, [this] (int value) {
        if (buttonApply->isEnabled()) {
            updateSubtitle();
        }
        m_model->requestResize(m_activeSub, value, true);
    });
    connect(buttonAdd, &QToolButton::clicked, this, &SubtitleEdit::addSubtitle);
    connect(buttonApply, &QToolButton::clicked, this, &SubtitleEdit::updateSubtitle);
    connect(buttonPrev, &QToolButton::clicked, this, &SubtitleEdit::goToPrevious);
    connect(buttonNext, &QToolButton::clicked, this, &SubtitleEdit::goToNext);
}

void SubtitleEdit::setModel(std::shared_ptr<SubtitleModel> model)
{
    m_model = model;
    m_activeSub = -1;
    subText->setEnabled(false);
    buttonApply->setEnabled(false);
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
    if (m_activeSub > -1 && m_model) {
        m_model->setText(m_activeSub, subText->toPlainText());
    }
}

void SubtitleEdit::setActiveSubtitle(int id)
{
    m_activeSub = id;
    if (m_model && id > -1) {
        subText->setEnabled(true);
        buttonApply->setEnabled(false);
        QSignalBlocker bk(subText);
        m_position->setEnabled(true);
        m_endPosition->setEnabled(true);
        m_duration->setEnabled(true);
        /*QSignalBlocker bk2(m_position);
        QSignalBlocker bk3(m_endPosition);
        QSignalBlocker bk4(m_duration);*/
        subText->setPlainText(m_model->getText(id));
        m_startPos = m_model->getStartPosForId(id);
        GenTime duration = GenTime(m_model->getSubtitlePlaytime(id), pCore->getCurrentFps());
        m_endPos = m_startPos + duration;
        m_position->setValue(m_startPos);
        m_endPosition->setValue(m_endPos);
        m_duration->setValue(duration);
    } else {
        subText->setEnabled(false);
        buttonApply->setEnabled(false);
        m_position->setEnabled(false);
        m_endPosition->setEnabled(false);
        m_duration->setEnabled(false);
        QSignalBlocker bk(subText);
        subText->clear();
    }
}

void SubtitleEdit::goToPrevious()
{
    if (m_model) {
        int id = m_model->getPreviousSub(m_activeSub);
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
        int id = m_model->getNextSub(m_activeSub);
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
