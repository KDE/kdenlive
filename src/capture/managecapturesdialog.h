/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef MANAGECAPTURESDIALOG_H
#define MANAGECAPTURESDIALOG_H

#include <QPushButton>

#include <QUrl>

#include "ui_managecaptures_ui.h"

class ManageCapturesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManageCapturesDialog(const QList<QUrl> &files, QWidget *parent = nullptr);
    ~ManageCapturesDialog() override;
    QList<QUrl> importFiles() const;

private slots:
    void slotRefreshButtons();
    void slotDeleteCurrent();
    void slotToggle();
    void slotCheckItemIcon();

private:
    Ui::ManageCaptures_UI m_view{};
    QPushButton *m_importButton;
};

#endif
