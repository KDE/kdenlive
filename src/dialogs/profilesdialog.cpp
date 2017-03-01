/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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
#include "utils/KoIconUtils.h"
#include "profiles/profilerepository.hpp"

#include <KMessageBox>
#include <KMessageWidget>
#include "klocalizedstring.h"

#include <QDir>
#include <QCloseEvent>
#include <QStandardPaths>
#include "kdenlive_debug.h"

ProfilesDialog::ProfilesDialog(const QString &profileDescription, QWidget *parent) :
    QDialog(parent),
    m_profileIsModified(false),
    m_isCustomProfile(false)
{

    //ask profile repository for a refresh
    ProfileRepository::get()->refresh();

    m_view.setupUi(this);

    // Add message widget
    QGridLayout *lay = (QGridLayout *)layout();
    m_infoMessage = new KMessageWidget;
    lay->addWidget(m_infoMessage, 2, 0, 1, -1);
    m_infoMessage->setCloseButtonVisible(true);
    m_infoMessage->hide();

    // Fill colorspace list (see mlt_profile.h)
    m_view.colorspace->addItem(getColorspaceDescription(601), 601);
    m_view.colorspace->addItem(getColorspaceDescription(709), 709);
    m_view.colorspace->addItem(getColorspaceDescription(240), 240);
    m_view.colorspace->addItem(getColorspaceDescription(0), 0);

    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");

    m_view.button_delete->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    m_view.button_delete->setToolTip(i18n("Delete profile"));
    m_view.button_save->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-save")));
    m_view.button_save->setToolTip(i18n("Save profile"));
    m_view.button_create->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-new")));
    m_view.button_create->setToolTip(i18n("Create new profile"));

    fillList(profileDescription);
    slotUpdateDisplay();
    connect(m_view.profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(m_view.button_create, &QAbstractButton::clicked, this, &ProfilesDialog::slotCreateProfile);
    connect(m_view.button_save, &QAbstractButton::clicked, this, &ProfilesDialog::slotSaveProfile);
    connect(m_view.button_delete, &QAbstractButton::clicked, this, &ProfilesDialog::slotDeleteProfile);
    connect(m_view.button_default, &QAbstractButton::clicked, this, &ProfilesDialog::slotSetDefaultProfile);

    connect(m_view.description, &QLineEdit::textChanged, this, &ProfilesDialog::slotProfileEdited);
    connect(m_view.frame_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.frame_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.aspect_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.aspect_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.display_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.display_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.progressive, &QCheckBox::stateChanged, this, &ProfilesDialog::slotProfileEdited);
    connect(m_view.size_h, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.size_w, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.size_w, &QAbstractSpinBox::editingFinished, this, &ProfilesDialog::slotAdjustWidth);
    m_view.size_w->setSingleStep(8);
}

ProfilesDialog::ProfilesDialog(const QString &profilePath, bool, QWidget *parent) :
    QDialog(parent),
    m_profileIsModified(false),
    m_isCustomProfile(true),
    m_customProfilePath(profilePath)
{
    m_view.setupUi(this);

    // Add message widget
    QGridLayout *lay = (QGridLayout *)layout();
    m_infoMessage = new KMessageWidget;
    lay->addWidget(m_infoMessage, 2, 0, 1, -1);
    m_infoMessage->setCloseButtonVisible(true);
    m_infoMessage->hide();

    // Fill colorspace list (see mlt_profile.h)
    m_view.colorspace->addItem(getColorspaceDescription(601), 601);
    m_view.colorspace->addItem(getColorspaceDescription(709), 709);
    m_view.colorspace->addItem(getColorspaceDescription(240), 240);
    m_view.colorspace->addItem(getColorspaceDescription(0), 0);

    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");

    m_view.button_save->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-save")));
    m_view.button_save->setToolTip(i18n("Save profile"));
    m_view.button_create->setHidden(true);
    m_view.profiles_list->setHidden(true);
    m_view.button_delete->setHidden(true);
    m_view.button_default->setHidden(true);
    m_view.description->setEnabled(false);

    slotUpdateDisplay(profilePath);
    connect(m_view.button_save, &QAbstractButton::clicked, this, &ProfilesDialog::slotSaveProfile);

    connect(m_view.description, &QLineEdit::textChanged, this, &ProfilesDialog::slotProfileEdited);
    connect(m_view.frame_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.frame_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.aspect_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.aspect_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.display_num, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.display_den, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.progressive, &QCheckBox::stateChanged, this, &ProfilesDialog::slotProfileEdited);
    connect(m_view.size_h, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.size_w, SIGNAL(valueChanged(int)), this, SLOT(slotProfileEdited()));
    connect(m_view.size_w, &QAbstractSpinBox::editingFinished, this, &ProfilesDialog::slotAdjustWidth);
    m_view.size_w->setSingleStep(8);
}

void ProfilesDialog::slotAdjustWidth()
{
    // A profile's width should always be a multiple of 8
    m_view.size_w->blockSignals(true);
    int val = m_view.size_w->value();
    int correctedWidth = (val + 7) / 8 * 8;
    if (val == correctedWidth) {
        // Ok, no action required, width is a multiple of 8
        m_infoMessage->animatedHide();
    } else {
        m_view.size_w->setValue(correctedWidth);
        m_infoMessage->setText(i18n("Profile width must be a multiple of 8. It was adjusted to %1", correctedWidth));
        m_infoMessage->setWordWrap(m_infoMessage->text().length() > 35);
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->animatedShow();
    }
    m_view.size_w->blockSignals(false);
}

void ProfilesDialog::slotProfileEdited()
{
    m_profileIsModified = true;
}

void ProfilesDialog::fillList(const QString &selectedProfile)
{
    m_view.profiles_list->clear();
    // Retrieve the list from the repository
    QVector<QPair<QString, QString> > profiles = ProfileRepository::get()->getAllProfiles();
    for (const auto & p : profiles) {
        m_view.profiles_list->addItem(p.first, p.second);
    }

    if (!KdenliveSettings::default_profile().isEmpty()) {
        int ix = m_view.profiles_list->findData(KdenliveSettings::default_profile());
        if (ix > -1) {
            m_view.profiles_list->setCurrentIndex(ix);
        } else {
            // Error, profile not found
            qCWarning(KDENLIVE_LOG) << "Project profile not found, disable  editing";
        }
    }
    int ix = m_view.profiles_list->findText(selectedProfile);
    if (ix != -1) {
        m_view.profiles_list->setCurrentIndex(ix);
    }
    m_selectedProfileIndex = m_view.profiles_list->currentIndex();
}

void ProfilesDialog::accept()
{
    if (askForSave()) {
        QDialog::accept();
    }
}

void ProfilesDialog::reject()
{
    if (askForSave()) {
        QDialog::reject();
    }
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
    if (!m_profileIsModified) {
        return true;
    }
    if (KMessageBox::questionYesNo(this, i18n("The custom profile was modified, do you want to save it?")) != KMessageBox::Yes) {
        return true;
    }
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
    if (m_profileIsModified) {
        m_infoMessage->setText(i18n("Save your profile before setting it to default"));
        m_infoMessage->setWordWrap(m_infoMessage->text().length() > 35);
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->animatedShow();
        return;
    }
    int ix = m_view.profiles_list->currentIndex();
    QString path = m_view.profiles_list->itemData(ix).toString();
    if (!path.isEmpty()) {
        KdenliveSettings::setDefault_profile(path);
    }
}

bool ProfilesDialog::slotSaveProfile()
{
    slotAdjustWidth();

    if (!m_customProfilePath.isEmpty()) {
        saveProfile(m_customProfilePath);
        return true;
    }
    const QString profileDesc = m_view.description->text();
    int ix = m_view.profiles_list->findText(profileDesc);
    if (ix != -1) {
        // this profile name already exists
        const QString path = m_view.profiles_list->itemData(ix).toString();
        if (!path.contains(QLatin1Char('/'))) {
            KMessageBox::sorry(this, i18n("A profile with same name already exists in MLT's default profiles, please choose another description for your custom profile."));
            return false;
        }
        saveProfile(path);
    } else {
        int i = 0;
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }
        QString customName = QStringLiteral("customprofile");
        QString profilePath =  dir.absoluteFilePath(customName + QString::number(i));
        while (QFile::exists(profilePath)) {
            ++i;
            profilePath = dir.absoluteFilePath(customName + QString::number(i));
        }
        saveProfile(profilePath);
    }
    m_profileIsModified = false;
    fillList(profileDesc);
    m_view.button_create->setEnabled(true);
    return true;
}

void ProfilesDialog::saveProfile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(this, i18n("Cannot open file %1", path));
        return;
    }
    QTextStream out(&file);
    out << "description=" << m_view.description->text() << '\n' << "frame_rate_num=" << m_view.frame_num->value() << '\n' << "frame_rate_den=" << m_view.frame_den->value() << '\n' << "width=" << m_view.size_w->value() << '\n' << "height=" << m_view.size_h->value() << '\n' << "progressive=" << m_view.progressive->isChecked() << '\n' << "sample_aspect_num=" << m_view.aspect_num->value() << '\n' << "sample_aspect_den=" << m_view.aspect_den->value() << '\n' << "display_aspect_num=" << m_view.display_num->value() << '\n' << "display_aspect_den=" << m_view.display_den->value() << '\n' << "colorspace=" << m_view.colorspace->itemData(m_view.colorspace->currentIndex()).toInt() << '\n';
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", path));
    }
    file.close();
}

void ProfilesDialog::slotDeleteProfile()
{
    const QString path = m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
    bool success = false;
    if (path.contains(QLatin1Char('/'))) {
        success = QFile::remove(path);
        fillList();
    }
    if (!success) {
        qCDebug(KDENLIVE_LOG) << "//// Cannot delete profile " << path << ", does not seem to be custom one";
    }
}

// static
MltVideoProfile ProfilesDialog::getVideoProfileFromXml(const QDomElement &element)
{
    MltVideoProfile result;
    result.description = element.attribute(QStringLiteral("description"));
    result.frame_rate_num = element.attribute(QStringLiteral("frame_rate_num")).toInt();
    result.frame_rate_den = element.attribute(QStringLiteral("frame_rate_den")).toInt();
    result.width = element.attribute(QStringLiteral("width")).toInt();
    result.height = element.attribute(QStringLiteral("height")).toInt();
    result.progressive = element.attribute(QStringLiteral("progressive")).toInt();
    result.sample_aspect_num = element.attribute(QStringLiteral("sample_aspect_num")).toInt();
    result.sample_aspect_den = element.attribute(QStringLiteral("sample_aspect_den")).toInt();
    result.display_aspect_num = element.attribute(QStringLiteral("display_aspect_num")).toInt();
    result.display_aspect_den = element.attribute(QStringLiteral("display_aspect_den")).toInt();
    result.colorspace = element.attribute(QStringLiteral("colorspace")).toInt();
    result.path = existingProfile(result);
    return result;
}

// static
MltVideoProfile ProfilesDialog::getVideoProfile(const QString &name)
{
    MltVideoProfile result;
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QString path;
    bool isCustom = false;
    if (name.contains(QLatin1Char('/'))) {
        isCustom = true;
    }
    if (!isCustom) {
        // List the Mlt profiles
        QDir mltDir(KdenliveSettings::mltpath());
        if (mltDir.exists(name)) {
            path = mltDir.absoluteFilePath(name);
        }
    }
    if (isCustom || path.isEmpty()) {
        path = name;
    }

    if (path.isEmpty() || !QFile::exists(path)) {
        if (name == QLatin1String("dv_pal")) {
            //qCDebug(KDENLIVE_LOG) << "!!! WARNING, COULD NOT FIND DEFAULT MLT PROFILE";
            return result;
        }
        if (name == KdenliveSettings::default_profile()) {
            KdenliveSettings::setDefault_profile(QStringLiteral("dv_pal"));
        }
        //qCDebug(KDENLIVE_LOG) << "// WARNING, COULD NOT FIND PROFILE " << name;
        return result;
    }
    return getProfileFromPath(path, name);
}

MltVideoProfile ProfilesDialog::getProfileFromPath(const QString &path, const QString &name)
{
    KConfig confFile(path, KConfig::SimpleConfig);
    MltVideoProfile result;
    QMap<QString, QString> entries = confFile.entryMap();
    result.path = name;
    result.description = entries.value(QStringLiteral("description"));
    result.frame_rate_num = entries.value(QStringLiteral("frame_rate_num")).toInt();
    result.frame_rate_den = entries.value(QStringLiteral("frame_rate_den")).toInt();
    result.width = entries.value(QStringLiteral("width")).toInt();
    result.height = entries.value(QStringLiteral("height")).toInt();
    result.progressive = entries.value(QStringLiteral("progressive")).toInt();
    result.sample_aspect_num = entries.value(QStringLiteral("sample_aspect_num")).toInt();
    result.sample_aspect_den = entries.value(QStringLiteral("sample_aspect_den")).toInt();
    result.display_aspect_num = entries.value(QStringLiteral("display_aspect_num")).toInt();
    result.display_aspect_den = entries.value(QStringLiteral("display_aspect_den")).toInt();
    result.colorspace = entries.value(QStringLiteral("colorspace")).toInt();
    return result;
}

// static
MltVideoProfile ProfilesDialog::getVideoProfile(Mlt::Profile &profile)
{
    MltVideoProfile result;
    result.description = profile.description();
    result.frame_rate_num = profile.frame_rate_num();
    result.frame_rate_den = profile.frame_rate_den();
    result.width = profile.width();
    result.height = profile.height();
    result.progressive = profile.progressive();
    result.sample_aspect_num = profile.sample_aspect_num();
    result.sample_aspect_den = profile.sample_aspect_den();
    result.display_aspect_num = profile.display_aspect_num();
    result.display_aspect_den = profile.display_aspect_den();
    result.colorspace = profile.colorspace();
    return result;
}

// static
bool ProfilesDialog::existingProfileDescription(const QString &desc)
{
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");

    // List the Mlt profiles
    QDir mltDir(KdenliveSettings::mltpath());
    QStringList profilesFiles = mltDir.entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(mltDir.absoluteFilePath(profilesFiles.at(i)), KConfig::SimpleConfig);
        if (desc == confFile.entryMap().value(QStringLiteral("description"))) {
            return true;
        }
    }

    // List custom profiles
    QStringList customProfiles = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("profiles/"), QStandardPaths::LocateDirectory);
    for (int i = 0; i < customProfiles.size(); ++i) {
        QDir customDir(customProfiles.at(i));
        profilesFiles = customDir.entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profilesFiles.size(); ++j) {
            KConfig confFile(customDir.absoluteFilePath(profilesFiles.at(j)), KConfig::SimpleConfig);
            if (desc == confFile.entryMap().value(QStringLiteral("description"))) {
                return true;
            }
        }
    }
    return false;
}

// static
QString ProfilesDialog::existingProfile(const MltVideoProfile &profile)
{
    // Check if the profile has a matching entry in existing ones
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");

    // Check the Mlt profiles
    QDir mltDir(KdenliveSettings::mltpath());
    QStringList profilesFiles = mltDir.entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        MltVideoProfile test = getProfileFromPath(mltDir.absoluteFilePath(profilesFiles.at(i)), profilesFiles.at(i));
        if (!test.isValid()) {
            continue;
        }
        if (test == profile) {
            return profilesFiles.at(i);
        }
    }

    // Check custom profiles
    QStringList customProfiles = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("profiles/"), QStandardPaths::LocateDirectory);
    for (int i = 0; i < customProfiles.size(); ++i) {
        QDir customDir(customProfiles.at(i));
        profilesFiles = customDir.entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profilesFiles.size(); ++j) {
            QString path = customDir.absoluteFilePath(profilesFiles.at(j));
            MltVideoProfile test = getProfileFromPath(path, path);
            if (!test.isValid()) {
                continue;
            }
            if (test == profile) {
                return path;
            }
        }
    }
    return QString();
}

// static
QList<MltVideoProfile> ProfilesDialog::profilesList()
{
    // Check if the profile has a matching entry in existing ones
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QList<MltVideoProfile> list;
    // Check the Mlt profiles
    QDir mltDir(KdenliveSettings::mltpath());
    QStringList profilesFiles = mltDir.entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        MltVideoProfile test = getProfileFromPath(mltDir.absoluteFilePath(profilesFiles.at(i)), profilesFiles.at(i));
        if (!test.isValid()) {
            continue;
        }
        list << test;
    }

    // Check custom profiles
    qDebug() << "searching custom in "<<QStandardPaths::AppDataLocation;
    qDebug() << "name" << QStandardPaths::displayName(QStandardPaths::AppDataLocation);
    qDebug() << "loc" << QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QStringList customProfiles = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("profiles/"), QStandardPaths::LocateDirectory);
    for (int i = 0; i < customProfiles.size(); ++i) {
        QDir customDir(customProfiles.at(i));
        profilesFiles = customDir.entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profilesFiles.size(); ++j) {
            QString path = customDir.absoluteFilePath(profilesFiles.at(j));
            MltVideoProfile test = getProfileFromPath(path, path);
            if (!test.isValid()) {
                continue;
            }
            list << test;
        }
    }
    return list;
}


// static
QMap< QString, QString > ProfilesDialog::getSettingsFromFile(const QString &path)
{
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    profilesFilter << QStringLiteral("*");
    QDir mltDir(KdenliveSettings::mltpath());
    if (!path.contains(QLatin1Char('/'))) {
        // This is an MLT profile
        KConfig confFile(mltDir.absoluteFilePath(path), KConfig::SimpleConfig);
        return confFile.entryMap();
    } else {
        // This is a custom profile
        KConfig confFile(path, KConfig::SimpleConfig);
        return confFile.entryMap();
    }
}

// static
bool ProfilesDialog::matchProfile(int width, int height, double fps, double par, bool isImage, const MltVideoProfile &profile)
{
    int profileWidth;
    if (isImage) {
        // when using image, compare with display width
        profileWidth = profile.height * profile.display_aspect_num / profile.display_aspect_den;
    } else {
        profileWidth = profile.width;
    }
    if (width != profileWidth || height != profile.height || (fps > 0 && qAbs((double) profile.frame_rate_num / profile.frame_rate_den - fps) > 0.4) || (!isImage && par > 0 && qAbs((double) profile.sample_aspect_num / profile.sample_aspect_den - par) > 0.1)) {
        return false;
    }
    return true;
}

// static
QMap<QString, QString> ProfilesDialog::getProfilesFromProperties(int width, int height, double fps, double par, bool useDisplayWidth)
{
    QStringList profilesNames;
    QStringList profilesFiles;
    QStringList profilesFilter;
    QMap<QString, QString> result;
    profilesFilter << QStringLiteral("*");
    // List the Mlt profiles
    QDir mltDir(KdenliveSettings::mltpath());
    profilesFiles = mltDir.entryList(profilesFilter, QDir::Files);
    for (int i = 0; i < profilesFiles.size(); ++i) {
        KConfig confFile(mltDir.absoluteFilePath(profilesFiles.at(i)), KConfig::SimpleConfig);
        QMap< QString, QString > values = confFile.entryMap();
        int profileWidth;
        if (useDisplayWidth) {
            profileWidth = values.value(QStringLiteral("height")).toInt() * values.value(QStringLiteral("display_aspect_num")).toInt() / values.value(QStringLiteral("display_aspect_den")).toInt();
        } else {
            profileWidth = values.value(QStringLiteral("width")).toInt();
        }
        if (profileWidth == width && values.value(QStringLiteral("height")).toInt() == height) {
            double profile_fps = values.value(QStringLiteral("frame_rate_num")).toDouble() / values.value(QStringLiteral("frame_rate_den")).toDouble();
            double profile_par = values.value(QStringLiteral("sample_aspect_num")).toDouble() / values.value(QStringLiteral("sample_aspect_den")).toDouble();
            if ((fps <= 0 || qAbs(profile_fps - fps) < 0.5) && (par <= 0 || qAbs(profile_par - par) < 0.1)) {
                result.insert(profilesFiles.at(i), values.value(QStringLiteral("description")));
            }
        }
    }

    // List custom profiles
    QStringList customProfiles = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("profiles/"), QStandardPaths::LocateDirectory);
    for (int i = 0; i < customProfiles.size(); ++i) {
        QStringList profiles = QDir(customProfiles.at(i)).entryList(profilesFilter, QDir::Files);
        for (int j = 0; j < profiles.size(); ++j) {
            KConfig confFile(customProfiles.at(i) + profiles.at(j), KConfig::SimpleConfig);
            QMap< QString, QString > values = confFile.entryMap();
            int profileWidth;
            if (useDisplayWidth) {
                profileWidth = values.value(QStringLiteral("height")).toInt() * values.value(QStringLiteral("display_aspect_num")).toInt() / values.value(QStringLiteral("display_aspect_den")).toInt();
            } else {
                profileWidth = values.value(QStringLiteral("width")).toInt();
            }
            if (profileWidth == width && values.value(QStringLiteral("height")).toInt() == height) {
                double profile_fps = values.value(QStringLiteral("frame_rate_num")).toDouble() / values.value(QStringLiteral("frame_rate_den")).toDouble();
                double profile_par = values.value(QStringLiteral("sample_aspect_num")).toDouble() / values.value(QStringLiteral("sample_aspect_den")).toDouble();
                if ((fps <= 0 || qAbs(profile_fps - fps) < 0.5) && (par <= 0 || qAbs(profile_par - par) < 0.1)) {
                    result.insert(customProfiles.at(i) + profiles.at(j), values.value(QStringLiteral("description")));
                }
            }
        }
    }
    return result;
}

// static
void ProfilesDialog::saveProfile(MltVideoProfile &profile, QString profilePath)
{
    if (profilePath.isEmpty()) {
        int i = 0;
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }
        QString customName = QStringLiteral("customprofile");
        profilePath =  dir.absoluteFilePath(customName + QString::number(i));
        while (QFile::exists(profilePath)) {
            ++i;
            profilePath = dir.absoluteFilePath(customName + QString::number(i));
        }
    }
    QFile file(profilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(nullptr, i18n("Cannot open file %1", profilePath));
        return;
    }
    QTextStream out(&file);
    out << "description=" << profile.description << '\n' << "frame_rate_num=" << profile.frame_rate_num << '\n' << "frame_rate_den=" << profile.frame_rate_den << '\n' << "width=" << profile.width << '\n' << "height=" << profile.height << '\n' << "progressive=" << profile.progressive << '\n' << "sample_aspect_num=" << profile.sample_aspect_num << '\n' << "sample_aspect_den=" << profile.sample_aspect_den << '\n' << "display_aspect_num=" << profile.display_aspect_num << '\n' << "display_aspect_den=" << profile.display_aspect_den << '\n' << "colorspace=" << profile.colorspace << '\n';
    if (file.error() != QFile::NoError) {
        KMessageBox::error(nullptr, i18n("Cannot write to file %1", profilePath));
    }
    file.close();
    profile.path = profilePath;
}

void ProfilesDialog::slotUpdateDisplay(QString currentProfile)
{
    if (askForSave() == false) {
        m_view.profiles_list->blockSignals(true);
        m_view.profiles_list->setCurrentIndex(m_selectedProfileIndex);
        m_view.profiles_list->blockSignals(false);
        return;
    }
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    m_selectedProfileIndex = m_view.profiles_list->currentIndex();
    if (currentProfile.isEmpty()) {
        currentProfile = m_view.profiles_list->itemData(m_view.profiles_list->currentIndex()).toString();
    }
    m_isCustomProfile = currentProfile.contains(QLatin1Char('/'));
    m_view.button_create->setEnabled(true);
    m_view.button_delete->setEnabled(m_isCustomProfile);
    m_view.properties->setEnabled(m_isCustomProfile);
    m_view.button_save->setEnabled(m_isCustomProfile);
    QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(currentProfile);
    m_view.description->setText(values.value(QStringLiteral("description")));
    m_view.size_w->setValue(values.value(QStringLiteral("width")).toInt());
    m_view.size_h->setValue(values.value(QStringLiteral("height")).toInt());
    m_view.aspect_num->setValue(values.value(QStringLiteral("sample_aspect_num")).toInt());
    m_view.aspect_den->setValue(values.value(QStringLiteral("sample_aspect_den")).toInt());
    m_view.display_num->setValue(values.value(QStringLiteral("display_aspect_num")).toInt());
    m_view.display_den->setValue(values.value(QStringLiteral("display_aspect_den")).toInt());
    m_view.frame_num->setValue(values.value(QStringLiteral("frame_rate_num")).toInt());
    m_view.frame_den->setValue(values.value(QStringLiteral("frame_rate_den")).toInt());
    m_view.progressive->setChecked(values.value(QStringLiteral("progressive")).toInt());
    if (values.value(QStringLiteral("progressive")).toInt()) {
        m_view.fields->setText(locale.toString((double) values.value(QStringLiteral("frame_rate_num")).toInt() / values.value(QStringLiteral("frame_rate_den")).toInt(), 'f', 2));
    } else {
        m_view.fields->setText(locale.toString((double) 2 * values.value(QStringLiteral("frame_rate_num")).toInt() / values.value(QStringLiteral("frame_rate_den")).toInt(), 'f', 2));
    }

    int colorix = m_view.colorspace->findData(values.value(QStringLiteral("colorspace")).toInt());
    if (colorix > -1) {
        m_view.colorspace->setCurrentIndex(colorix);
    }
    m_profileIsModified = false;
}

//static
QString ProfilesDialog::getColorspaceDescription(int colorspace)
{
    //TODO: should the descriptions be translated?
    switch (colorspace) {
    case 601:
        return QStringLiteral("ITU-R 601");
    case 709:
        return QStringLiteral("ITU-R 709");
    case 240:
        return QStringLiteral("SMPTE240M");
    default:
        return i18n("Unknown");
    }
}

//static
int ProfilesDialog::getColorspaceFromDescription(const QString &description)
{
    //TODO: should the descriptions be translated?
    if (description == QLatin1String("SMPTE240M")) {
        return 240;
    } else if (description == QLatin1String("ITU-R 709")) {
        return 709;
    }
    return 601;
}

