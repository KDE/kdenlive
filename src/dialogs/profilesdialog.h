/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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
    void slotScanningChanged(int ix);
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
    void saveProfile(const QString &path);
    bool askForSave();
    void connectDialog();
    void showMessage(const QString &text = QString(), KMessageWidget::MessageType type = KMessageWidget::Warning);
};

#endif
