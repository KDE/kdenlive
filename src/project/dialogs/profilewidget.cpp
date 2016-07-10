/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "profilewidget.h"
#include "utils/KoIconUtils.h"

#include <QLabel>
#include <QComboBox>
#include <QDebug>
#include <QListWidgetItem>
#include <KLocalizedString>

ProfileWidget::ProfileWidget(QWidget* parent) :
        QWidget(parent)
{
    setupUi(this);
    manage_profiles->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    manage_profiles->setToolTip(i18n("Manage project profiles"));
    connect(manage_profiles, SIGNAL(clicked(bool)), this, SLOT(slotEditProfiles()));
    connect(standard, SIGNAL(activated(int)), this, SLOT(updateList()));
    connect(profile_list, SIGNAL(activated(int)), this, SLOT(updateDisplay()));
}

ProfileWidget::~ProfileWidget()
{
}

void ProfileWidget::loadProfile(const QString &profile)
{
    standard->blockSignals(true);
    standard->clear();
    QList <MltVideoProfile> list = ProfilesDialog::profilesList();
    for (int i = 0; i < list.count(); i++) {
        MltVideoProfile prof = list.at(i);
        if (prof.path == profile) {
            m_currentProfile = prof;
        }
        switch(prof.height) {
            case 2160:
                m_list4K.append(prof);
                break;
            case 1440:
                m_list2K.append(prof);
                break;
            case 1080:
                if (prof.width == 1920)
                    m_listFHD.append(prof);
                else
                    m_listCustom.append(prof);
                break;
            case 720:
                m_listHD.append(prof);
                break;
            case 576:
            case 480:
                if (prof.display_aspect_num == 4)
                    m_listSD.append(prof);
                else if (prof.display_aspect_num == 16)
                    m_listSDWide.append(prof);
                else
                    m_listCustom.append(prof);
                break;
            default:
                m_listCustom.append(prof);
                break;
        }
    }
    if (!m_list4K.isEmpty()) {
        standard->addItem(i18n("4K UHD 2160"), Std4K);
    }
    if (!m_list2K.isEmpty()) {
        standard->addItem(i18n("2.5K QHD 1440"), Std2K);
    }
    if (!m_listFHD.isEmpty()) {
        standard->addItem(i18n("Full HD 1080"), StdFHD);
    }
    if (!m_listHD.isEmpty()) {
        standard->addItem(i18n("HD 720"), StdHD);
    }
    if (!m_listSD.isEmpty()) {
        standard->addItem(i18n("SD/DVD 4:3"), StdSD);
    }
    if (!m_listSDWide.isEmpty()) {
        standard->addItem(i18n("SD/DVD 16:9"), StdSDWide);
    }
    if (!m_listCustom.isEmpty()) {
        standard->addItem(i18n("Custom"), StdCustom);
    }
    standard->blockSignals(false);
    VIDEOSTD std = getStandard(m_currentProfile);
    standard->setCurrentIndex(standard->findData(std));
    updateList();
}

void ProfileWidget::updateList()
{
    profile_list->blockSignals(true);
    profile_list->clear();
    VIDEOSTD std = (VIDEOSTD) standard->currentData().toInt();
    double fps = m_currentProfile.frame_rate_num / m_currentProfile.frame_rate_den;
    bool progressive = m_currentProfile.progressive;
    QString similarProfile;
    QList <MltVideoProfile> currentStd;
    switch (std) {
        case Std4K:
            currentStd = m_list4K;
            break;
        case Std2K:
            currentStd = m_list2K;
            break;
        case StdFHD:
            currentStd = m_listFHD;
            break;
        case StdHD:
            currentStd = m_listHD;
            break;
        case StdSD:
            currentStd = m_listSD;
            break;
        case StdSDWide:
            currentStd = m_listSDWide;
            break;
        default:
            currentStd = m_listCustom;
            break;
    }
    QMap <QString, QString> profiles;
    for (int i = 0; i < currentStd.count(); i++) {
        MltVideoProfile prof = currentStd.at(i);
        profiles.insert(prof.dialogDescriptiveString(), prof.path);
        if (similarProfile.isEmpty()) {
            if (fps == prof.frame_rate_num / prof.frame_rate_den && progressive == prof.progressive)
                similarProfile = prof.path;
        }
    }
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        profile_list->addItem(i.key(), i.value());
    }
    profile_list->blockSignals(false);
    int ix = profile_list->findData(m_currentProfile.path);
    if (ix == -1 && !similarProfile.isEmpty())
        ix = profile_list->findData(similarProfile);
    profile_list->setCurrentIndex(ix > -1 ? ix : 0);
    updateDisplay();
}

ProfileWidget::VIDEOSTD ProfileWidget::getStandard(MltVideoProfile profile)
{
    switch(profile.height) {
        case 2160:
            return Std4K;
            break;
        case 1440:
            return Std2K;
            break;
        case 1080:
            return StdFHD;
            break;
        case 720:
            return StdHD;
            break;
        case 576:
        case 480:
            if (profile.display_aspect_num == 4)
                return StdSD;
            else if (profile.display_aspect_num == 16)
                return StdSDWide;
            else
                return StdCustom;
            break;
        default:
            return StdCustom;
            break;
    }
}

const QString ProfileWidget::selectedProfile() const
{
    return m_currentProfile.path;
}

void ProfileWidget::updateDisplay()
{
    QString profilePath = profile_list->currentData().toString();
    detailsTree->clear();
    if (profilePath.isEmpty()) {
        return;
    }
    m_currentProfile = ProfilesDialog::getVideoProfile(profilePath);
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    detailsTree->addTopLevelItem(new QTreeWidgetItem(QStringList() << i18n("Frame Size") << QString("%1x%2").arg(m_currentProfile.width).arg(m_currentProfile.height)));
    detailsTree->addTopLevelItem(new QTreeWidgetItem(QStringList() << i18n("Frame Rate") << QString("%1/%2").arg(m_currentProfile.frame_rate_num).arg(m_currentProfile.frame_rate_den)));
    detailsTree->addTopLevelItem(new QTreeWidgetItem(QStringList() << i18n("Display Ratio") << QString("%1/%2").arg(m_currentProfile.display_aspect_num).arg(m_currentProfile.display_aspect_den)));
    detailsTree->addTopLevelItem(new QTreeWidgetItem(QStringList() << i18n("Sample Ratio") << QString("%1/%2").arg(m_currentProfile.sample_aspect_num).arg(m_currentProfile.sample_aspect_den)));
    if (m_currentProfile.progressive == 1) {
        detailsTree->addTopLevelItem(new QTreeWidgetItem(QStringList() << i18n("Scanning") << i18n("Progressive")));
    } else {
        QString fs = locale.toString(2.0 * m_currentProfile.frame_rate_num / m_currentProfile.frame_rate_den, 'f', 2);
        detailsTree->addTopLevelItem(new QTreeWidgetItem(QStringList() << i18n("Scanning") << i18n("Interlaced (%1 fields/s)", fs)));
    }
    detailsTree->addTopLevelItem(new QTreeWidgetItem(QStringList() << i18n("Color Space") << ProfilesDialog::getColorspaceDescription(m_currentProfile.colorspace)));
    detailsTree->resizeColumnToContents(0);
    detailsTree->setFixedHeight(detailsTree->sizeHintForRow(0) * detailsTree->topLevelItemCount() + 2 * detailsTree->frameWidth());
}

void ProfileWidget::slotEditProfiles()
{
    ProfilesDialog *w = new ProfilesDialog(m_currentProfile.path);
    w->exec();
    loadProfile(m_currentProfile.path);
    delete w;
}


