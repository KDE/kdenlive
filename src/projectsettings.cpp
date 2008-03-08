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

#include <QDir>

#include <KStandardDirs>
#include <KDebug>

#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "projectsettings.h"

ProjectSettings::ProjectSettings(QWidget * parent): QDialog(parent), m_isCustomProfile(false) {
    m_view.setupUi(this);

    QStringList profilesNames = ProfilesDialog::getProfileNames();
    m_view.profiles_list->addItems(profilesNames);
    m_view.project_folder->setMode(KFile::Directory);
    QString defaulfProf = ProfilesDialog::getSettingsFromFile(KdenliveSettings::current_profile()).value("description");
    if (profilesNames.contains(defaulfProf)) m_view.profiles_list->setCurrentItem(defaulfProf);
    buttonOk = m_view.buttonBox->button(QDialogButtonBox::Ok);
    //buttonOk->setEnabled(false);
    m_view.audio_thumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_view.video_thumbs->setChecked(KdenliveSettings::videothumbnails());
    slotUpdateDisplay();
    connect(m_view.profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(m_view.project_folder, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButton(const QString &)));
}


void ProjectSettings::slotUpdateDisplay() {
    QString currentProfile = m_view.profiles_list->currentText();
    QMap< QString, QString > values = ProfilesDialog::getSettingsForProfile(currentProfile);
    m_view.p_size->setText(values.value("width") + "x" + values.value("height"));
    m_view.p_fps->setText(values.value("frame_rate_num") + "/" + values.value("frame_rate_den"));
    m_view.p_aspect->setText(values.value("sample_aspect_num") + "/" + values.value("sample_aspect_den"));
    m_view.p_display->setText(values.value("display_aspect_num") + "/" + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0) m_view.p_progressive->setText(i18n("Interlaced"));
    else m_view.p_progressive->setText(i18n("Progressive"));
}

void ProjectSettings::slotUpdateButton(const QString &path) {
    if (path.isEmpty()) buttonOk->setEnabled(false);
    else buttonOk->setEnabled(true);
}

QString ProjectSettings::selectedProfile() {
    return ProfilesDialog::getPathFromDescription(m_view.profiles_list->currentText());
}

#include "projectsettings.moc"


