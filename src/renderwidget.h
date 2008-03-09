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


#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <QDialog>
#include <QPushButton>

#include "ui_renderwidget_ui.h"

class RenderWidget : public QDialog {
    Q_OBJECT

public:
    RenderWidget(QWidget * parent = 0);

private slots:
    void slotUpdateButtons();
    void slotExport();

private:
    Ui::RenderWidget_UI m_view;

signals:
    void doRender(const QString&, const QStringList &, bool, bool);
};


#endif

