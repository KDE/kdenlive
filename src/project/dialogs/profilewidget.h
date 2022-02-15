
/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef PROFILEWIDGET_H
#define PROFILEWIDGET_H

#include "dialogs/profilesdialog.h"
#include <memory>

#include <QWidget>

class QTextEdit;
class ProfileTreeModel;
class ProfileFilter;
class TreeView;
class QTreeView;

/** @class ProfileWidget
    @brief Provides interface to choose and filter profiles
    @author Jean-Baptiste Mardelle, Nicolas Carion
 */
class ProfileWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProfileWidget(QWidget *parent = nullptr);
    ~ProfileWidget() override;
    void loadProfile(const QString &profile);
    const QString selectedProfile() const;

private:
    /** @brief currently selected's profile path */
    QString m_currentProfile;
    QString m_lastValidProfile;
    QString m_originalProfile;
    void slotUpdateInfoDisplay();

    QComboBox *m_fpsFilt;
    QComboBox *m_scanningFilt;

    QTreeView *m_treeView;
    std::shared_ptr<ProfileTreeModel> m_treeModel;
    ProfileFilter *m_filter;

    QTextEdit *m_descriptionPanel;

    /** @brief Open project profile management dialog. */
    void slotEditProfiles();

    /** @brief Manage a change in the selection */
    void slotChangeSelection(const QModelIndex &current, const QModelIndex &previous);
    /** @brief Fill the description of the profile.
       @param profile_path is the path to the profile
    */
    void fillDescriptionPanel(const QString &profile_path);

    /** @brief Select the profile with given path. Returns true on success */
    bool trySelectProfile(const QString &profile);

    /** @brief Slot to be called whenever filtering changes */
    void slotFilterChanged();

    /** @brief Reload available fps values */
    void refreshFpsCombo();

signals:
    void profileChanged();
};

#endif
