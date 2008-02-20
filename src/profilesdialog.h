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


#ifndef PROFILESDIALOG_H
#define PROFILESDIALOG_H

#include <QDialog>

#include "ui_profiledialog_ui.h"

class ProfilesDialog : public QDialog
{
  Q_OBJECT
  
  public:
    ProfilesDialog(QWidget * parent = 0);

  static QStringList getProfileNames();
  static QMap< QString, QString > getSettingsForProfile(const QString profileName);
  static QMap< QString, QString > getSettingsFromFile(const QString path);

  private slots:
    void slotUpdateDisplay();

  private:
    Ui::ProfilesDialog_UI m_view;
    QStringList m_mltProfilesList;
    QStringList m_customProfilesList;
    bool m_isCustomProfile;
};


#endif

