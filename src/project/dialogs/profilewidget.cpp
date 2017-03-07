/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2017  Nicolas Carion
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
#include "profiles/tree/profiletreemodel.hpp"
#include "profiles/tree/profilefilter.hpp"
#include "profiles/profilerepository.hpp"
#include "profiles/profilemodel.hpp"
#include "utils/KoIconUtils.h"
#include "kxmlgui_version.h"

#include <QLabel>
#include <QComboBox>
#include <KLocalizedString>
#include <KMessageWidget>
#include <QTextEdit>

#include <KCollapsibleGroupBox>



ProfileWidget::ProfileWidget(QWidget *parent) :
    QWidget(parent)
{
    QVBoxLayout *lay = new QVBoxLayout;

    QHBoxLayout *labelLay = new QHBoxLayout;
    QLabel *title = new QLabel(i18n("Select the profile (preset) of the project"),this);
    labelLay->addWidget(title);
    labelLay->addStretch(1);

    QToolButton *manage_profiles = new QToolButton(this);
    labelLay->addWidget(manage_profiles);
    manage_profiles->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    manage_profiles->setToolTip(i18n("Manage project profiles"));
    connect(manage_profiles, &QAbstractButton::clicked, this, &ProfileWidget::slotEditProfiles);
    lay->addLayout(labelLay);


    QHBoxLayout *profileLay = new QHBoxLayout;

    m_treeView = new QTreeView(this);
    m_treeView->setMinimumSize(QSize(400, 400));
    m_treeModel = new ProfileTreeModel(this);
    m_filter = new ProfileFilter(this);
    m_filter->setSourceModel(m_treeModel);
    m_treeView->setModel(m_filter);
    for (int i = 1; i < m_treeModel->columnCount(); ++i) {
        m_treeView->hideColumn(i);
    }
    m_treeView->header()->hide();
    QItemSelectionModel *selectionModel = m_treeView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentRowChanged, this, &ProfileWidget::slotChangeSelection);
    connect(selectionModel, &QItemSelectionModel::selectionChanged,
            [&](const QItemSelection &selected, const QItemSelection &deselected){
                QModelIndex current, old;
                if (!selected.indexes().isEmpty()) {
                    current = selected.indexes().front();
                }
                if (!deselected.indexes().isEmpty()) {
                    old = deselected.indexes().front();
                }
                slotChangeSelection(current, old);
            });
    profileLay->addWidget(m_treeView, 8);

    m_descriptionPanel = new QTextEdit(this);
    m_descriptionPanel->setReadOnly(true);
    profileLay->addWidget(m_descriptionPanel, 10);

    lay->addLayout(profileLay);

    QGridLayout* filterLayout = new QGridLayout;
    filterLayout->setColumnStretch(0, 1);
    filterLayout->setColumnStretch(1, 10);
    filterLayout->setColumnStretch(2, 10);

    m_enableScanning = new QCheckBox(this);
    m_labelScanning = new QLabel(i18n("Scanning"), this);
    m_labelScanning->setEnabled(false);

    m_widScanning = new QButtonGroup(this);
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    m_but1Scanning = new QPushButton(i18n("Interlaced"));
    m_but1Scanning->setEnabled(false);
    m_but1Scanning->setCheckable(true);
    m_but1Scanning->setChecked(true);
    buttonLayout->addWidget(m_but1Scanning);
    m_widScanning->addButton(m_but1Scanning);
    m_widScanning->setId(m_but1Scanning, 0);
    m_but2Scanning = new QPushButton(i18n("Progressive"));
    m_but2Scanning->setEnabled(false);
    m_but2Scanning->setCheckable(true);
    m_but2Scanning->setChecked(false);
    buttonLayout->addWidget(m_but2Scanning);
    m_widScanning->addButton(m_but2Scanning);
    m_widScanning->setId(m_but2Scanning, 1);

    buttonLayout->setSpacing(0);
    buttonLayout->setContentsMargins(0,0,0,0);

    auto updateScanning = [&]() {
        m_labelScanning->setEnabled(m_enableScanning->isChecked());
        m_but1Scanning->setEnabled(m_enableScanning->isChecked());
        m_but2Scanning->setEnabled(m_enableScanning->isChecked());
        m_filter->setFilterInterlaced(m_enableScanning->isChecked(),
                                      m_widScanning->checkedId() == 0);
        slotFilterChanged();
    };
    connect(m_enableScanning, &QCheckBox::toggled, updateScanning);
    connect(m_widScanning, static_cast<void (QButtonGroup::*)(int,bool)>(&QButtonGroup::buttonToggled), updateScanning);

    filterLayout->addWidget(m_enableScanning, 0, 0);
    filterLayout->addWidget(m_labelScanning, 0, 1);
    filterLayout->addLayout(buttonLayout, 0, 2);

    m_enableFps = new QCheckBox(this);
    m_labelFps = new QLabel(i18n("Fps"), this);
    m_labelFps->setEnabled(false);

    m_widFps = new QComboBox(this);
    m_widFps->setEnabled(false);
    auto all_fps = ProfileRepository::get()->getAllFps();

    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    for (double fps : all_fps) {
        m_widFps->addItem(locale.toString(fps), fps);
    }

    auto updateFps = [&]() {
        m_labelFps->setEnabled(m_enableFps->isChecked());
        m_widFps->setEnabled(m_enableFps->isChecked());
        m_filter->setFilterFps(m_enableFps->isChecked(),
                               m_widFps->currentData().toDouble());
        slotFilterChanged();
    };
    connect(m_enableFps, &QCheckBox::toggled, updateFps);
    connect(m_widFps, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), updateFps);

    filterLayout->addWidget(m_enableFps, 1, 0);
    filterLayout->addWidget(m_labelFps, 1, 1);
    filterLayout->addWidget(m_widFps, 1, 2);


    KCollapsibleGroupBox *filter_box = new KCollapsibleGroupBox(this);
    filter_box->setTitle(i18n("Filter profiles"));
    filter_box->setLayout(filterLayout);
    lay->addWidget(filter_box);


    setLayout(lay);
}

ProfileWidget::~ProfileWidget()
{
}

void ProfileWidget::loadProfile(const QString& profile)
{
    auto index = m_treeModel->findProfile(profile);
    if (index.isValid()) {
        m_currentProfile = m_lastValidProfile = profile;
        //expand corresponding category
        auto parent = m_treeModel->parent(index);
        m_treeView->expand(m_filter->mapFromSource(parent));
        // select profile
        QItemSelectionModel *selection = m_treeView->selectionModel();
        selection->select(m_filter->mapFromSource(index), QItemSelectionModel::Select);
    }
}

const QString ProfileWidget::selectedProfile() const
{
    return m_currentProfile;
}

void ProfileWidget::slotEditProfiles()
{
    ProfilesDialog *w = new ProfilesDialog(m_currentProfile);
    w->exec();
    loadProfile(m_currentProfile);
    delete w;
}


void ProfileWidget::fillDescriptionPanel(const QString& profile_path)
{
    QString description;
    if (profile_path.isEmpty()) {
        description += i18n("No profile selected");
    } else {
        std::unique_ptr<ProfileModel> & profile = ProfileRepository::get()->getProfile(profile_path);

        description += i18n("<h5>Video Settings</h5>");
        description += i18n("<p style='font-size:small'>Frame size: %1 x %2 (%3)</p>",profile->width(), profile->height(), profile->dar());
        description += i18n("<p style='font-size:small'>Frame rate: %1 fps</p>",profile->fps());
        description += i18n("<p style='font-size:small'>Pixel Aspect Ratio: %1</p>",profile->sar());
        description += i18n("<p style='font-size:small'>Color Space: %1</p>",profile->colorspaceDescription());
        QString interlaced = i18n("yes");
        if (profile->progressive()) {
            interlaced = i18n("no");
        }
        description += i18n("<p style='font-size:small'>Interlaced : %1</p>", interlaced);
    }
    m_descriptionPanel->setHtml(description);
}

void ProfileWidget::slotChangeSelection(const QModelIndex &current, const QModelIndex &previous)
{
    auto originalIndex = m_filter->mapToSource(current);
    if (m_treeModel->parent(originalIndex) == QModelIndex()) {
        //in that case, we have selected a category, which we don't want
        QItemSelectionModel *selection = m_treeView->selectionModel();
        selection->select(previous, QItemSelectionModel::Select);
        return;
    }
    m_currentProfile = ProfileTreeModel::getProfile(originalIndex);
    if (!m_currentProfile.isEmpty()) {
        m_lastValidProfile = m_currentProfile;
    }
    fillDescriptionPanel(m_currentProfile);
}

bool ProfileWidget::trySelectProfile(const QString& profile)
{
    auto index = m_treeModel->findProfile(profile);
    if (index.isValid()) {
        // check if element is still visible
        if (m_filter->isVisible(index)) {
            //reselect
            QItemSelectionModel *selection = m_treeView->selectionModel();
            selection->select(m_filter->mapFromSource(index), QItemSelectionModel::Select);
            return true;
        } else {
            return false;
        }
    }
    return false;
}

void ProfileWidget::slotFilterChanged()
{
    //When filtering change, we must check if the current profile is still visible.
    if (!trySelectProfile(m_currentProfile)) {
        //we try to back-up the last valid profile
        if (!trySelectProfile(m_lastValidProfile)) {
            //Everything fails, we don't have any profile
            m_currentProfile = QString();
            fillDescriptionPanel(QString());
        }
    }
}
