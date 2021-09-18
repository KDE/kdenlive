/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle (jb@kdenlive.org)

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef ENCODINGPROFILESDIALOG_H
#define ENCODINGPROFILESDIALOG_H

#include <KConfigGroup>

#include "definitions.h"
#include "ui_manageencodingprofile_ui.h"

class EncodingProfilesDialog : public QDialog, Ui::ManageEncodingProfile_UI
{
    Q_OBJECT

public:
    explicit EncodingProfilesDialog(int profileType, QWidget *parent = nullptr);
    ~EncodingProfilesDialog() override;

private slots:
    void slotLoadProfiles();
    void slotShowParams();
    void slotDeleteProfile();
    void slotAddProfile();
    void slotEditProfile();

private:
    KConfig *m_configFile;
    KConfigGroup *m_configGroup;
};

#endif
