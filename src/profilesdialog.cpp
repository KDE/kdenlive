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
#include <KMessageBox>
#include <KIO/NetAccess>

#include "kdenlivesettings.h"
#include "profilesdialog.h"

ProfilesDialog::ProfilesDialog(QWidget * parent): QDialog(parent), m_isCustomProfile(false) {
    m_view.setupUi(this);

    QStringList profilesFilter;
    profilesFilter << "*";

    m_view.button_delete->setIcon(KIcon("trash-empty"));
    m_view.button_delete->setToolTip(i18n("Delete profile"));
    m_view.button_save->setIcon(KIcon("document-save"));
    m_view.button_save->setToolTip(i18n("Save profile"));
    m_view.button_create->setIcon(KIcon("document-new"));
    m_view.button_create->setToolTip(i18n("Create new profile"));

    fillList();
    slotUpdateDisplay();
    connect(m_view.profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(m_view.button_create, SIGNAL(clicked()), this, SLOT(slotCreateProfile()));
    connect(m_view.button_save, SIGNAL(clicked()), this, SLOT(slotSaveProfile()));
    connect(m_view.button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteProfile()));
}

void ProfilesDialog::fillList(const QString selectedProfile) {
    // List the Mlt profiles
    m_view.profiles_list->clear();
    QMap <QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMapIterator<QString, QString> i(profilesInfo);
    while (i.hasNext()) {
        i.next();
        m_view.profiles_list->addItem(i.key(), i.value());
    }

    if (!KdenliveSettings::default_profile().isEmpty()) {
        for (int i = 0; i < m_view.profiles_list->count(); i++) {
            if (m_view.profiles_list->itemData(i).toString() == KdenliveSettings::default_profile()) {
                m_view.profiles_list->setCurrentIndex(i);
                break;
            }
        }
    }
    int ix = m_view.profiles_list->findText(selectedProfile);
    if (ix != -1) m_view.profiles_list->setCurrentIndex(ix);
}

void ProfilesDialog::slotCreateProfile() {
    m_view.button_delete->setEnabled(false);
    m_view.button_create->setEnabled(false);
    m_view.button_save->setEnabled(true);
    m_view.properties->setEnabled(true);
}

void ProfilesDialog::slotSaveProfile() {
    const QString profileDesc = m_view.description->text();
    int ix = m_view.profiles_list->findText(profileDesc);
    if (ix != -1) {
        // this profile name already exists
        const QString path = m_view.profiles_list->itemData(ix).toString();
        if (!path.contains("/")) {
            KMessageBox::sorry(this, i18n("A profile with same name already exists in MLT's default profiles, please choose another description for your custom profile."));
            return;
        }
        saveProfile(path);
    } else {
        int i = 0;
        QString customName = "profiles/customprofile";
        QString profilePath = KStandardDirs::locateLocal("appdata", customName + QString::number(i));
        kDebug() << " TYING PROFILE FILE: " << profilePath;
        while (KIO::NetAccess::exists(KUrl(profilePath), KIO::NetAccess::SourceSide, this)) {
            i++;
            profilePath = KStandardDirs::locateLocal("appdata", customName + QString::number(i));
        }
        saveProfile(profilePath);
    }
    fillList(profileDesc);
}

void ProfilesDialog::saveProfile(const QString path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(this, i18n("Cannot write to file %1", path));
        return;
    }
    QTextStream out(&file);
    out << "description=" << m_view.description->text() << "\n" << "frame_rate_num=" << m_view.frame_num->value() << "\n" << "frame_rate_den=" << m_view.frame_den->value() << "\n" << "width=" << m_view.size_w->value() << "\n" << "height=" << m_view.size_h->value() << "\n" << "progressive=" << m_view.progressive->isChecked() << "\n" << "sample_aspect_num=" << m_view.aspect_num->value() << "\n" << "sample_aspect_den=" << m_view.aspect_den->value() << "\n" << "display_aspect_num=" << m_view.display_num->value() << "\n" << "display_aspect_den=" << m_view.display_den->value() << "\n";
    file.close();
}

void ProfilesDialog::slotDeleteProfile() {
    const QString path = m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
    if (path.contains("/")) {
        KIO::NetAccess::del(KUrl(path), this);
        fillList();
    } else kDebug() << "//// Cannot delete profile " << path << ", does not seem to be custom one";
}

// static
MltVideoProfile ProfilesDialog::getVideoProfile(QString name) {
    MltVideoProfile result;
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";
    QString path;
    bool isCustom = false;
    if (name.contains('/')) isCustom = true;

    if (!isCustom) {
        // List the Mlt profiles
        profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        if (profilesFiles.contains(name)) path = KdenliveSettings::mltpath() + "/" + name;
    }
    if (isCustom  || path.isEmpty()) {
        // List custom profiles
        path = name;
        /*        QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
                for (int i = 0; i < customProfiles.size(); ++i) {
                    profilesFiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
                    if (profilesFiles.contains(name)) {
                        path = customProfiles.at(i) + "/" + name;
                        break;
                    }
                }*/
    }

    if (path.isEmpty()) return result;
    KConfig confFile(path, KConfig::SimpleConfig);
    result.path = name;
    result.description = confFile.entryMap().value("description");
    result.frame_rate_num = confFile.entryMap().value("frame_rate_num").toInt();
    result.frame_rate_den = confFile.entryMap().value("frame_rate_den").toInt();
    result.width = confFile.entryMap().value("width").toInt();
    result.height = confFile.entryMap().value("height").toInt();
    result.progressive = confFile.entryMap().value("progressive").toInt();
    result.sample_aspect_num = confFile.entryMap().value("sample_aspect_num").toInt();
    result.sample_aspect_den = confFile.entryMap().value("sample_aspect_den").toInt();
    result.display_aspect_num = confFile.entryMap().value("display_aspect_num").toInt();
    result.display_aspect_den = confFile.entryMap().value("display_aspect_den").toInt();
    return result;
}

// static
QString ProfilesDialog::getProfileDescription(QString name) {
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    if (profilesFiles.contains(name)) {
        KConfig confFile(KdenliveSettings::mltpath() + "/" + name, KConfig::SimpleConfig);
        return confFile.entryMap().value("description");
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        profilesFiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        if (profilesFiles.contains(name)) {
            KConfig confFile(customProfiles.at(i) + "/" + name, KConfig::SimpleConfig);
            return confFile.entryMap().value("description");
        }
    }

    return QString();
}

// static
QMap <QString, QString> ProfilesDialog::getProfilesInfo() {
    QMap <QString, QString> result;
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    QStringList profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + "/" + profilesFiles.at(i), KConfig::SimpleConfig);
        QString desc = confFile.entryMap().value("description");
        if (!desc.isEmpty()) result.insert(desc, profilesFiles.at(i));
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        profilesFiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profilesFiles.size(); ++j) {
            KConfig confFile(customProfiles.at(i) + "/" + profilesFiles.at(j), KConfig::SimpleConfig);
            QString desc = confFile.entryMap().value("description");
            if (!desc.isEmpty()) result.insert(desc, customProfiles.at(i) + "/" + profilesFiles.at(j));
        }
    }
    return result;
}

// static
QMap< QString, QString > ProfilesDialog::getSettingsFromFile(const QString path) {
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";

    if (!path.contains("/")) {
        // This is an MLT profile
        KConfig confFile(KdenliveSettings::mltpath() + "/" + path, KConfig::SimpleConfig);
        return confFile.entryMap();
    } else {
        // This is a custom profile
        KConfig confFile(path, KConfig::SimpleConfig);
        return confFile.entryMap();
    }
}

// static
QMap< QString, QString > ProfilesDialog::getSettingsForProfile(const QString profileName) {
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + "/" + profilesFiles.at(i), KConfig::SimpleConfig);
        QMap< QString, QString > values = confFile.entryMap();
        if (values.value("description") == profileName) {
            values.insert("path", profilesFiles.at(i));
            return values;
        }
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        QStringList profiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int i = 0; i < profiles.size(); ++i) {
            KConfig confFile(customProfiles.at(i) + "/" + profiles.at(i), KConfig::SimpleConfig);
            QMap< QString, QString > values = confFile.entryMap();
            if (values.value("description") == profileName) {
                values.insert("path", customProfiles.at(i) + "/" + profiles.at(i));
                return values;
            }
        }
    }
    return QMap< QString, QString >();
}

// static
QString ProfilesDialog::getPathFromDescription(const QString profileDesc) {
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + "/" + profilesFiles.at(i), KConfig::SimpleConfig);
        QMap< QString, QString > values = confFile.entryMap();
        if (values.value("description") == profileDesc) return profilesFiles.at(i);
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        QStringList profiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int i = 0; i < profiles.size(); ++i) {
            KConfig confFile(customProfiles.at(i) + "/" + profiles.at(i), KConfig::SimpleConfig);
            QMap< QString, QString > values = confFile.entryMap();
            if (values.value("description") == profileDesc) return customProfiles.at(i) + "/" + profiles.at(i);
        }
    }
    return QString();
}


void ProfilesDialog::slotUpdateDisplay() {
    QString currentProfile = m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
    m_isCustomProfile = currentProfile.contains("/");
    m_view.button_delete->setEnabled(m_isCustomProfile);
    m_view.properties->setEnabled(m_isCustomProfile);
    m_view.button_save->setEnabled(m_isCustomProfile);
    QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(currentProfile);
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


