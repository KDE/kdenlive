/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KConfigGroup>

#include "definitions.h"
#include "ui_manageencodingprofile_ui.h"

class KMessageWidget;

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
    EncodingProfilesChooser(QWidget *parent, EncodingProfilesManager::ProfileType, bool showAutoItem = false, const QString &configname = {},
                            bool native = true);
    QString currentExtension();
    QString currentParams();
    void hideMessage();
    /** @brief Only enable preview profiles with matching framerate */
    virtual void filterPreviewProfiles(const QString & /*profile*/);

public slots:
    void slotUpdateProfile(int ix);

protected:
    QComboBox *m_profilesCombo;
    EncodingProfilesManager::ProfileType m_type;
    bool m_showAutoItem;
    KMessageWidget *m_messageWidget;

private:
    QPlainTextEdit *m_info;

private slots:
    void slotManageEncodingProfile();
    virtual void loadEncodingProfiles();
};

class EncodingTimelinePreviewProfilesChooser : public EncodingProfilesChooser
{
    Q_OBJECT

public:
    EncodingTimelinePreviewProfilesChooser(QWidget *parent, bool showAutoItem = false, const QString &defaultValue = {}, bool selectFromConfig = false);
    /** @brief Only enable preview profiles with matching framerate */
    void filterPreviewProfiles(const QString &profile) override;

private slots:
    void loadEncodingProfiles() override;

signals:
    void currentIndexChanged();
};
