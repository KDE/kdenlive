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

#include "projectsettings.h"
#include "kdenlivesettings.h"
#include "profilesdialog.h"

#include <KStandardDirs>
#include <KMessageBox>
#include <KDebug>

#include <QDir>
#include <kmessagebox.h>

ProjectSettings::ProjectSettings(int videotracks, int audiotracks, const QString projectPath, bool readOnlyTracks, bool savedProject, QWidget * parent) :
        QDialog(parent), m_savedProject(savedProject)
{
    setupUi(this);

    QMap <QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMapIterator<QString, QString> i(profilesInfo);
    while (i.hasNext()) {
        i.next();
        profiles_list->addItem(i.key(), i.value());
    }
    project_folder->setMode(KFile::Directory);
    project_folder->setUrl(KUrl(projectPath));
    QString currentProf = KdenliveSettings::current_profile();

    for (int i = 0; i < profiles_list->count(); i++) {
        if (profiles_list->itemData(i).toString() == currentProf) {
            profiles_list->setCurrentIndex(i);
            break;
        }
    }

    m_buttonOk = buttonBox->button(QDialogButtonBox::Ok);
    //buttonOk->setEnabled(false);
    audio_thumbs->setChecked(KdenliveSettings::audiothumbnails());
    video_thumbs->setChecked(KdenliveSettings::videothumbnails());
    audio_tracks->setValue(audiotracks);
    video_tracks->setValue(videotracks);
    if (readOnlyTracks) {
        video_tracks->setEnabled(false);
        audio_tracks->setEnabled(false);
    }
    slotUpdateDisplay();
    connect(profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(project_folder, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButton(const QString &)));
}

void ProjectSettings::accept()
{
    if (!m_savedProject && selectedProfile() != KdenliveSettings::current_profile())
        if (KMessageBox::warningContinueCancel(this, i18n("Changing the profile of your project cannot be undone.\nIt is recommended to save your project before attempting this operation that might cause some corruption in transitions.\n Are you sure you want to proceed?"), i18n("Confirm profile change")) == KMessageBox::Cancel) return;
    QDialog::accept();
}

void ProjectSettings::slotUpdateDisplay()
{
    QString currentProfile = profiles_list->itemData(profiles_list->currentIndex()).toString();
    QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(currentProfile);
    p_size->setText(values.value("width") + 'x' + values.value("height"));
    p_fps->setText(values.value("frame_rate_num") + '/' + values.value("frame_rate_den"));
    p_aspect->setText(values.value("sample_aspect_num") + '/' + values.value("sample_aspect_den"));
    p_display->setText(values.value("display_aspect_num") + '/' + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0) p_progressive->setText(i18n("Interlaced"));
    else p_progressive->setText(i18n("Progressive"));
}

void ProjectSettings::slotUpdateButton(const QString &path)
{
    if (path.isEmpty()) m_buttonOk->setEnabled(false);
    else m_buttonOk->setEnabled(true);
}

QString ProjectSettings::selectedProfile() const
{
    return profiles_list->itemData(profiles_list->currentIndex()).toString();
}

KUrl ProjectSettings::selectedFolder() const
{
    return project_folder->url();
}

QPoint ProjectSettings::tracks()
{
    QPoint p;
    p.setX(video_tracks->value());
    p.setY(audio_tracks->value());
    return p;
}

bool ProjectSettings::enableVideoThumbs() const
{
    return video_thumbs->isChecked();
}

bool ProjectSettings::enableAudioThumbs() const
{
    return audio_thumbs->isChecked();
}

#include "projectsettings.moc"


