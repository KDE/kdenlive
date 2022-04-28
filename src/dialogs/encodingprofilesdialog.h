/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KConfigGroup>

#include "definitions.h"
#include "ui_manageencodingprofile_ui.h"

class EncodingProfilesManager
{

public:
    enum ProfileType {
        ProxyClips = 0,
        TimelinePreview = 1,
        V4LCapture = 2,
        ScreenCapture = 3,
        DecklinkCapture = 4
    };

    static QString configGroupName(ProfileType type);
};

class EncodingProfilesDialog : public QDialog, Ui::ManageEncodingProfile_UI
{
    Q_OBJECT

public:
    explicit EncodingProfilesDialog(EncodingProfilesManager::ProfileType profileType, QWidget *parent = nullptr);
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

class EncodingProfilesChooser : public QWidget
{
    Q_OBJECT

public:
    EncodingProfilesChooser(QWidget *parent, EncodingProfilesManager::ProfileType, bool showAutoItem = false, const QString &configname = {});
    QString currentExtension();
    QString currentParams();

public slots:
    void slotUpdateProfile(int ix);

private:
    QComboBox *m_profilesCombo;
    QPlainTextEdit *m_info;
    EncodingProfilesManager::ProfileType m_type;
    bool m_showAutoItem;

private slots:
    void slotManageEncodingProfile();
    void loadEncodingProfiles();

};
