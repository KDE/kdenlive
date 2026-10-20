/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2017 Nicolas Carion
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "profilewidget.h"
#include "dialogs/profilesdialog.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "profiles/tree/profilefilter.hpp"
#include "profiles/tree/profiletreemodel.hpp"

#include <KLocalizedString>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSplitter>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

ProfilePreviewWidget::ProfilePreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ProfilePreviewWidget::setDimensions(int width, int height, int aspectNum, int aspectDen)
{
    m_width = width;
    m_height = height;
    m_aspectNum = aspectNum;
    m_aspectDen = aspectDen;
    m_valid = (m_width > 0 && m_height > 0);
    update();
}

void ProfilePreviewWidget::clear()
{
    m_width = 0;
    m_height = 0;
    m_aspectNum = 0;
    m_aspectDen = 0;
    m_valid = false;
    update();
}

QSize ProfilePreviewWidget::sizeHint() const
{
    return QSize(160, 64);
}

QSize ProfilePreviewWidget::minimumSizeHint() const
{
    return QSize(120, 50);
}

void ProfilePreviewWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF totalRect = rect();
    if (totalRect.width() <= 0 || totalRect.height() <= 0) {
        return;
    }

    if (!m_valid) {
        painter.setPen(QPen(palette().color(QPalette::Disabled, QPalette::Text), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        QRectF emptyBox = totalRect.adjusted(8, 8, -8, -8);
        painter.drawRoundedRect(emptyBox, 4, 4);
        painter.drawText(emptyBox, Qt::AlignCenter, i18n("No profile selected"));
        return;
    }

    double aspect = 16.0 / 9.0;
    if (m_aspectNum > 0 && m_aspectDen > 0) {
        aspect = double(m_aspectNum) / double(m_aspectDen);
    } else if (m_height > 0) {
        aspect = double(m_width) / double(m_height);
    }

    QString ratioStr;
    if (m_aspectNum > 0 && m_aspectDen > 0) {
        ratioStr = QStringLiteral("%1:%2").arg(m_aspectNum).arg(m_aspectDen);
    } else {
        ratioStr = QString::number(aspect, 'f', 2) + QStringLiteral(":1");
    }

    QRectF previewArea = totalRect.adjusted(8, 4, -8, -4);

    if (previewArea.width() <= 0 || previewArea.height() <= 0) {
        return;
    }

    double maxW = previewArea.width();
    double maxH = previewArea.height();
    double boxAspect = maxW / maxH;
    double shapeW = 0;
    double shapeH = 0;

    if (aspect >= boxAspect) {
        shapeW = maxW;
        shapeH = maxW / aspect;
    } else {
        shapeH = maxH;
        shapeW = maxH * aspect;
    }

    QRectF shapeRect(previewArea.center().x() - shapeW / 2.0, previewArea.center().y() - shapeH / 2.0, shapeW, shapeH);

    // Shape rectangle with theme highlight color
    QColor highlightColor = palette().highlight().color();
    QColor fillColor = highlightColor;
    fillColor.setAlpha(60);

    QPen strokePen(highlightColor, 2);
    painter.setPen(strokePen);
    painter.setBrush(fillColor);
    painter.drawRoundedRect(shapeRect, 4, 4);

    // Aspect ratio label inside shape if space allows
    if (shapeRect.width() >= 26 && shapeRect.height() >= 18) {
        QFont ratioFont = font();
        ratioFont.setBold(true);
        if (shapeRect.height() < 30 || shapeRect.width() < 50) {
            ratioFont.setPointSize(qMax(7, ratioFont.pointSize() - 2));
        }
        painter.setFont(ratioFont);
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(shapeRect, Qt::AlignCenter, ratioStr);
    }
}

ProfileWidget::ProfileWidget(QWidget *parent)
    : QWidget(parent)
    , m_originalProfile(QStringLiteral("invalid"))
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    auto *lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    auto *labelLay = new QHBoxLayout;

    // Search bar
    QLineEdit *searchBar = new QLineEdit(this);
    searchBar->setPlaceholderText(i18n("Search…"));
    searchBar->setClearButtonEnabled(true);
    labelLay->addWidget(searchBar);

    auto *fpsLabel = new QLabel(i18n("Fps:"), this);
    m_fpsFilt = new QComboBox(this);
    fpsLabel->setBuddy(m_fpsFilt);
    labelLay->addWidget(fpsLabel);
    labelLay->addWidget(m_fpsFilt);

    auto *scanningLabel = new QLabel(i18nc("@label Scanning as the video method: interlaced or progressive", "Scanning:"), this);
    m_scanningFilt = new QComboBox(this);
    scanningLabel->setBuddy(m_scanningFilt);
    labelLay->addWidget(scanningLabel);
    labelLay->addWidget(m_scanningFilt);
    labelLay->addStretch(1);

    auto *manage_profiles = new QToolButton(this);
    labelLay->addWidget(manage_profiles);
    manage_profiles->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    manage_profiles->setToolTip(i18n("Manage project profiles"));
    manage_profiles->setWhatsThis(
        xi18nc("@info:whatsthis",
               "Opens the profile dialog window in which you can change project profiles. Note: The profile used in the open project cannot be changed."));
    connect(manage_profiles, &QAbstractButton::clicked, this, &ProfileWidget::slotEditProfiles);
    lay->addLayout(labelLay);

    auto *profileSplitter = new QSplitter;

    m_treeView = new QTreeView(this);
    m_treeModel = ProfileTreeModel::construct(this);
    m_filter = new ProfileFilter(this);
    m_filter->setSourceModel(m_treeModel.get());
    m_treeView->setModel(m_filter);

    connect(searchBar, &QLineEdit::textChanged, this, [this](const QString &str) {
        m_filter->slotSetSearchString(str);
        if (str.isEmpty()) {
            // focus last selected item when clearing search line
            trySelectProfile(m_currentProfile);
        } else {
            m_treeView->expandAll();
        }
    });

    for (int i = 1; i < m_treeModel->columnCount(); ++i) {
        m_treeView->hideColumn(i);
    }
    m_treeView->header()->hide();
    QItemSelectionModel *selectionModel = m_treeView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentRowChanged, this, &ProfileWidget::slotChangeSelection);
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, [&](const QItemSelection &selected, const QItemSelection &deselected) {
        QModelIndex current;
        QModelIndex old;
        if (!selected.indexes().isEmpty()) {
            current = selected.indexes().front();
        }
        if (!deselected.indexes().isEmpty()) {
            old = deselected.indexes().front();
        }
        slotChangeSelection(current, old);
    });
    int treeViewFontHeight = QFontInfo(m_treeView->font()).pixelSize();
    m_treeView->setMinimumHeight(treeViewFontHeight);
    profileSplitter->addWidget(m_treeView);
    m_treeView->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

    auto *detailsWidget = new QWidget(this);
    auto *detailsLayout = new QVBoxLayout(detailsWidget);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(4);

    m_previewWidget = new ProfilePreviewWidget(this);
    detailsLayout->addWidget(m_previewWidget);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    detailsLayout->addWidget(m_previewLabel);

    m_descriptionPanel = new QTextEdit(this);
    m_descriptionPanel->setReadOnly(true);
    m_descriptionPanel->viewport()->setCursor(Qt::ArrowCursor);
    m_descriptionPanel->viewport()->setBackgroundRole(QPalette::Mid);
    m_descriptionPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_descriptionPanel->setFrameStyle(QFrame::NoFrame);
    m_descriptionPanel->setMinimumHeight(treeViewFontHeight);
    detailsLayout->addWidget(m_descriptionPanel, 1);

    profileSplitter->addWidget(detailsWidget);

    lay->addWidget(profileSplitter);
    profileSplitter->setStretchFactor(0, 4);
    profileSplitter->setStretchFactor(1, 3);

    refreshFpsCombo();
    auto updateFps = [&]() {
        double current = m_fpsFilt->currentData().toDouble();
        KdenliveSettings::setProfile_fps_filter(m_fpsFilt->currentText());
        m_filter->setFilterFps(current > 0, current);
        slotFilterChanged();
    };
    connect(m_fpsFilt, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), updateFps);
    int ix = m_fpsFilt->findText(KdenliveSettings::profile_fps_filter());
    if (ix > -1) {
        m_fpsFilt->setCurrentIndex(ix);
    }
    m_scanningFilt->addItem(i18n("Any"), -1);
    m_scanningFilt->addItem(i18n("Interlaced"), 0);
    m_scanningFilt->addItem(i18n("Progressive"), 1);

    auto updateScanning = [&]() {
        int current = m_scanningFilt->currentData().toInt();
        KdenliveSettings::setProfile_scanning_filter(m_scanningFilt->currentText());
        m_filter->setFilterInterlaced(current != -1, current == 0);
        slotFilterChanged();
    };
    connect(m_scanningFilt, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), updateScanning);

    ix = m_scanningFilt->findText(KdenliveSettings::profile_scanning_filter());
    if (ix > -1) {
        m_scanningFilt->setCurrentIndex(ix);
    }
    setLayout(lay);
}

ProfileWidget::~ProfileWidget() = default;

void ProfileWidget::refreshFpsCombo()
{
    QLocale locale; // Used for UI → OK
    QVariant currentValue;
    if (m_fpsFilt->count() > 1) {
        // remember last selected value
        currentValue = m_fpsFilt->currentData();
    }
    m_fpsFilt->clear();
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    m_fpsFilt->addItem(i18n("Any"), -1);
    auto all_fps = ProfileRepository::get()->getAllFps();
    for (double fps : std::as_const(all_fps)) {
        m_fpsFilt->addItem(locale.toString(fps), fps);
    }
    if (currentValue.isValid()) {
        int ix = m_fpsFilt->findData(currentValue);
        if (ix > -1) {
            m_fpsFilt->setCurrentIndex(ix);
        }
    }
}

void ProfileWidget::loadProfile(const QString &profile)
{
    auto index = m_treeModel->findProfile(profile);
    if (index.isValid()) {
        m_originalProfile = m_currentProfile = m_lastValidProfile = profile;
        if (!trySelectProfile(profile)) {
            // When loading a profile, ensure it is visible so reset filters if necessary
            m_fpsFilt->setCurrentIndex(0);
            m_scanningFilt->setCurrentIndex(0);
        }
    }
}

const QString ProfileWidget::selectedProfile() const
{
    return m_currentProfile;
}

void ProfileWidget::slotEditProfiles()
{
    auto *w = new ProfilesDialog(ProfileRepository::get()->getProfile(m_currentProfile)->description());
    w->exec();
    if (w->profileTreeChanged()) {
        // Rebuild profiles tree
        m_treeModel.reset();
        m_treeModel = ProfileTreeModel::construct(this);
        m_filter->setSourceModel(m_treeModel.get());
        refreshFpsCombo();
        loadProfile(m_currentProfile);
    }
    delete w;
}

void ProfileWidget::fillDescriptionPanel(const QString &profile_path)
{
    QString description;
    if (profile_path.isEmpty()) {
        if (m_previewWidget) {
            m_previewWidget->clear();
        }
        if (m_previewLabel) {
            m_previewLabel->clear();
        }
        description += i18n("No profile selected");
    } else {
        std::unique_ptr<ProfileModel> &profile = ProfileRepository::get()->getProfile(profile_path);
        if (m_previewWidget) {
            m_previewWidget->setDimensions(profile->width(), profile->height(), profile->display_aspect_num(), profile->display_aspect_den());
        }
        if (m_previewLabel) {
            double aspect = 16.0 / 9.0;
            if (profile->display_aspect_num() > 0 && profile->display_aspect_den() > 0) {
                aspect = double(profile->display_aspect_num()) / double(profile->display_aspect_den());
            } else if (profile->height() > 0) {
                aspect = double(profile->width()) / double(profile->height());
            }
            QString orientationStr = aspect > 1.05 ? i18n("Landscape") : (aspect < 0.95 ? i18n("Portrait") : i18n("Square"));
            m_previewLabel->setText(QStringLiteral("%1 • %2 x %3").arg(orientationStr).arg(profile->width()).arg(profile->height()));
        }

        description += QStringLiteral("<h5>%1</h5>").arg(i18n("Video Settings"));
        description +=
            QStringLiteral("<p style='font-size:small'>%1<br/>")
                .arg(i18n("Frame size: %1 x %2 (%3:%4)", profile->width(), profile->height(), profile->display_aspect_num(), profile->display_aspect_den()));
        description += i18n("Frame rate: %1 fps", profile->fps());
        description += QStringLiteral("<br/>");
        description += i18n("Pixel aspect ratio: %1", profile->sar());
        description += QStringLiteral("<br/>");
        description += i18n("Color space: %1", ProfileRepository::getColorspaceDescription(profile->colorspace()));
        description += QStringLiteral("<br/>");
        description += i18n("Interlaced: %1", profile->progressive() ? i18n("no") : i18n("yes"));
        if (!profile->progressive()) {
            description += QStringLiteral("<br/>");
            description += i18n("Field order: %1", profile->bottom_field_first() ? i18n("Bottom field first") : i18n("Top field first"));
        }
        description += QStringLiteral("</p>");
    }
    m_descriptionPanel->setHtml(description);
}

void ProfileWidget::slotChangeSelection(const QModelIndex &current, const QModelIndex &previous)
{
    auto originalIndex = m_filter->mapToSource(current);
    if (m_treeModel->parent(originalIndex) == QModelIndex()) {
        // in that case, we have selected a category, which we don't want
        QItemSelectionModel *selection = m_treeView->selectionModel();
        selection->select(previous, QItemSelectionModel::Select);
        return;
    }
    m_currentProfile = m_treeModel->getProfile(originalIndex);
    if (!m_currentProfile.isEmpty()) {
        m_lastValidProfile = m_currentProfile;
    }
    if (m_originalProfile != m_currentProfile) {
        Q_EMIT profileChanged();
    }
    fillDescriptionPanel(m_currentProfile);
}

bool ProfileWidget::trySelectProfile(const QString &profile)
{
    auto index = m_treeModel->findProfile(profile);
    if (index.isValid()) {
        // check if element is visible
        if (m_filter->isVisible(index)) {
            // reselect
            QItemSelectionModel *selection = m_treeView->selectionModel();
            selection->select(m_filter->mapFromSource(index), QItemSelectionModel::Select);
            // expand corresponding category
            auto parent = m_treeModel->parent(index);
            m_treeView->expand(m_filter->mapFromSource(parent));
            m_treeView->scrollTo(m_filter->mapFromSource(index), QAbstractItemView::PositionAtCenter);
            return true;
        }
    }
    return false;
}

void ProfileWidget::slotFilterChanged()
{
    // When filtering change, we must check if the current profile is still visible.
    if (!trySelectProfile(m_currentProfile)) {
        // we try to back-up the last valid profile
        if (!trySelectProfile(m_lastValidProfile)) {
            // Everything fails, we don't have any profile
            m_currentProfile = QString();
            Q_EMIT profileChanged();
            fillDescriptionPanel(QString());
        }
    }
}
