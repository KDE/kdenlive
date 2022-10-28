/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "dialogs/encodingprofilesdialog.h"
#include "guidecategories.h"

#include <QDialog>
#include <QPushButton>

#include "ui_projectsettings_ui.h"

class KdenliveDoc;
class ProfileWidget;

class ProjectSettings : public QDialog, public Ui::ProjectSettings_UI
{
    Q_OBJECT

public:
    ProjectSettings(KdenliveDoc *doc, QMap<QString, QString> metadata, QStringList lumas, int videotracks, int audiotracks, int audiochannels, const QString &projectPath,
                    bool readOnlyTracks, bool unsavedProject, QWidget *parent = nullptr);
    QString selectedProfile() const;
    QPair<int, int> tracks() const;
    const QStringList guidesCategories() const;
    int audioChannels() const;
    bool enableVideoThumbs() const;
    bool enableAudioThumbs() const;
    bool useProxy() const;
    bool useExternalProxy() const;
    bool generateProxy() const;
    bool docFolderAsStorageFolder() const;
    int proxyMinSize() const;
    bool generateImageProxy() const;
    int proxyImageMinSize() const;
    int proxyImageSize() const;
    int proxyResize() const;
    QString externalProxyParams() const;
    QString proxyParams() const;
    QString proxyExtension() const;
    QString previewParams() const;
    QString previewExtension() const;
    const QMap<QString, QString> metadata() const;
    static QStringList extractPlaylistUrls(const QString &path);
    static QStringList extractSlideshowUrls(const QString &url);
    const QString selectedPreview() const;
    const QString storageFolder() const;

public slots:
    void accept() override;

private slots:
    void slotUpdateButton(const QString &path);
    void slotUpdateFiles(bool cacheOnly = false);
    void slotDeleteUnused();
    /** @brief Export project data to text file. */
    void slotExportToText();
    /** @brief Update the displayed proxy parameters when user changes selection. */
    void slotUpdateProxyParams();
    /** @brief Insert a new metadata field. */
    void slotAddMetadataField();
    /** @brief Delete current metadata field. */
    void slotDeleteMetadataField();
    /** @brief Display proxy profiles management dialog. */
    void slotManageEncodingProfile();
    /** @brief Open editor for metadata item. */
    void slotEditMetadata(QTreeWidgetItem *, int);
    /** @brief Shows external proxy settings. */
    void slotExternalProxyChanged(bool enabled);
    void slotExternalProxyProfileChanged(const QString &);
    void setExternalProxyProfileData(const QString &profile);

private:
    QPushButton *m_buttonOk;
    ProfileWidget *m_pw;
    bool m_savedProject;
    QStringList m_lumas;
    QString m_proxyparameters;
    QString m_proxyextension;
    bool m_newProject;
    /** @brief List of all proxies urls in this project. */
    QStringList m_projectProxies;
    /** @brief List of all thumbnails used in this project. */
    QStringList m_projectThumbs;
    QDir m_previewDir;
    /** @brief Fill the proxy profiles combobox. */
    void loadProxyProfiles();
    /** @brief Fill the external proxy profiles combobox. */
    void loadExternalProxyProfiles();
    QString m_previewparams;
    QString m_previewextension;
    QString m_initialExternalProxyProfile;
    EncodingTimelinePreviewProfilesChooser *m_tlPreviewProfiles;
    GuideCategories *m_guidesCategories;
    /** @brief Fill the proxy profiles combobox. */
    // void loadPreviewProfiles();

signals:
    /** @brief User deleted proxies, so disable them in project. */
    void disableProxies();
    void disablePreview();
    void refreshProfiles();
};
