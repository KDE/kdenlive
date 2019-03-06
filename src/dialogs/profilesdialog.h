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

#include "definitions.h"
#include "ui_profiledialog_ui.h"
#include <mlt++/Mlt.h>

class KMessageWidget;

class ProfilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProfilesDialog(const QString &profileDescription = QString(), QWidget *parent = nullptr);
    /** @brief Using this constructor, the dialog only allows editing one profile. */
    explicit ProfilesDialog(const QString &profilePath, bool, QWidget *parent = nullptr);
    void fillList(const QString &selectedProfile = QString());
    bool profileTreeChanged() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void slotUpdateDisplay(QString currentProfilePath = QString());
    void slotCreateProfile();
    bool slotSaveProfile();
    void slotDeleteProfile();
    void slotSetDefaultProfile();
    void slotProfileEdited();
    /** @brief Make sure the profile's width is always a multiple of 8 */
    void slotAdjustWidth();
    /** @brief Make sure the profile's height is always a multiple of 2 */
    void slotAdjustHeight();
    void accept() override;
    void reject() override;

private:
    Ui::ProfilesDialog_UI m_view;
    int m_selectedProfileIndex;
    bool m_profileIsModified{false};
    bool m_isCustomProfile{false};
    /** @brief If we are in single profile editing, should contain the path for this profile. */
    QString m_customProfilePath;
    /** @brief True if a profile was saved / deleted and profile tree requires a reload. */
    bool m_profilesChanged{false};
    KMessageWidget *m_infoMessage;
    void saveProfile(const QString &path);
    bool askForSave();
    void connectDialog();
};

#endif
