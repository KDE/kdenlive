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

class KdenliveSettingsDialog : public KConfigDialog
{
  Q_OBJECT
  
  public:
    KdenliveSettingsDialog(QWidget * parent = 0);

  private slots:
    void slotUpdateDisplay();

  private:
    Ui::ConfigEnv_UI* m_configEnv;
    Ui::ConfigMisc_UI* m_configMisc;
    QStringList m_mltProfilesList;
    QStringList m_customProfilesList;
    bool m_isCustomProfile;
};


#endif

