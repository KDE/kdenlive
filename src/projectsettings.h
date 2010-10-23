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

#include "projectlist.h"
#include "ui_projectsettings_ui.h"

class ProjectSettings : public QDialog, public Ui::ProjectSettings_UI
{
    Q_OBJECT

public:
    ProjectSettings(ProjectList *projectlist, QStringList lumas, int videotracks, int audiotracks, const QString projectPath, bool readOnlyTracks, bool unsavedProject, QWidget * parent = 0);
    QString selectedProfile() const;
    KUrl selectedFolder() const;
    QPoint tracks();
    bool enableVideoThumbs() const;
    bool enableAudioThumbs() const;
    static QStringList extractPlaylistUrls(QString path);
    static QStringList extractSlideshowUrls(KUrl url);

public slots:
    virtual void accept();

private slots:
    void slotUpdateDisplay();
    void slotUpdateButton(const QString &path);
    void slotUpdateFiles(bool cacheOnly = false);
    void slotClearCache();
    void slotDeleteUnused();
    /** @brief Export project data to text file. */
    void slotExportToText();

private:
    QPushButton *m_buttonOk;
    bool m_savedProject;
    ProjectList *m_projectList;
    QStringList m_lumas;
};


#endif

