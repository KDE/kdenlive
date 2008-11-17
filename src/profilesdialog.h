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

#include "definitions.h"
#include "ui_profiledialog_ui.h"

class ProfilesDialog : public QDialog {
    Q_OBJECT

public:
    ProfilesDialog(QWidget * parent = 0);

    void fillList(const QString selectedProfile = QString());
    static QString getProfileDescription(QString name);
    static QMap< QString, QString > getSettingsForProfile(const QString profileName);
    static QMap< QString, QString > getSettingsFromFile(const QString path);
    static QString getPathFromDescription(const QString profileDesc);
    static MltVideoProfile getVideoProfile(QString name);
    static QMap <QString, QString> getProfilesInfo();

protected:
    virtual void closeEvent(QCloseEvent *event);

private slots:
    void slotUpdateDisplay();
    void slotCreateProfile();
    bool slotSaveProfile();
    void slotDeleteProfile();
    void slotSetDefaultProfile();
    void slotProfileEdited();
    virtual void accept();

private:
    Ui::ProfilesDialog_UI m_view;
    QStringList m_mltProfilesList;
    QStringList m_customProfilesList;
    int m_selectedProfileIndex;
    bool m_profileIsModified;
    bool m_isCustomProfile;
    void saveProfile(const QString path);
    bool askForSave();
};


#endif

