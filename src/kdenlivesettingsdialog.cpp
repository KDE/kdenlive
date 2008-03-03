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

KdenliveSettingsDialog::KdenliveSettingsDialog(QWidget * parent): KConfigDialog(parent, "settings", KdenliveSettings::self()) {

    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    page1 = addPage(p1, i18n("Misc"), "misc");

    QWidget *p3 = new QWidget;
    m_configDisplay.setupUi(p3);
    page3 = addPage(p3, i18n("Display"), "display");

    QWidget *p2 = new QWidget;
    m_configEnv.setupUi(p2);
    m_configEnv.mltpathurl->setMode(KFile::Directory);
    m_configEnv.mltpathurl->lineEdit()->setObjectName("kcfg_mltpath");
    m_configEnv.rendererpathurl->lineEdit()->setObjectName("kcfg_rendererpath");
    m_configEnv.tmppathurl->setMode(KFile::Directory);
    m_configEnv.tmppathurl->lineEdit()->setObjectName("kcfg_currenttmpfolder");
    page2 = addPage(p2, i18n("Environnement"), "env");

    QStringList profilesNames = ProfilesDialog::getProfileNames();
    m_configMisc.profiles_list->addItems(profilesNames);
    m_defaulfProfile = ProfilesDialog::getSettingsFromFile(KdenliveSettings::default_profile()).value("description");
    if (profilesNames.contains(m_defaulfProfile)) m_configMisc.profiles_list->setCurrentItem(m_defaulfProfile);

    slotUpdateDisplay();
    connect(m_configMisc.profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
}

KdenliveSettingsDialog::~KdenliveSettingsDialog() {}


bool KdenliveSettingsDialog::hasChanged() {
    kDebug() << "// // // KCONFIG hasChanged called";
    if (m_configMisc.profiles_list->currentText() != m_defaulfProfile) return true;
    return KConfigDialog::hasChanged();
}

void KdenliveSettingsDialog::slotUpdateDisplay() {
    QString currentProfile = m_configMisc.profiles_list->currentText();
    QMap< QString, QString > values = ProfilesDialog::getSettingsForProfile(currentProfile);
    m_configMisc.p_size->setText(values.value("width") + "x" + values.value("height"));
    m_configMisc.p_fps->setText(values.value("frame_rate_num") + "/" + values.value("frame_rate_den"));
    m_configMisc.p_aspect->setText(values.value("sample_aspect_num") + "/" + values.value("sample_aspect_den"));
    m_configMisc.p_display->setText(values.value("display_aspect_num") + "/" + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0) m_configMisc.p_progressive->setText(i18n("Interlaced"));
    else m_configMisc.p_progressive->setText(i18n("Progressive"));
}


#include "kdenlivesettingsdialog.moc"


