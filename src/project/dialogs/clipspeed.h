/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef CLIPSPEED_H
#define CLIPSPEED_H

#include "ui_clipspeed_ui.h"
#include <QDialog>

/**
 * @class ClipSpeed
 * @brief A dialog allowing to set a destination and a new speed to create an MLT file from a clip.
 */

class ClipSpeed : public QDialog
{
    Q_OBJECT

public:
    explicit ClipSpeed(const QUrl &destination, bool isDirectory, QWidget *parent = nullptr);
    QUrl selectedUrl() const;
    double speed() const;

private slots:
    void slotUpdateSlider(double);
    void slotUpdateSpeed(int);
    void adjustSpeed(QAction *a);

private:
    Ui::ClipSpeed_UI m_view;
};

#endif
