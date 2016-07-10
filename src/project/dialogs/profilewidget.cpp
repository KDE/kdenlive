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
#include <KLocalizedString>

ProfileWidget::ProfileWidget(QWidget* parent) :
        QWidget(parent)
{
    setupUi(this);
    detailsTree->setVisible(false);
    manage_profiles->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    manage_profiles->setToolTip(i18n("Manage project profiles"));
    connect(manage_profiles, SIGNAL(clicked(bool)), this, SLOT(slotEditProfiles()));
    infoButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    connect(infoButton, SIGNAL(toggled(bool)), this, SLOT(slotShowDetails(bool)));
    connect(standard, SIGNAL(activated(int)), this, SLOT(updateFps()));
    connect(fps, SIGNAL(activated(int)), this, SLOT(updateStatus()));
    connect(interlaced, SIGNAL(toggled(bool)), this, SLOT(updateStatus()));
    connect(resolution, SIGNAL(activated(int)), this, SLOT(updateCustom()));
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
                m_listFHD.append(prof);
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
    for (int i = 0; i < m_listCustom.count(); i++) {
        MltVideoProfile prof = m_listCustom.at(i);
        resolution->addItem(prof.description, prof.path);
    }
    standard->blockSignals(false);
    updateCombos();
}

void ProfileWidget::updateCombos()
{
    VIDEOSTD std = getStandard(m_currentProfile);
    resolution->setVisible(std == StdCustom);
    standard->setCurrentIndex(standard->findData(std));
    updateFps();
    slotUpdateInfoDisplay();
}

void ProfileWidget::updateFps()
{
    VIDEOSTD std = (VIDEOSTD) standard->currentData().toInt();
    bool isCustom = (std == StdCustom);
    fps->setVisible(!isCustom);
    fps_label->setVisible(!isCustom);
    interlaced->setVisible(!isCustom);
    resolution->setVisible(isCustom);
    if (isCustom)
        return;
    QLocale locale;
    QString selectedFps = fps->currentText();
    if (selectedFps.isEmpty())
        selectedFps = locale.toString((double)m_currentProfile.frame_rate_num / m_currentProfile.frame_rate_den, 'f', 2);
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
    QStringList rates;
    for (int i = 0; i < currentStd.count(); i++) {
        MltVideoProfile prof = currentStd.at(i);
        QString frameRate = locale.toString((double)prof.frame_rate_num / prof.frame_rate_den, 'f', 2);
        if (!rates.contains(frameRate)) {
            rates << frameRate;
        }
    }
    qSort(rates);
    fps->blockSignals(true);
    fps->clear();
    fps->addItems(rates);
    fps->setCurrentIndex(fps->findText(selectedFps));
    fps->blockSignals(false);
    updateSelectedProfile(currentStd, fps->currentText());
}

void ProfileWidget::updateCustom()
{
    QString path = resolution->currentData().toString();
    m_currentProfile = ProfilesDialog::getVideoProfile(path);
    slotUpdateInfoDisplay();
}

void ProfileWidget::updateStatus()
{
    VIDEOSTD std = (VIDEOSTD) standard->currentData().toInt();
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
    updateSelectedProfile(currentStd, fps->currentText());
}

void ProfileWidget::updateSelectedProfile(QList <MltVideoProfile> list, QString rate)
{
    QLocale locale;
    QList <MltVideoProfile> matching;
    for (int i = 0; i < list.count(); i++) {
        MltVideoProfile prof = list.at(i);
        QString frameRate = locale.toString((double)prof.frame_rate_num / prof.frame_rate_den, 'f', 2);
        if (frameRate == rate) {
            matching << prof;
        }
    }
    if (matching.count() == 1) {
        // Only one match, disable interlaced checkbox
        m_currentProfile = matching.first();
        interlaced->setCheckState(m_currentProfile.progressive ? Qt::Unchecked : Qt::Checked);
        interlaced->setEnabled(false);
    } else {
        bool interlaceFound = false;
        bool progressiveFound = false;
        for (int i = 0; i < matching.count(); i++) {
            MltVideoProfile prof = matching.at(i);
            if (prof.progressive) {
                progressiveFound = true;
                if (interlaced->checkState() == Qt::Unchecked) {
                    m_currentProfile = prof;
                }
            } else {
                interlaceFound = true;
                if (interlaced->checkState() == Qt::Checked) {
                    m_currentProfile = prof;
                }
            }
        }
        interlaced->setEnabled(progressiveFound && interlaceFound);
    }
    slotUpdateInfoDisplay();
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

void ProfileWidget::slotUpdateInfoDisplay()
{
    if (!detailsTree->isVisible())
        return;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    detailsTree->clear();
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

void ProfileWidget::slotShowDetails(bool show)
{
    detailsTree->setVisible(show);
    if (show)
        slotUpdateInfoDisplay();
}

void ProfileWidget::updateSelection()
{
}

