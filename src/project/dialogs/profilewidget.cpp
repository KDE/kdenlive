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
#include <QTextEdit>
#include <QSplitter>

ProfileWidget::ProfileWidget(QWidget *parent) :
    QWidget(parent)
{
    m_originalProfile = QStringLiteral("invalid");
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    QVBoxLayout *lay = new QVBoxLayout;

    QHBoxLayout *labelLay = new QHBoxLayout;
    QLabel* fpsLabel = new QLabel(i18n("Fps"),this);
    fpsFilt = new QComboBox(this);
    fpsLabel->setBuddy(fpsFilt);
    labelLay->addWidget(fpsLabel);
    labelLay->addWidget(fpsFilt);

    QLabel* scanningLabel = new QLabel(i18n("Scanning"),this);
    scanningFilt = new QComboBox(this);
    scanningLabel->setBuddy(scanningFilt);
    labelLay->addWidget(scanningLabel);
    labelLay->addWidget(scanningFilt);
    labelLay->addStretch(1);

    QToolButton *manage_profiles = new QToolButton(this);
    labelLay->addWidget(manage_profiles);
    manage_profiles->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    manage_profiles->setToolTip(i18n("Manage project profiles"));
    connect(manage_profiles, &QAbstractButton::clicked, this, &ProfileWidget::slotEditProfiles);
    lay->addLayout(labelLay);


    QSplitter *profileSplitter = new QSplitter;

    m_treeView = new QTreeView(this);
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
    profileSplitter->addWidget(m_treeView);
    m_treeView->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_descriptionPanel = new QTextEdit(this);
    m_descriptionPanel->setReadOnly(true);
    m_descriptionPanel->viewport()->setCursor(Qt::ArrowCursor);
    m_descriptionPanel->viewport()->setBackgroundRole(QPalette::Mid);
    m_descriptionPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_descriptionPanel->setFrameStyle(QFrame::NoFrame);
    profileSplitter->addWidget(m_descriptionPanel);

    lay->addWidget(profileSplitter);
    profileSplitter->setStretchFactor(0, 2);
    profileSplitter->setStretchFactor(1, 1);
    auto all_fps = ProfileRepository::get()->getAllFps();

    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    fpsFilt->addItem("Any", -1);
    for (double fps : all_fps) {
        fpsFilt->addItem(locale.toString(fps), fps);
    }
    auto updateFps = [&]() {
        double current = fpsFilt->currentData().toDouble();
        KdenliveSettings::setProfile_fps_filter(fpsFilt->currentText());
        m_filter->setFilterFps(current > 0,
                               current);
        slotFilterChanged();
    };
    connect(fpsFilt, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), updateFps);
    int ix = fpsFilt->findText(KdenliveSettings::profile_fps_filter());
    if (ix > -1) {
        fpsFilt->setCurrentIndex(ix);
    }
    scanningFilt->addItem("Any", -1);
    scanningFilt->addItem("Interlaced", 0);
    scanningFilt->addItem("Progressive", 1);

    auto updateScanning = [&]() {
        int current = scanningFilt->currentData().toInt();
        KdenliveSettings::setProfile_scanning_filter(scanningFilt->currentText());
        m_filter->setFilterInterlaced(current != -1,
                                      current == 0);
        slotFilterChanged();
    };
    connect(scanningFilt, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), updateScanning);

    ix = scanningFilt->findText(KdenliveSettings::profile_scanning_filter());
    if (ix > -1) {
        scanningFilt->setCurrentIndex(ix);
    }
    setLayout(lay);
}

ProfileWidget::~ProfileWidget()
{
}

void ProfileWidget::loadProfile(const QString& profile)
{
    auto index = m_treeModel->findProfile(profile);
    if (index.isValid()) {
        m_originalProfile = m_currentProfile = m_lastValidProfile = profile;
        if (!trySelectProfile(profile)) {
            // When loading a profile, ensure it is visible so reset filters if necessary
            fpsFilt->setCurrentIndex(0);
            scanningFilt->setCurrentIndex(0);
        }
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
        description += i18n("<p style='font-size:small'>Frame size: %1 x %2 (%3:%4)<br/>",profile->width(), profile->height(), profile->display_aspect_num(), profile->display_aspect_den());
        description += i18n("Frame rate: %1 fps<br/>",profile->fps());
        description += i18n("Pixel Aspect Ratio: %1<br/>",profile->sar());
        description += i18n("Color Space: %1<br/>",profile->colorspaceDescription());
        QString interlaced = i18n("yes");
        if (profile->progressive()) {
            interlaced = i18n("no");
        }
        description += i18n("Interlaced : %1</p>", interlaced);
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
    if (m_originalProfile != m_currentProfile) {
        emit profileChanged();
    }
    fillDescriptionPanel(m_currentProfile);
}

bool ProfileWidget::trySelectProfile(const QString& profile)
{
    auto index = m_treeModel->findProfile(profile);
    if (index.isValid()) {
        // check if element is visible
        if (m_filter->isVisible(index)) {
            //reselect
            QItemSelectionModel *selection = m_treeView->selectionModel();
            selection->select(m_filter->mapFromSource(index), QItemSelectionModel::Select);
            //expand corresponding category
            auto parent = m_treeModel->parent(index);
            m_treeView->expand(m_filter->mapFromSource(parent));
            m_treeView->scrollTo(m_filter->mapFromSource(index), QAbstractItemView::PositionAtCenter);
            return true;
        } else {
            return false;
        }
    } else {
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
            emit profileChanged();
            fillDescriptionPanel(QString());
        }
    }
}
