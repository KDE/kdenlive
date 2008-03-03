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


#ifndef KDENLIVESETTINGSDIALOG_H
#define KDENLIVESETTINGSDIALOG_H

#include <QDialog>

#include <KConfigDialog>

#include "ui_configmisc_ui.h"
#include "ui_configenv_ui.h"
#include "ui_configdisplay_ui.h"

class KdenliveSettingsDialog : public KConfigDialog {
    Q_OBJECT

public:
    KdenliveSettingsDialog(QWidget * parent = 0);
    ~KdenliveSettingsDialog();

protected:
    virtual bool hasChanged();

private slots:
    void slotUpdateDisplay();

private:
    KPageWidgetItem *page1;
    KPageWidgetItem *page2;
    KPageWidgetItem *page3;
    Ui::ConfigEnv_UI m_configEnv;
    Ui::ConfigMisc_UI m_configMisc;
    Ui::ConfigDisplay_UI m_configDisplay;
    QStringList m_mltProfilesList;
    QStringList m_customProfilesList;
    bool m_isCustomProfile;
    QString m_defaulfProfile;

signals:
    void customChanged();

};


#endif

