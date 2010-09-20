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

#include "profilesdialog.h"
#include "kdenlivesettings.h"

#include <KStandardDirs>
#include <KDebug>
#include <KMessageBox>
#include <KIO/NetAccess>

#include <QDir>
#include <QCloseEvent>

ProfilesDialog::ProfilesDialog(QWidget * parent) :
    QDialog(parent),
    m_profileIsModified(false),
    m_isCustomProfile(false)
{
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
    connect(m_view.button_default, SIGNAL(clicked()), this, SLOT(slotSetDefaultProfile()));

    connect(m_view.description, SIGNAL(textChanged(const QString &)), this, SLOT(slotProfileEdited()));
    connect(m_view.frame_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.frame_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.aspect_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.aspect_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.display_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.display_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.progressive, SIGNAL(stateChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.size_h, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.size_w, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
}

void ProfilesDialog::slotProfileEdited()
{
    m_profileIsModified = true;
}

void ProfilesDialog::fillList(const QString selectedProfile)
{
    // List the Mlt profiles
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
    m_selectedProfileIndex = m_view.profiles_list->currentIndex();
}

void ProfilesDialog::accept()
{
    if (askForSave()) QDialog::accept();
}

void ProfilesDialog::closeEvent(QCloseEvent *event)
{
    if (askForSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

bool ProfilesDialog::askForSave()
{
    if (!m_profileIsModified) return true;
    if (KMessageBox::questionYesNo(this, i18n("The custom profile was modified, do you want to save it?")) != KMessageBox::Yes) return true;
    return slotSaveProfile();
}

void ProfilesDialog::slotCreateProfile()
{
    m_view.button_delete->setEnabled(false);
    m_view.button_create->setEnabled(false);
    m_view.button_save->setEnabled(true);
    m_view.properties->setEnabled(true);
}

void ProfilesDialog::slotSetDefaultProfile()
{
    int ix = m_view.profiles_list->currentIndex();
    QString path = m_view.profiles_list->itemData(ix).toString();
    if (!path.isEmpty()) KdenliveSettings::setDefault_profile(path);
}

bool ProfilesDialog::slotSaveProfile()
{
    const QString profileDesc = m_view.description->text();
    int ix = m_view.profiles_list->findText(profileDesc);
    if (ix != -1) {
        // this profile name already exists
        const QString path = m_view.profiles_list->itemData(ix).toString();
        if (!path.contains('/')) {
            KMessageBox::sorry(this, i18n("A profile with same name already exists in MLT's default profiles, please choose another description for your custom profile."));
            return false;
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
    m_profileIsModified = false;
    fillList(profileDesc);
    m_view.button_create->setEnabled(true);
    return true;
}

void ProfilesDialog::saveProfile(const QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(this, i18n("Cannot write to file %1", path));
        return;
    }
    QTextStream out(&file);
    out << "description=" << m_view.description->text() << "\n" << "frame_rate_num=" << m_view.frame_num->value() << "\n" << "frame_rate_den=" << m_view.frame_den->value() << "\n" << "width=" << m_view.size_w->value() << "\n" << "height=" << m_view.size_h->value() << "\n" << "progressive=" << m_view.progressive->isChecked() << "\n" << "sample_aspect_num=" << m_view.aspect_num->value() << "\n" << "sample_aspect_den=" << m_view.aspect_den->value() << "\n" << "display_aspect_num=" << m_view.display_num->value() << "\n" << "display_aspect_den=" << m_view.display_den->value() << "\n";
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", path));
    }
    file.close();
}

void ProfilesDialog::slotDeleteProfile()
{
    const QString path = m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
    if (path.contains('/')) {
        KIO::NetAccess::del(KUrl(path), this);
        fillList();
    } else kDebug() << "//// Cannot delete profile " << path << ", does not seem to be custom one";
}

// static
MltVideoProfile ProfilesDialog::getVideoProfile(QString name)
{
    MltVideoProfile result;
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";
    QString path;
    bool isCustom = false;
    if (name.contains('/')) isCustom = true;

    if (!isCustom) {
        // List the Mlt profiles
        profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        if (profilesFiles.contains(name)) path = KdenliveSettings::mltpath() + name;
    }
    if (isCustom || path.isEmpty()) {
        path = name;
    }

    if (path.isEmpty() || !QFile::exists(path)) {
        if (name == "dv_pal") {
            kDebug() << "!!! WARNING, COULD NOT FIND DEFAULT MLT PROFILE";
            return result;
        }
        if (name == KdenliveSettings::default_profile()) KdenliveSettings::setDefault_profile("dv_pal");
        kDebug() << "// WARNING, COULD NOT FIND PROFILE " << name;
        return result;
    }
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
double ProfilesDialog::getStringEval(const MltVideoProfile &profile, QString eval)
{
    double result;
    eval.replace("%width", QString::number(profile.width));
    eval.replace("%height", QString::number(profile.height));
    if (eval.contains('/')) result = (double) eval.section('/', 0, 0).toInt() / eval.section('/', 1, 1).toInt();
    else if (eval.contains('*')) result = (double) eval.section('*', 0, 0).toInt() * eval.section('*', 1, 1).toInt();
    else if (eval.contains('+')) result = (double) eval.section('+', 0, 0).toInt() + eval.section('+', 1, 1).toInt();
    else if (eval.contains('-')) result = (double) eval.section('-', 0, 0).toInt() - eval.section('-', 1, 1).toInt();
    else result = eval.toDouble();
    return result;
}


// static
bool ProfilesDialog::existingProfileDescription(const QString &desc)
{
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    QStringList profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + profilesFiles.at(i), KConfig::SimpleConfig);
        if (desc == confFile.entryMap().value("description")) return true;
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        profilesFiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profilesFiles.size(); ++j) {
            KConfig confFile(customProfiles.at(i) + profilesFiles.at(j), KConfig::SimpleConfig);
            if (desc == confFile.entryMap().value("description")) return true;
        }
    }
    return false;
}

// static
QString ProfilesDialog::existingProfile(MltVideoProfile profile)
{
    // Check if the profile has a matching entry in existing ones
    QStringList profilesFilter;
    profilesFilter << "*";

    // Check the Mlt profiles
    QStringList profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + profilesFiles.at(i), KConfig::SimpleConfig);
        if (profile.display_aspect_den != confFile.entryMap().value("display_aspect_den").toInt()) continue;
        if (profile.display_aspect_num != confFile.entryMap().value("display_aspect_num").toInt()) continue;
        if (profile.sample_aspect_den != confFile.entryMap().value("sample_aspect_den").toInt()) continue;
        if (profile.sample_aspect_num != confFile.entryMap().value("sample_aspect_num").toInt()) continue;
        if (profile.width != confFile.entryMap().value("width").toInt()) continue;
        if (profile.height != confFile.entryMap().value("height").toInt()) continue;
        if (profile.frame_rate_den != confFile.entryMap().value("frame_rate_den").toInt()) continue;
        if (profile.frame_rate_num != confFile.entryMap().value("frame_rate_num").toInt()) continue;
        if (profile.progressive != confFile.entryMap().value("progressive").toInt()) continue;
        return profilesFiles.at(i);
    }

    // Check custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        profilesFiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profilesFiles.size(); ++j) {
            KConfig confFile(customProfiles.at(i) + profilesFiles.at(j), KConfig::SimpleConfig);
            if (profile.display_aspect_den != confFile.entryMap().value("display_aspect_den").toInt()) continue;
            if (profile.display_aspect_num != confFile.entryMap().value("display_aspect_num").toInt()) continue;
            if (profile.sample_aspect_den != confFile.entryMap().value("sample_aspect_den").toInt()) continue;
            if (profile.sample_aspect_num != confFile.entryMap().value("sample_aspect_num").toInt()) continue;
            if (profile.width != confFile.entryMap().value("width").toInt()) continue;
            if (profile.height != confFile.entryMap().value("height").toInt()) continue;
            if (profile.frame_rate_den != confFile.entryMap().value("frame_rate_den").toInt()) continue;
            if (profile.frame_rate_num != confFile.entryMap().value("frame_rate_num").toInt()) continue;
            if (profile.progressive != confFile.entryMap().value("progressive").toInt()) continue;
            return customProfiles.at(i) + profilesFiles.at(j);
        }
    }
    return QString();
}

// static
QMap <QString, QString> ProfilesDialog::getProfilesInfo()
{
    QMap <QString, QString> result;
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    QStringList profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + profilesFiles.at(i), KConfig::SimpleConfig);
        QString desc = confFile.entryMap().value("description");
        if (!desc.isEmpty()) result.insert(desc, profilesFiles.at(i));
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        profilesFiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profilesFiles.size(); ++j) {
            KConfig confFile(customProfiles.at(i) + profilesFiles.at(j), KConfig::SimpleConfig);
            QString desc = confFile.entryMap().value("description");
            if (!desc.isEmpty()) result.insert(desc, customProfiles.at(i) + profilesFiles.at(j));
        }
    }
    return result;
}

// static
QMap< QString, QString > ProfilesDialog::getSettingsFromFile(const QString path)
{
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";

    if (!path.contains('/')) {
        // This is an MLT profile
        KConfig confFile(KdenliveSettings::mltpath() + path, KConfig::SimpleConfig);
        return confFile.entryMap();
    } else {
        // This is a custom profile
        KConfig confFile(path, KConfig::SimpleConfig);
        return confFile.entryMap();
    }
}

// static
QMap< QString, QString > ProfilesDialog::getSettingsForProfile(const QString profileName)
{
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + profilesFiles.at(i), KConfig::SimpleConfig);
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
        for (int j = 0; j < profiles.size(); ++j) {
            KConfig confFile(customProfiles.at(i) + profiles.at(j), KConfig::SimpleConfig);
            QMap< QString, QString > values = confFile.entryMap();
            if (values.value("description") == profileName) {
                values.insert("path", customProfiles.at(i) + profiles.at(j));
                return values;
            }
        }
    }
    return QMap< QString, QString >();
}

// static
bool ProfilesDialog::matchProfile(int width, int height, double fps, double par, bool isImage, MltVideoProfile profile)
{
    int profileWidth;
    if (isImage) {
        // when using image, compare with display width
        profileWidth = profile.height * profile.display_aspect_num / profile.display_aspect_den + 0.5;
    } else profileWidth = profile.width;
    if (width != profileWidth || height != profile.height || (fps > 0 && qAbs(profile.frame_rate_num / profile.frame_rate_den - fps) > 0.4) || (par > 0 && qAbs(profile.sample_aspect_num / profile.sample_aspect_den - par) > 0.1)) return false;
    return true;
}

// static
QMap <QString, QString> ProfilesDialog::getProfilesFromProperties(int width, int height, double fps, double par, bool useDisplayWidth)
{
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    QMap <QString, QString> result;
    profilesFilter << "*";
    // List the Mlt profiles
    profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + profilesFiles.at(i), KConfig::SimpleConfig);
        QMap< QString, QString > values = confFile.entryMap();
        int profileWidth;
        if (useDisplayWidth) profileWidth = values.value("height").toInt() * values.value("display_aspect_num").toInt() / values.value("display_aspect_den").toInt() + 0.5;
        else profileWidth = values.value("width").toInt();
        if (profileWidth == width && values.value("height").toInt() == height) {
            double profile_fps = values.value("frame_rate_num").toDouble() / values.value("frame_rate_den").toDouble();
            double profile_par = values.value("sample_aspect_num").toDouble() / values.value("sample_aspect_den").toDouble();
            if ((fps <= 0 || qAbs(profile_fps - fps) < 0.5) && (par <= 0 || qAbs(profile_par - par) < 0.1))
                result.insert(profilesFiles.at(i), values.value("description"));
        }
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        QStringList profiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profiles.size(); j++) {
            KConfig confFile(customProfiles.at(i) + profiles.at(j), KConfig::SimpleConfig);
            QMap< QString, QString > values = confFile.entryMap();
            int profileWidth;
            if (useDisplayWidth) profileWidth = values.value("height").toInt() * values.value("display_aspect_num").toInt() / values.value("display_aspect_den").toInt() + 0.5;
            else profileWidth = values.value("width").toInt();
            if (profileWidth == width && values.value("height").toInt() == height) {
                double profile_fps = values.value("frame_rate_num").toDouble() / values.value("frame_rate_den").toDouble();
                double profile_par = values.value("sample_aspect_num").toDouble() / values.value("sample_aspect_den").toDouble();
                if ((fps <= 0 || qAbs(profile_fps - fps) < 0.5) && (par <= 0 || qAbs(profile_par - par) < 0.1))
                    result.insert(profiles.at(j), values.value("description"));
            }
        }
    }
    return result;
}

// static
QString ProfilesDialog::getPathFromDescription(const QString profileDesc)
{
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << "*";

    // List the Mlt profiles
    profilesFiles = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(KdenliveSettings::mltpath() + profilesFiles.at(i), KConfig::SimpleConfig);
        QMap< QString, QString > values = confFile.entryMap();
        if (values.value("description") == profileDesc) return profilesFiles.at(i);
    }

    // List custom profiles
    QStringList customProfiles = KGlobal::dirs()->findDirs("appdata", "profiles");
    for (int i = 0; i < customProfiles.size(); ++i) {
        QStringList profiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profiles.size(); ++j) {
            KConfig confFile(customProfiles.at(i) + profiles.at(j), KConfig::SimpleConfig);
            QMap< QString, QString > values = confFile.entryMap();
            if (values.value("description") == profileDesc) return customProfiles.at(i) + profiles.at(j);
        }
    }
    return QString();
}

// static
void ProfilesDialog::saveProfile(MltVideoProfile &profile)
{
    int i = 0;
    QString customName = "profiles/customprofile";
    QString profilePath = KStandardDirs::locateLocal("appdata", customName + QString::number(i));
    kDebug() << " TYING PROFILE FILE: " << profilePath;
    while (KIO::NetAccess::exists(KUrl(profilePath), KIO::NetAccess::SourceSide, 0)) {
        i++;
        profilePath = KStandardDirs::locateLocal("appdata", customName + QString::number(i));
    }
    QFile file(profilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(0, i18n("Cannot write to file %1", profilePath));
        return;
    }
    QTextStream out(&file);
    out << "description=" << profile.description << "\n" << "frame_rate_num=" << profile.frame_rate_num << "\n" << "frame_rate_den=" << profile.frame_rate_den << "\n" << "width=" << profile.width << "\n" << "height=" << profile.height << "\n" << "progressive=" << profile.progressive << "\n" << "sample_aspect_num=" << profile.sample_aspect_num << "\n" << "sample_aspect_den=" << profile.sample_aspect_den << "\n" << "display_aspect_num=" << profile.display_aspect_num << "\n" << "display_aspect_den=" << profile.display_aspect_den << "\n";
    if (file.error() != QFile::NoError) {
        KMessageBox::error(0, i18n("Cannot write to file %1", profilePath));
    }
    file.close();
    profile.path = profilePath;
}


void ProfilesDialog::slotUpdateDisplay()
{
    if (askForSave() == false) {
        m_view.profiles_list->blockSignals(true);
        m_view.profiles_list->setCurrentIndex(m_selectedProfileIndex);
        m_view.profiles_list->blockSignals(false);
        return;
    }

    m_selectedProfileIndex = m_view.profiles_list->currentIndex();
    QString currentProfile = m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
    m_isCustomProfile = currentProfile.contains('/');
    m_view.button_create->setEnabled(true);
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
    if (values.value("progressive").toInt()) {
        m_view.fields->setText(QString::number((double) values.value("frame_rate_num").toInt() / values.value("frame_rate_den").toInt(), 'f', 2));
    } else {
        m_view.fields->setText(QString::number((double) 2 * values.value("frame_rate_num").toInt() / values.value("frame_rate_den").toInt(), 'f', 2));
    }
    m_profileIsModified = false;
}


#include "profilesdialog.moc"


