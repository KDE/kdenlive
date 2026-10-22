
/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <memory>

#include <QWidget>

class QTextEdit;
class ProfileTreeModel;
class ProfileFilter;
class TreeView;
class QTreeView;
class QComboBox;
class QLabel;

/** @class ProfilePreviewWidget
    @brief Small preview widget showing proportional shape of the selected video profile (landscape/portrait/square)
 */
class ProfilePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProfilePreviewWidget(QWidget *parent = nullptr);
    void setDimensions(int width, int height, int aspectNum = 0, int aspectDen = 0);
    void clear();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_width{0};
    int m_height{0};
    int m_aspectNum{0};
    int m_aspectDen{0};
    bool m_valid{false};
};

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

    ProfilePreviewWidget *m_previewWidget{nullptr};
    QLabel *m_previewLabel{nullptr};
    QTextEdit *m_descriptionPanel{nullptr};

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

Q_SIGNALS:
    void profileChanged();
};
