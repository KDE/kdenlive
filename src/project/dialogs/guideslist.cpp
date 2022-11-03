/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "guideslist.h"
#include "bin/bin.h"
#include "bin/model/markersortmodel.h"
#include "bin/projectclip.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "project/projectmanager.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QMenu>
#include <QPainter>

class GuidesProxyModel : public QIdentityProxyModel
{
public:
    explicit GuidesProxyModel(QObject *parent = nullptr)
        : QIdentityProxyModel(parent)
    {
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role == Qt::DisplayRole) {
            return QString("%1 %2").arg(QIdentityProxyModel::data(index, MarkerListModel::TCRole).toString(),
                                        QIdentityProxyModel::data(index, role).toString());
        }
        return sourceModel()->data(mapToSource(index), role);
    }
};

GuidesList::GuidesList(QWidget *parent)
    : QWidget(parent)
    , m_markerMode(false)
{
    setupUi(this);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    categoryContent->setLayout(&m_categoriesLayout);
    category_frame->setVisible(false);
    connect(show_categories, &QToolButton::toggled, category_frame, &QFrame::setVisible);
    m_proxy = new GuidesProxyModel(this);
    connect(guides_list, &QListView::doubleClicked, this, &GuidesList::editGuide);
    connect(guide_delete, &QToolButton::clicked, this, &GuidesList::removeGuide);
    connect(guide_add, &QToolButton::clicked, this, &GuidesList::addGuide);
    connect(guide_edit, &QToolButton::clicked, this, &GuidesList::editGuides);
    connect(filter_line, &QLineEdit::textChanged, this, &GuidesList::filterView);
    connect(pCore.get(), &Core::updateDefaultMarkerCategory, this, &GuidesList::refreshDefaultCategory);

    //  Settings menu
    QMenu *settingsMenu = new QMenu(this);
    QAction *importGuides = new QAction(QIcon::fromTheme(QStringLiteral("document-import")), i18n("Import..."), this);
    connect(importGuides, &QAction::triggered, this, &GuidesList::importGuides);
    settingsMenu->addAction(importGuides);
    QAction *exportGuides = new QAction(QIcon::fromTheme(QStringLiteral("document-export")), i18n("Export..."), this);
    connect(exportGuides, &QAction::triggered, this, &GuidesList::saveGuides);
    settingsMenu->addAction(exportGuides);
    QAction *categories = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure Categories"), this);
    connect(categories, &QAction::triggered, this, &GuidesList::configureGuides);
    settingsMenu->addAction(categories);
    guides_settings->setMenu(settingsMenu);

    // Sort menu
    m_filterGroup = new QActionGroup(this);
    QMenu *sortMenu = new QMenu(this);
    QAction *sort1 = new QAction(i18n("Sort by Category"), this);
    sort1->setCheckable(true);
    QAction *sort2 = new QAction(i18n("Sort by Time"), this);
    sort2->setCheckable(true);
    QAction *sort3 = new QAction(i18n("Sort by Comment"), this);
    sort3->setCheckable(true);
    QAction *sortDescending = new QAction(i18n("Descending"), this);
    sortDescending->setCheckable(true);

    sort1->setData(0);
    sort2->setData(1);
    sort3->setData(2);
    m_filterGroup->addAction(sort1);
    m_filterGroup->addAction(sort2);
    m_filterGroup->addAction(sort3);
    sortMenu->addAction(sort1);
    sortMenu->addAction(sort2);
    sortMenu->addAction(sort3);
    sortMenu->addSeparator();
    sortMenu->addAction(sortDescending);
    sort2->setChecked(true);
    sort_guides->setMenu(sortMenu);
    connect(m_filterGroup, &QActionGroup::triggered, this, &GuidesList::sortView);
    connect(sortDescending, &QAction::triggered, this, &GuidesList::changeSortOrder);

    guide_add->setToolTip(i18n("Add new guide."));
    guide_add->setWhatsThis(xi18nc("@info:whatsthis", "Add new guide. This will add a guide at the current frame position."));
    guide_delete->setToolTip(i18n("Delete guide."));
    guide_delete->setWhatsThis(xi18nc("@info:whatsi18nthis", "Delete guide. This will erase all selected guides."));
    guide_edit->setToolTip(i18n("Edit selected guide."));
    guide_edit->setWhatsThis(xi18nc("@info:whatsthis", "Edit selected guide. Selecting multiple guides allows changing their category."));
    show_categories->setToolTip(i18n("Filter guide categories."));
    show_categories->setWhatsThis(
        xi18nc("@info:whatsthis", "Filter guide categories. This allows you to show or hide selected guide categories in this dialog and in the timeline."));
    default_category->setToolTip(i18n("Default guide category."));
    default_category->setWhatsThis(xi18nc("@info:whatsthis", "Default guide category. The category used for newly created markers."));
}

void GuidesList::configureGuides()
{
    pCore->window()->slotEditProjectSettings(2);
}

void GuidesList::importGuides()
{
    if (auto markerModel = m_model.lock()) {
        QScopedPointer<QFileDialog> fd(
            new QFileDialog(this, i18nc("@title:window", "Load Clip Markers"), pCore->projectManager()->current()->projectDataFolder()));
        fd->setMimeTypeFilters({QStringLiteral("application/json"), QStringLiteral("text/plain")});
        fd->setFileMode(QFileDialog::ExistingFile);
        if (fd->exec() != QDialog::Accepted) {
            return;
        }
        QStringList selection = fd->selectedFiles();
        QString url;
        if (!selection.isEmpty()) {
            url = selection.first();
        }
        if (url.isEmpty()) {
            return;
        }
        QFile file(url);
        if (file.size() > 1048576 &&
            KMessageBox::warningContinueCancel(this, i18n("Marker file is larger than 1MB, are you sure you want to import ?")) != KMessageBox::Continue) {
            // If marker file is larger than 1MB, ask for confirmation
            return;
        }
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            KMessageBox::error(this, i18n("Cannot read file %1", QUrl::fromLocalFile(url).fileName()));
            return;
        }
        QString fileContent = QString::fromUtf8(file.readAll());
        file.close();
        markerModel->importFromFile(fileContent, true);
    }
}

void GuidesList::saveGuides()
{
    if (auto markerModel = m_model.lock()) {
        markerModel->exportGuidesGui(this, GenTime());
    }
}

void GuidesList::editGuides()
{
    QModelIndexList selectedIndexes = guides_list->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        return;
    }
    if (selectedIndexes.size() == 1) {
        editGuide(selectedIndexes.first());
        return;
    }
    QList<GenTime> timeList;
    for (auto &ix : selectedIndexes) {
        int frame = m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
        GenTime pos(frame, pCore->getCurrentFps());
        timeList << pos;
    }
    std::sort(timeList.begin(), timeList.end());
    if (auto markerModel = m_model.lock()) {
        markerModel->editMultipleMarkersGui(timeList, qApp->activeWindow());
    }
}

void GuidesList::editGuide(const QModelIndex &ix)
{
    if (!ix.isValid()) return;
    int frame = m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
    GenTime pos(frame, pCore->getCurrentFps());
    if (auto markerModel = m_model.lock()) {
        markerModel->editMarkerGui(pos, qApp->activeWindow(), false, m_clip.get());
    }
}

void GuidesList::removeGuide()
{
    QModelIndexList selection = guides_list->selectionModel()->selectedIndexes();
    if (auto markerModel = m_model.lock()) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        for (auto &ix : selection) {
            int frame = m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
            GenTime pos(frame, pCore->getCurrentFps());
            markerModel->removeMarker(pos, undo, redo);
        }
        if (!selection.isEmpty()) {
            pCore->pushUndo(undo, redo, i18n("Remove guides"));
        }
    }
}

void GuidesList::addGuide()
{
    int frame = pCore->getMonitorPosition(m_markerMode ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor);
    if (frame >= 0) {
        GenTime pos(frame, pCore->getCurrentFps());
        if (auto markerModel = m_model.lock()) {
            markerModel->addMultipleMarkersGui(pos, this, true, m_clip.get());
        }
    }
}

void GuidesList::selectionChanged(const QItemSelection &selected, const QItemSelection &)
{
    if (selected.indexes().isEmpty()) {
        return;
    }
    const QModelIndex ix = selected.indexes().first();
    if (!ix.isValid()) {
        return;
    }
    int pos = m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
    pCore->seekMonitor(m_markerMode ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor, pos);
}

GuidesList::~GuidesList() = default;

void GuidesList::setClipMarkerModel(std::shared_ptr<ProjectClip> clip)
{
    m_markerMode = true;
    if (clip == m_clip) {
        return;
    }
    m_clip = clip;
    if (clip == nullptr) {
        m_sortModel = nullptr;
        m_proxy->setSourceModel(m_sortModel);
        guides_list->setModel(m_proxy);
        guideslist_label->clear();
        setEnabled(false);
        return;
    }
    setEnabled(true);
    guideslist_label->setText(i18n("Markers for %1", clip->clipName()));
    m_sortModel = clip->getFilteredMarkerModel().get();
    m_model = clip->getMarkerModel();
    m_proxy->setSourceModel(m_sortModel);
    guides_list->setModel(m_proxy);
    guides_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(guides_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GuidesList::selectionChanged);
    if (auto markerModel = m_model.lock()) {
        connect(markerModel.get(), &MarkerListModel::categoriesChanged, this, &GuidesList::rebuildCategories);
    }
    rebuildCategories();
}

void GuidesList::setModel(std::weak_ptr<MarkerListModel> model, std::shared_ptr<MarkerSortModel> viewModel)
{
    m_clip = nullptr;
    m_markerMode = false;
    if (viewModel.get() == m_sortModel) {
        // already displayed
        return;
    }
    m_model = std::move(model);
    setEnabled(true);
    guideslist_label->setText(i18n("Timeline Guides"));
    if (auto markerModel = m_model.lock()) {
        m_sortModel = viewModel.get();
        m_proxy->setSourceModel(m_sortModel);
        guides_list->setModel(m_proxy);
        guides_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        connect(guides_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GuidesList::selectionChanged);
        connect(markerModel.get(), &MarkerListModel::categoriesChanged, this, &GuidesList::rebuildCategories);
    }
    rebuildCategories();
}

void GuidesList::rebuildCategories()
{
    // Clean up categories
    QLayoutItem *item;
    while ((item = m_categoriesLayout.takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    delete catGroup;
    catGroup = new QButtonGroup(this);
    catGroup->setExclusive(false);
    QPixmap pixmap(32, 32);
    // Cleanup default marker category menu
    QMenu *markerDefaultMenu = default_category->menu();
    if (markerDefaultMenu) {
        markerDefaultMenu->clear();
    } else {
        markerDefaultMenu = new QMenu(this);
        connect(markerDefaultMenu, &QMenu::triggered, this, [this](QAction *ac) {
            int val = ac->data().toInt();
            KdenliveSettings::setDefault_marker_type(val);
            default_category->setIcon(ac->icon());
        });
        default_category->setMenu(markerDefaultMenu);
    }

    QMapIterator<int, Core::MarkerCategory> i(pCore->markerTypes);
    bool defaultCategoryFound = false;
    while (i.hasNext()) {
        i.next();
        pixmap.fill(i.value().color);
        QIcon colorIcon(pixmap);
        QCheckBox *cb = new QCheckBox(i.value().displayName, this);
        cb->setProperty("index", i.key());
        cb->setIcon(colorIcon);
        QAction *ac = new QAction(colorIcon, i.value().displayName);
        ac->setData(i.key());
        markerDefaultMenu->addAction(ac);
        if (i.key() == KdenliveSettings::default_marker_type()) {
            default_category->setIcon(colorIcon);
            defaultCategoryFound = true;
        }
        catGroup->addButton(cb);
        m_categoriesLayout.addWidget(cb);
    }
    if (!defaultCategoryFound) {
        // Default marker category not found. set it to first one
        QAction *ac = markerDefaultMenu->actions().first();
        if (ac) {
            default_category->setIcon(ac->icon());
            KdenliveSettings::setDefault_marker_type(ac->data().toInt());
        }
    }
    connect(catGroup, &QButtonGroup::buttonToggled, this, &GuidesList::updateFilter);
}

void GuidesList::refreshDefaultCategory()
{
    int ix = KdenliveSettings::default_marker_type();
    QMenu *menu = default_category->menu();
    if (menu) {
        QList<QAction *> actions = menu->actions();
        for (auto *ac : actions) {
            if (ac->data() == ix) {
                default_category->setIcon(ac->icon());
                break;
            }
        }
    }
}

void GuidesList::updateFilter(QAbstractButton *, bool)
{
    QList<int> filters;
    QList<QAbstractButton *> buttons = catGroup->buttons();
    for (auto &b : buttons) {
        if (b->isChecked()) {
            filters << b->property("index").toInt();
        }
    }
    pCore->currentDoc()->setGuidesFilter(filters);
    emit pCore->refreshActiveGuides();
}

void GuidesList::filterView(const QString &text)
{
    if (m_sortModel) {
        m_sortModel->slotSetFilterString(text);
    }
}

void GuidesList::sortView(QAction *ac)
{
    if (m_sortModel) {
        m_sortModel->slotSetSortColumn(ac->data().toInt());
    }
}

void GuidesList::changeSortOrder(bool descending)
{
    if (m_sortModel) {
        m_sortModel->slotSetSortOrder(descending);
    }
}
