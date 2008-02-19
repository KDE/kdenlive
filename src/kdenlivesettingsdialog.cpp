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

#include "profilesdialog.h"
#include "kdenlivesettings.h"
#include "kdenlivesettingsdialog.h"

KdenliveSettingsDialog::KdenliveSettingsDialog(QWidget * parent): KConfigDialog(parent, "settings", KdenliveSettings::self())
{

  QWidget *page1 = new QWidget;
  m_configMisc = new Ui::ConfigMisc_UI( );
  m_configMisc->setupUi(page1);
  addPage( page1, i18n("Misc"), "misc" );

  QWidget *page2 = new QWidget;
  m_configEnv = new Ui::ConfigEnv_UI( );
  m_configEnv->setupUi(page2);
  m_configEnv->kcfg_mltpath->setMode(KFile::Directory);

  //WARNING: the 2 lines below should not be necessary, but does not work without it...
  m_configEnv->kcfg_mltpath->setPath(KdenliveSettings::mltpath());
  m_configEnv->kcfg_rendererpath->setPath(KdenliveSettings::rendererpath());
  addPage( page2, i18n("Environnement"), "env" );

  QStringList profilesNames = ProfilesDialog::getProfileNames();
  m_configMisc->profiles_list->addItems(profilesNames);

  //User edited the configuration - update your local copies of the
  //configuration data

  slotUpdateDisplay();
  connect(m_configMisc->profiles_list, SIGNAL(currentIndexChanged( int )), this, SLOT(slotUpdateDisplay()));
}


void KdenliveSettingsDialog::slotUpdateDisplay()
{
  QString currentProfile = m_configMisc->profiles_list->currentText();
  QMap< QString, QString > values = ProfilesDialog::getSettingsForProfile(currentProfile);
  m_configMisc->p_size->setText(values.value("width") + "x" + values.value("height"));
  m_configMisc->p_fps->setText(values.value("frame_rate_num") + "/" + values.value("frame_rate_den"));
  m_configMisc->p_aspect->setText(values.value("sample_aspect_num") + "/" + values.value("sample_aspect_den"));
  m_configMisc->p_display->setText(values.value("display_aspect_num") + "/" + values.value("display_aspect_den"));
  if (values.value("progressive").toInt() == 0) m_configMisc->p_progressive->setText(i18n("Interlaced"));
  else m_configMisc->p_progressive->setText(i18n("Progressive"));
}


#include "kdenlivesettingsdialog.moc"


