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

#ifndef PROJECTSETTINGS_H
#define PROJECTSETTINGS_H

#include <QDialog>
#include <QPushButton>

#include "ui_projectsettings_ui.h"

class KdenliveDoc;
class ProfileWidget;

class ProjectSettings : public QDialog, public Ui::ProjectSettings_UI
{
    Q_OBJECT

public:
    ProjectSettings(KdenliveDoc *doc, QMap<QString, QString> metadata, const QStringList &lumas, int videotracks, int audiotracks, const QString &projectPath, bool readOnlyTracks, bool unsavedProject, QWidget *parent = nullptr);
    QString selectedProfile() const;
    QUrl selectedFolder() const;
    QPoint tracks() const;
    bool enableVideoThumbs() const;
    bool enableAudioThumbs() const;
    bool useProxy() const;
    bool generateProxy() const;
    int proxyMinSize() const;
    bool generateImageProxy() const;
    int proxyImageMinSize() const;
    QString proxyParams() const;
    QString proxyExtension() const;
    const QMap<QString, QString> metadata() const;
    static QStringList extractPlaylistUrls(const QString &path);
    static QStringList extractSlideshowUrls(const QString &url);
    const QString selectedPreview() const;
    const QString storageFolder() const;

public slots:
    void accept() Q_DECL_OVERRIDE;

private slots:
    void slotUpdateButton(const QString &path);
    void slotUpdateFiles(bool cacheOnly = false);
    void slotDeleteUnused();
    /** @brief Export project data to text file. */
    void slotExportToText();
    /** @brief Update the displayed proxy parameters when user changes selection. */
    void slotUpdateProxyParams();
    void slotUpdatePreviewParams();
    /** @brief Insert a new metadata field. */
    void slotAddMetadataField();
    /** @brief Delete current metadata field. */
    void slotDeleteMetadataField();
    /** @brief Display proxy profiles management dialog. */
    void slotManageEncodingProfile();
    void slotManagePreviewProfile();
    /** @brief Open editor for metadata item. */
    void slotEditMetadata(QTreeWidgetItem *, int);

private:
    QPushButton *m_buttonOk;
    ProfileWidget *m_pw;
    bool m_savedProject;
    QStringList m_lumas;
    QString m_proxyparameters;
    QString m_proxyextension;
    /** @brief List of all proxies urls in this project. */
    QStringList m_projectProxies;
    /** @brief List of all thumbnails used in this project. */
    QStringList m_projectThumbs;
    QDir m_previewDir;
    /** @brief Fill the proxy profiles combobox. */
    void loadProxyProfiles();
    QString m_previewparams;
    QString m_previewextension;
    /** @brief Fill the proxy profiles combobox. */
    void loadPreviewProfiles();

signals:
    /** @brief User deleted proxies, so disable them in project. */
    void disableProxies();
    void disablePreview();
    void refreshProfiles();
};

#endif

