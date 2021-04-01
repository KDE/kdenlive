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

#ifndef SUBTITLEEDIT_H
#define SUBTITLEEDIT_H

#include "ui_editsub_ui.h"

#include "definitions.h"


class SubtitleModel;
class TimecodeDisplay;

class ShiftEnterFilter : public QObject
{
    Q_OBJECT
public:
    ShiftEnterFilter(QObject *parent = nullptr);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
signals:
    void triggerUpdate();
};  


/**
 * @class SubtitleEdit: Subtitle edit widget
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */
class SubtitleEdit : public QWidget, public Ui::SubEdit_UI
{
    Q_OBJECT

public:
    explicit SubtitleEdit(QWidget *parent = nullptr);
    void setModel(std::shared_ptr<SubtitleModel> model);

public slots:
    void setActiveSubtitle(int id);

private slots:
    void updateSubtitle();
    void goToPrevious();
    void goToNext();

private:
    std::shared_ptr<SubtitleModel> m_model;
    int m_activeSub{-1};
    TimecodeDisplay *m_position;
    TimecodeDisplay *m_endPosition;
    TimecodeDisplay *m_duration;
    GenTime m_startPos;
    GenTime m_endPos;

signals:
    void addSubtitle();
    void cutSubtitle(int id, int cursorPos);
};

#endif
