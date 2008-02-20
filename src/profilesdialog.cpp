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

ProfilesDialog::ProfilesDialog(QWidget * parent): QDialog(parent), m_isCustomProfile(false)
{
  m_view.setupUi(this);

  QStringList profilesFilter;
  profilesFilter<<"*";

  // List the Mlt profiles
  m_mltProfilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
  m_view.profiles_list->addItems(m_mltProfilesList);

  // List custom profiles
  QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
  for (int i = 0; i < customProfiles.size(); ++i)
    m_customProfilesList << QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
  m_view.profiles_list->addItems(m_customProfilesList);

  if (!KdenliveSettings::default_profile().isEmpty()) {
    int ix = m_view.profiles_list->findText(KdenliveSettings::default_profile());
    m_view.profiles_list->setCurrentIndex(ix);
  }
  slotUpdateDisplay();
  connect(m_view.profiles_list, SIGNAL(currentIndexChanged( int )), this, SLOT(slotUpdateDisplay()));
}


// static
QStringList ProfilesDialog::getProfileNames()
{
  QStringList profilesNames;
  QStringList profilesFiles;
  QStringList profilesFilter;
  profilesFilter<<"*";

  // List the Mlt profiles
  profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
  for (int i = 0; i < profilesFiles.size(); ++i) {
    KConfig confFile(KdenliveSettings::mltpath() + "/" + profilesFiles.at(i));
    QString desc = confFile.entryMap().value("description");
    if (!desc.isEmpty()) profilesNames.append(desc);
  }

  // List custom profiles
  QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
  for (int i = 0; i < customProfiles.size(); ++i) {
    profilesFiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
      KConfig confFile(customProfiles.at(i) + "/" + profilesFiles.at(i));
    QString desc = confFile.entryMap().value("description");
    if (!desc.isEmpty()) profilesNames.append(desc);
    }
  }

  return profilesNames;
}

// static
QMap< QString, QString > ProfilesDialog::getSettingsFromFile(const QString path)
{
  QStringList profilesNames;
  QStringList profilesFiles;
  QStringList profilesFilter;
  profilesFilter<<"*";

  // List the Mlt profiles
  profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
  for (int i = 0; i < profilesFiles.size(); ++i) {
    if (profilesFiles.at(i) == path) {
      KConfig confFile(KdenliveSettings::mltpath() + "/" + profilesFiles.at(i));
      return confFile.entryMap();
    }
  }

  // List custom profiles
  QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
  for (int i = 0; i < customProfiles.size(); ++i) {
    QStringList profiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profiles.size(); ++i) {
      if (profiles.at(i) == path) {
	KConfig confFile(customProfiles.at(i) + "/" + profiles.at(i));
	return confFile.entryMap();
      }
    }
  }
  return QMap< QString, QString >();
}

// static
QMap< QString, QString > ProfilesDialog::getSettingsForProfile(const QString profileName)
{
  QStringList profilesNames;
  QStringList profilesFiles;
  QStringList profilesFilter;
  profilesFilter<<"*";

  // List the Mlt profiles
  profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
  for (int i = 0; i < profilesFiles.size(); ++i) {
    KConfig confFile(KdenliveSettings::mltpath() + "/" + profilesFiles.at(i));
    QMap< QString, QString > values = confFile.entryMap();
    if (values.value("description") == profileName) return values;
  }

  // List custom profiles
  QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
  for (int i = 0; i < customProfiles.size(); ++i) {
    QStringList profiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profiles.size(); ++i) {
      KConfig confFile(customProfiles.at(i) + "/" + profiles.at(i));
      QMap< QString, QString > values = confFile.entryMap();
      if (values.value("description") == profileName) return values;
    }
  }
  return QMap< QString, QString >();
}

void ProfilesDialog::slotUpdateDisplay()
{
  QString currentProfile = m_view.profiles_list->currentText();
  QString currentProfilePath;
  if (m_mltProfilesList.indexOf(currentProfile) != -1) { 
    currentProfilePath = KdenliveSettings::mltpath() + "/" + currentProfile;
    m_isCustomProfile = false;
  }
  else {
    m_isCustomProfile = true;
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "mltprofiles");
    QStringList profilesFilter;
    profilesFilter<<"*";
    int i;
    for (i = 0; i < customProfiles.size(); ++i) {
      QStringList profs = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
      if (profs.indexOf(currentProfile) != -1) break;
    }
    currentProfilePath = customProfiles.at(i) + "/" + currentProfile;
  }
  m_view.button_delete->setEnabled(m_isCustomProfile);
  m_view.properties->setEnabled(m_isCustomProfile);

  KConfig confFile(currentProfilePath);
  QMap< QString, QString > values = confFile.entryMap();
  m_view.description->setText(values.value("description"));
  m_view.size_w->setValue(values.value("width").toInt());
  m_view.size_h->setValue(values.value("height").toInt());
  m_view.aspect_num->setValue(values.value("sample_aspect_num").toInt());
  m_view.aspect_den->setValue(values.value("sample_aspect_den").toInt());
  m_view.display_num->setValue(values.value("display_aspect_num").toInt());
  m_view.display_den->setValue(values.value("display_aspect_den").toInt());
  m_view.frame_num->setValue(values.value("frame_rate_num").toInt());
  m_view.frame_den->setValue(values.value("frame_rate_den").toInt());
  m_view.progressive->setChecked(values.value("progressive").toInt());

}


#include "profilesdialog.moc"


