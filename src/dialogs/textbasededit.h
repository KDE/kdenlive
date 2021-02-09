/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle                          *
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

#ifndef TEXTBASEDEDIT_H
#define TEXTBASEDEDIT_H

#include "ui_textbasededit_ui.h"
#include "definitions.h"

#include <QProcess>



/**
 * @class TextBasedEdit: Subtitle edit widget
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class TextBasedEdit : public QWidget, public Ui::TextBasedEdit_UI
{
    Q_OBJECT

public:
    explicit TextBasedEdit(QWidget *parent = nullptr);


private slots:
    void startRecognition();
    void slotProcessSpeech();
    void parseVoskDictionaries();
    void slotProcessSpeechStatus(int, QProcess::ExitStatus status);

private:
    std::unique_ptr<QProcess> m_speechJob;
    QString m_binId;
    QString m_sourceUrl;
    QAction *m_abortAction;
};

#endif
