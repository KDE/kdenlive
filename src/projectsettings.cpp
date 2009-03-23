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
#include <KDebug>

#include <QDir>

ProjectSettings::ProjectSettings(int videotracks, int audiotracks, const QString projectPath, bool readOnlyTracks, QWidget * parent): QDialog(parent), m_isCustomProfile(false) {
    m_view.setupUi(this);

    QMap <QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMapIterator<QString, QString> i(profilesInfo);
    while (i.hasNext()) {
        i.next();
        m_view.profiles_list->addItem(i.key(), i.value());
    }
    m_view.project_folder->setMode(KFile::Directory);
    m_view.project_folder->setPath(projectPath);
    QString currentProf = KdenliveSettings::current_profile();

    for (int i = 0; i < m_view.profiles_list->count(); i++) {
        if (m_view.profiles_list->itemData(i).toString() == currentProf) {
            m_view.profiles_list->setCurrentIndex(i);
            break;
        }
    }

    buttonOk = m_view.buttonBox->button(QDialogButtonBox::Ok);
    //buttonOk->setEnabled(false);
    m_view.audio_thumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_view.video_thumbs->setChecked(KdenliveSettings::videothumbnails());
    m_view.audio_tracks->setValue(audiotracks);
    m_view.video_tracks->setValue(videotracks);
    if (readOnlyTracks) {
        m_view.video_tracks->setEnabled(false);
        m_view.audio_tracks->setEnabled(false);
    }
    slotUpdateDisplay();
    connect(m_view.profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(m_view.project_folder, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButton(const QString &)));
}


void ProjectSettings::slotUpdateDisplay() {
    QString currentProfile = m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
    QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(currentProfile);
    m_view.p_size->setText(values.value("width") + 'x' + values.value("height"));
    m_view.p_fps->setText(values.value("frame_rate_num") + '/' + values.value("frame_rate_den"));
    m_view.p_aspect->setText(values.value("sample_aspect_num") + '/' + values.value("sample_aspect_den"));
    m_view.p_display->setText(values.value("display_aspect_num") + '/' + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0) m_view.p_progressive->setText(i18n("Interlaced"));
    else m_view.p_progressive->setText(i18n("Progressive"));
}

void ProjectSettings::slotUpdateButton(const QString &path) {
    if (path.isEmpty()) buttonOk->setEnabled(false);
    else buttonOk->setEnabled(true);
}

QString ProjectSettings::selectedProfile() const {
    return m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
}

KUrl ProjectSettings::selectedFolder() const {
    return m_view.project_folder->url();
}

QPoint ProjectSettings::tracks() {
    QPoint p;
    p.setX(m_view.video_tracks->value());
    p.setY(m_view.audio_tracks->value());
    return p;
}

bool ProjectSettings::enableVideoThumbs() const {
    return m_view.video_thumbs->isChecked();
}

bool ProjectSettings::enableAudioThumbs() const {
    return m_view.audio_thumbs->isChecked();
}

#include "projectsettings.moc"


