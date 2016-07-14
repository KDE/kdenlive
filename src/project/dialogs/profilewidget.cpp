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
#include "kxmlgui_version.h"

#include <QLabel>
#include <QComboBox>
#include <QDebug>
#include <QListWidgetItem>
#include <KLocalizedString>
#include <KMessageWidget>

#if KXMLGUI_VERSION_MINOR > 15 || KXMLGUI_VERSION_MAJOR > 5
#include <KCollapsibleGroupBox>
#endif

ProfileWidget::ProfileWidget(QWidget* parent) :
        QWidget(parent)
{
    QGridLayout *lay = new QGridLayout;
    QLabel *lab = new QLabel(i18n("Resolution"), this);
    lay->addWidget(lab, 0, 0);
    lab = new QLabel(i18n("Frame Rate"), this);
    lay->addWidget(lab, 1, 0);
    m_standard = new QComboBox;
    lay->addWidget(m_standard, 0, 1);
    QToolButton *manage_profiles = new QToolButton(this);
    lay->addWidget(manage_profiles, 0, 2);
    m_rate_list = new QComboBox;
    lay->addWidget(m_rate_list, 1, 1, 1, 2);
    lab = new QLabel(i18n("Scanning"), this);
    lay->addWidget(lab, 2, 0);
    m_interlaced = new QCheckBox(i18n("Interlaced"), this);
    lay->addWidget(m_interlaced, 2, 1, 1, 2);
    lay->setColumnStretch(1, 10);

    m_customSizeLabel = new QLabel(i18n("Frame Size"), this);
    m_customSize = new QComboBox;
    m_display_list = new QComboBox;
    QLabel *displayLab = new QLabel(i18n("Display Ratio"), this);
    m_sample_list = new QComboBox;
    QLabel *sampleLab = new QLabel(i18n("Sample Ratio"), this);
    m_color_list = new QComboBox;
    QLabel *colorLab = new QLabel(i18n("Color Space"), this);
    m_detailsLayout = new QGridLayout;
    m_detailsLayout->addWidget(m_customSizeLabel, 0, 0);
    m_detailsLayout->addWidget(m_customSize, 0, 1);
    m_detailsLayout->addWidget(displayLab, 1, 0);
    m_detailsLayout->addWidget(m_display_list, 1, 1);
    m_detailsLayout->addWidget(sampleLab, 2, 0);
    m_detailsLayout->addWidget(m_sample_list, 2, 1);
    m_detailsLayout->addWidget(colorLab, 3, 0);
    m_detailsLayout->addWidget(m_color_list, 3, 1);
    m_detailsLayout->setColumnStretch(1, 10);

#if KXMLGUI_VERSION_MINOR > 15 || KXMLGUI_VERSION_MAJOR > 5
    KCollapsibleGroupBox *details_box = new KCollapsibleGroupBox(this);
    details_box->setTitle(i18n("Details"));
    connect(this, &ProfileWidget::showDetails, details_box, &KCollapsibleGroupBox::expand);
    details_box->setLayout(m_detailsLayout);
    lay->addWidget(details_box, 3, 0, 1, 3);
#else
    QGroupBox *box = new QGroupBox(i18n("Details"), this);
    box->setLayout(m_detailsLayout);
    lay->addWidget(box, 3, 0, 1, 3);
#endif
    m_errorMessage = new KMessageWidget(i18n("No matching profile found"), this);
    m_errorMessage->setMessageType(KMessageWidget::Warning);
    m_errorMessage->setCloseButtonVisible(false);
    lay->addWidget(m_errorMessage, 4, 0, 1, 3);
    m_errorMessage->hide();
    setLayout(lay);
    manage_profiles->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    manage_profiles->setToolTip(i18n("Manage project profiles"));
    connect(manage_profiles, SIGNAL(clicked(bool)), this, SLOT(slotEditProfiles()));
    connect(m_standard, SIGNAL(activated(int)), this, SLOT(updateList()));
    connect(m_rate_list, SIGNAL(activated(int)), this, SLOT(ratesUpdated()));
    connect(m_customSize, SIGNAL(activated(int)), this, SLOT(slotCheckInterlace()));
    connect(m_interlaced, SIGNAL(clicked(bool)), this, SLOT(updateDisplay()));
    connect(m_display_list, SIGNAL(activated(int)), this, SLOT(selectProfile()));
    connect(m_sample_list, SIGNAL(activated(int)), this, SLOT(selectProfile()));
    connect(m_color_list, SIGNAL(activated(int)), this, SLOT(selectProfile()));
}

ProfileWidget::~ProfileWidget()
{
}

void ProfileWidget::loadProfile(QString profile)
{
    if (profile.isEmpty()) {
        profile = KdenliveSettings::current_profile();
        if (profile.isEmpty()) {
            profile = QStringLiteral("dv_pal");
        }
    }
    m_standard->blockSignals(true);
    m_standard->clear();
    m_list4K.clear();
    m_list4KWide.clear();
    m_list4KDCI.clear();
    m_list2K.clear();
    m_listFHD.clear();
    m_listHD.clear();
    m_listSD.clear();
    m_listSDWide.clear();
    m_listCustom.clear();
    m_currentProfile = ProfilesDialog::getVideoProfile(profile);
    QList <MltVideoProfile> list = ProfilesDialog::profilesList();
    for (int i = 0; i < list.count(); i++) {
        MltVideoProfile prof = list.at(i);
        switch(prof.height) {
            case 2160:
                if (prof.width == 3840) {
                    m_list4K.append(prof);
                } else if (prof.width == 5120) {
                    m_list4KWide.append(prof);
                } else if (prof.width == 4096) {
                    m_list4KDCI.append(prof);
                } else {
                    m_listCustom.append(prof);
                }
                break;
            case 1440:
                m_list2K.append(prof);
                break;
            case 1080:
                if (prof.width == 1920) {
                    m_listFHD.append(prof);
                } else {
                    m_listCustom.append(prof);
                }
                break;
            case 720:
                m_listHD.append(prof);
                break;
            case 576:
            case 480:
                if (prof.width != 720)
                    m_listCustom.append(prof);
                else if (prof.display_aspect_num == 4) {
                    m_listSD.append(prof);
                } else if (prof.display_aspect_num == 16) {
                    m_listSDWide.append(prof);
                } else {
                    m_listCustom.append(prof);
                }
                break;
            default:
                m_listCustom.append(prof);
                break;
        }
    }
    if (!m_list4KDCI.isEmpty()) {
        m_standard->addItem(i18n("4K DCI 2160"), Std4KDCI);
    }
    if (!m_list4KWide.isEmpty()) {
        m_standard->addItem(i18n("4K UHD Wide 2160"), Std4KWide);
    }
    if (!m_list4K.isEmpty()) {
        m_standard->addItem(i18n("4K UHD 2160"), Std4K);
    }
    if (!m_list2K.isEmpty()) {
        m_standard->addItem(i18n("2.5K QHD 1440"), Std2K);
    }
    if (!m_listFHD.isEmpty()) {
        m_standard->addItem(i18n("Full HD 1080"), StdFHD);
    }
    if (!m_listHD.isEmpty()) {
        m_standard->addItem(i18n("HD 720"), StdHD);
    }
    if (!m_listSD.isEmpty()) {
        m_standard->addItem(i18n("SD/DVD"), StdSD);
    }
    if (!m_listSDWide.isEmpty()) {
        m_standard->addItem(i18n("SD/DVD Widescreen"), StdSDWide);
    }
    if (!m_listCustom.isEmpty()) {
        m_standard->addItem(i18n("Custom"), StdCustom);
    }
    m_standard->blockSignals(false);
    VIDEOSTD std = getStandard(m_currentProfile);
    m_standard->setCurrentIndex(m_standard->findData(std));
    updateList();
}


QList <MltVideoProfile> ProfileWidget::getList(VIDEOSTD std)
{
    switch (std) {
        case Std4KDCI:
            return m_list4KDCI;
            break;
        case Std4KWide:
            return m_list4KWide;
            break;
        case Std4K:
            return m_list4K;
            break;
        case Std2K:
            return m_list2K;
            break;
        case StdFHD:
            return m_listFHD;
            break;
        case StdHD:
            return m_listHD;
            break;
        case StdSD:
            return m_listSD;
            break;
        case StdSDWide:
            return m_listSDWide;
            break;
        default:
            return m_listCustom;
            break;
    }
}

void ProfileWidget::updateList()
{
    m_rate_list->blockSignals(true);
    VIDEOSTD std = (VIDEOSTD) m_standard->currentData().toInt();
    QString currentFps;
    if (m_rate_list->count() == 0) {
        if (m_currentProfile.frame_rate_num % m_currentProfile.frame_rate_den == 0) {
            currentFps = QString::number(m_currentProfile.frame_rate_num / m_currentProfile.frame_rate_den);
        } else {
            currentFps = QString::number((double)m_currentProfile.frame_rate_num / m_currentProfile.frame_rate_den, 'f', 2);
        }
    } else {
        currentFps = m_rate_list->currentText();
    }
    m_rate_list->clear();
    QString similarProfile;
    QList <MltVideoProfile> currentStd = getList(std);
    QStringList frameRates;
    QString text;
    for (int i = 0; i < currentStd.count(); i++) {
        MltVideoProfile prof = currentStd.at(i);
        if (prof.frame_rate_num % prof.frame_rate_den == 0) {
            text = QString::number(prof.frame_rate_num / prof.frame_rate_den);
        } else {
            text = QString::number((double)prof.frame_rate_num / prof.frame_rate_den, 'f', 2);
        }
        if (!frameRates.contains(text))
            frameRates << text;
    }
    if (frameRates.isEmpty()) {
        m_rate_list->setEnabled(false);
        return;
    }
    m_rate_list->setEnabled(true);
    qSort(frameRates);
    m_rate_list->addItems(frameRates);
    m_rate_list->blockSignals(false);
    int ix = m_rate_list->findText(currentFps);
    m_rate_list->setCurrentIndex(ix > -1 ? ix : 0);
    if (std == StdCustom) {
        emit showDetails();
    }
    ratesUpdated();
}

void ProfileWidget::ratesUpdated()
{
    VIDEOSTD std = (VIDEOSTD) m_standard->currentData().toInt();
    QList <MltVideoProfile> currentStd = getList(std);
    // insert all frame sizes related to frame rate
    m_customSize->clear();
    m_customSize->addItems(getFrameSizes(currentStd, m_rate_list->currentText()));
    if (std == StdCustom) {
        int ix = m_customSize->findText(QString("%1x%2").arg(m_currentProfile.width).arg(m_currentProfile.height));
        if (ix > 0) {
            m_customSize->setCurrentIndex(ix);
        }
    }
    checkInterlace(currentStd, m_customSize->currentText(), m_rate_list->currentText());
    updateDisplay();
}



QStringList ProfileWidget::getFrameSizes(QList <MltVideoProfile> currentStd, const QString &rate)
{
    QStringList sizes;
    for (int i = 0; i < currentStd.count(); i++) {
        MltVideoProfile prof = currentStd.at(i);
        QString fps;
        if (prof.frame_rate_num % prof.frame_rate_den == 0) {
            fps = QString::number(prof.frame_rate_num / prof.frame_rate_den);
        } else {
            fps = QString::number((double)prof.frame_rate_num / prof.frame_rate_den, 'f', 2);
        }
        if (fps != rate)
            continue;
        QString res = QString("%1x%2").arg(prof.width).arg(prof.height);
        if (!sizes.contains(res))
            sizes << res;
    }
    qSort(sizes);
    return sizes;
}

void ProfileWidget::slotCheckInterlace()
{
    VIDEOSTD std = (VIDEOSTD) m_standard->currentData().toInt();
    checkInterlace(getList(std), m_customSize->currentText(), m_rate_list->currentText());
    updateDisplay();
}

void ProfileWidget::checkInterlace(QList <MltVideoProfile> currentStd, const QString &size, const QString &rate)
{
    bool allowInterlaced = false;
    bool allowProgressive = false;
    for (int i = 0; i < currentStd.count(); i++) {
        MltVideoProfile prof = currentStd.at(i);
        QString fps;
        if (prof.frame_rate_num % prof.frame_rate_den == 0) {
            fps = QString::number(prof.frame_rate_num / prof.frame_rate_den);
        } else {
            fps = QString::number((double)prof.frame_rate_num / prof.frame_rate_den, 'f', 2);
        }
        if (fps != rate)
            continue;
        QString res = QString("%1x%2").arg(prof.width).arg(prof.height);
        if (res != size)
            continue;
        if (prof.progressive)
            allowProgressive = true;
        else
            allowInterlaced = true;
    }
    m_interlaced->setChecked(!m_currentProfile.progressive);
    if (allowInterlaced && allowProgressive) {
        m_interlaced->setEnabled(true);
        m_interlaced->setChecked(false);
    } else {
        m_interlaced->setEnabled(false);
        if (m_interlaced->isChecked() && !allowInterlaced) {
            m_interlaced->setChecked(false);
        } else if (!m_interlaced->isChecked() && !allowProgressive) {
            m_interlaced->setChecked(true);
        }
    }
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
            if (profile.width != 720)
                return StdCustom;
            else if (profile.display_aspect_num == 4)
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
    VIDEOSTD std = (VIDEOSTD) m_standard->currentData().toInt();
    QList <MltVideoProfile> currentStd = getList(std);
    QList <MltVideoProfile> matching;
    QString rate = m_rate_list->currentText();
    QString size = m_customSize->currentText();
    bool interlaced = m_interlaced->isChecked();
    for (int i = 0; i < currentStd.count(); i++) {
        MltVideoProfile prof = currentStd.at(i);
        QString fps;
        if (prof.frame_rate_num % prof.frame_rate_den == 0) {
            fps = QString::number(prof.frame_rate_num / prof.frame_rate_den);
        } else {
            fps = QString::number((double)prof.frame_rate_num / prof.frame_rate_den, 'f', 2);
        }
        if (fps != rate)
            continue;
        QString res = QString("%1x%2").arg(prof.width).arg(prof.height);
        if (res != size)
            continue;
        if (prof.progressive == interlaced)
            continue;
        // Profile matches current properties
        matching << prof;
    }
    m_display_list->clear();
    m_sample_list->clear();
    m_color_list->clear();
    if (matching.isEmpty()) {
        m_errorMessage->animatedShow();
        return;
    }
    if (!m_errorMessage->isHidden()) {
        m_errorMessage->animatedHide();
    }
    QStringList displays;
    QStringList samples;
    QStringList colors;
    bool foundMatching = false;
    for (int i = 0; i < matching.count(); i++) {
        MltVideoProfile prof = matching.at(i);
        if (prof == m_currentProfile) {
            foundMatching = true;
        }
        displays << QString("%1:%2").arg(prof.display_aspect_num).arg(prof.display_aspect_den);
        samples << QString("%1:%2").arg(prof.sample_aspect_num).arg(prof.sample_aspect_den);
        colors << ProfilesDialog::getColorspaceDescription(prof.colorspace);
    }
    if (!foundMatching) {
        m_currentProfile = matching.first();
    }
    displays.removeDuplicates();
    samples.removeDuplicates();
    colors.removeDuplicates();
    qSort(displays);
    qSort(samples);
    qSort(colors);
    m_display_list->addItems(displays);
    m_sample_list->addItems(samples);
    m_color_list->addItems(colors);

    m_display_list->setCurrentText(QString("%1:%2").arg(m_currentProfile.display_aspect_num).arg(m_currentProfile.display_aspect_den));
    m_sample_list->setCurrentText(QString("%1:%2").arg(m_currentProfile.sample_aspect_num).arg(m_currentProfile.sample_aspect_den));
    m_color_list->setCurrentText(ProfilesDialog::getColorspaceDescription(m_currentProfile.colorspace));
}

void ProfileWidget::selectProfile()
{
    VIDEOSTD std = (VIDEOSTD) m_standard->currentData().toInt();
    QList <MltVideoProfile> currentStd = getList(std);
    QString rate = m_rate_list->currentText();
    QString size = m_customSize->currentText();
    QString display = m_display_list->currentText();
    QString sample = m_sample_list->currentText();
    QString color = m_color_list->currentText();
    bool interlaced = m_interlaced->isChecked();
    bool found = false;
    for (int i = 0; i < currentStd.count(); i++) {
        MltVideoProfile prof = currentStd.at(i);
        QString fps;
        if (prof.frame_rate_num % prof.frame_rate_den == 0) {
            fps = QString::number(prof.frame_rate_num / prof.frame_rate_den);
        } else {
            fps = QString::number((double)prof.frame_rate_num / prof.frame_rate_den, 'f', 2);
        }
        if (fps != rate)
            continue;
        QString res = QString("%1x%2").arg(prof.width).arg(prof.height);
        if (res != size)
            continue;
        if (prof.progressive == interlaced)
            continue;
        res = QString("%1:%2").arg(prof.display_aspect_num).arg(prof.display_aspect_den);
        if (res != display)
            continue;
        res = QString("%1:%2").arg(prof.sample_aspect_num).arg(prof.sample_aspect_den);
        if (res != sample)
            continue;
        res = ProfilesDialog::getColorspaceDescription(prof.colorspace);
        if (res != color)
            continue;
        found = true;
        m_currentProfile = prof;
        break;
    }
    if (!found) {
        m_errorMessage->animatedShow();
    } else if (!m_errorMessage->isHidden()) {
        m_errorMessage->animatedHide();
    }
}

void ProfileWidget::slotEditProfiles()
{
    ProfilesDialog *w = new ProfilesDialog(m_currentProfile.path);
    w->exec();
    loadProfile(m_currentProfile.path);
    delete w;
}


