/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "guideslist.h"
#include "bin/bin.h"
#include "bin/model/markersortmodel.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "jobs/cachetask.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"
#include "utils/thumbnailcache.hpp"

#include <KLocalizedString>
#include <KMessageBox>

#include <QButtonGroup>
#include <QCheckBox>
#include <QConcatenateTablesProxyModel>
#include <QDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>

GuideFilterEventEater::GuideFilterEventEater(QObject *parent)
    : QObject(parent)
{
}

bool GuideFilterEventEater::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            Q_EMIT clearSearchLine();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

GuidesProxyModel::GuidesProxyModel(int normalHeight, QObject *parent)
    : QIdentityProxyModel(parent)
{
    m_showThumbs = KdenliveSettings::guidesShowThumbs();
    m_height = normalHeight;
    refreshDar();
}

void GuidesProxyModel::refreshDar()
{
    m_width = m_height * 3 * pCore->getCurrentDar();
}

QVariant GuidesProxyModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DecorationRole && m_showThumbs) {
        int frames = QIdentityProxyModel::data(index, MarkerListModel::FrameRole).toInt();
        const QString binId = QIdentityProxyModel::data(index, MarkerListModel::clipIdRole).toString();
        int type = QIdentityProxyModel::data(index, MarkerListModel::TypeRole).toInt();
        const QColor markerColor = pCore->markerTypes.value(type).color;
        QImage thumb = ThumbnailCache::get()->getThumbnail(binId, frames);
        QPixmap pix(m_width, 3 * m_height);
        pix.fill(markerColor);
        QPainter p(&pix);
        int margin = 3;
        p.drawImage(QRect(margin, margin, pix.width() - 2 * margin, pix.height() - 2 * margin), thumb);
        p.end();
        return QIcon(pix);
    }
    if (role == Qt::SizeHintRole) {
        if (m_showThumbs) {
            return QSize(50, 3 * m_height + 2);
        }
        return QSize(50, m_height * 1.5 + 2);
    }
    if (role == Qt::DisplayRole) {
        int frames = timecodeOffset + QIdentityProxyModel::data(index, MarkerListModel::FrameRole).toInt();
        return QStringLiteral("%1 %2").arg(pCore->timecode().getDisplayTimecodeFromFrames(frames, false), QIdentityProxyModel::data(index, role).toString());
    }
    return sourceModel()->data(mapToSource(index), role);
}

GuidesList::GuidesList(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    int fontHeight = QFontMetrics(font()).lineSpacing();
    m_proxy = new GuidesProxyModel(fontHeight, this);
    connect(guides_list, &QListView::doubleClicked, this, &GuidesList::editGuide);
    connect(guide_delete, &QToolButton::clicked, this, &GuidesList::removeGuide);
    connect(guide_add, &QToolButton::clicked, this, &GuidesList::addGuide);
    connect(guide_edit, &QToolButton::clicked, this, &GuidesList::editGuides);
    connect(filter_line, &QLineEdit::textChanged, this, &GuidesList::filterView);
    connect(pCore.get(), &Core::updateDefaultMarkerCategory, this, &GuidesList::refreshDefaultCategory);
    connect(show_all, &QToolButton::toggled, this, &GuidesList::showAllMarkers);
    QAction *a = KStandardAction::renameFile(this, &GuidesList::editGuides, this);
    guides_list->addAction(a);
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fixedFont.setPointSize(font().pointSize());
    guides_list->setFont(fixedFont);
    slotShowThumbs(KdenliveSettings::guidesShowThumbs());
    a->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    //  Settings menu
    QMenu *settingsMenu = new QMenu(this);
    QAction *showThumbs = new QAction(QIcon::fromTheme(QStringLiteral("view-preview")), i18n("Show Thumbnails"), this);
    showThumbs->setCheckable(true);
    showThumbs->setChecked(KdenliveSettings::guidesShowThumbs());
    connect(showThumbs, &QAction::toggled, this, &GuidesList::slotShowThumbs);
    settingsMenu->addAction(showThumbs);

    m_importGuides = new QAction(QIcon::fromTheme(QStringLiteral("document-import")), i18n("Import…"), this);
    connect(m_importGuides, &QAction::triggered, this, &GuidesList::importGuides);
    settingsMenu->addAction(m_importGuides);
    m_exportGuides = new QAction(QIcon::fromTheme(QStringLiteral("document-export")), i18n("Export…"), this);
    connect(m_exportGuides, &QAction::triggered, this, &GuidesList::saveGuides);
    settingsMenu->addAction(m_exportGuides);
    QAction *categories = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure Categories"), this);
    connect(categories, &QAction::triggered, this, &GuidesList::configureGuides);
    settingsMenu->addAction(categories);
    guides_settings->setMenu(settingsMenu);

    show_all->setToolTip(i18n("Show markers for all clips in the project"));

    QAction *findAction = KStandardAction::find(filter_line, SLOT(setFocus()), this);
    addAction(findAction);
    findAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    // Sort menu
    m_sortGroup = new QActionGroup(this);
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
    m_sortGroup->addAction(sort1);
    m_sortGroup->addAction(sort2);
    m_sortGroup->addAction(sort3);
    sortMenu->addAction(sort1);
    sortMenu->addAction(sort2);
    sortMenu->addAction(sort3);
    sortMenu->addSeparator();
    sortMenu->addAction(sortDescending);
    sort2->setChecked(true);
    sort_guides->setMenu(sortMenu);
    connect(m_sortGroup, &QActionGroup::triggered, this, &GuidesList::sortView);
    connect(sortDescending, &QAction::triggered, this, &GuidesList::changeSortOrder);
    connect(pCore->bin(), &Bin::updateTabName, this, &GuidesList::renameTimeline);

    // Filtering
    show_categories->enableFilterMode();
    show_categories->setAllowAll(true);
    show_categories->setOnlyUsed(true);
    connect(show_categories, &QToolButton::toggled, this, &GuidesList::switchFilter);
    connect(show_categories, &MarkerCategoryButton::categoriesChanged, this, &GuidesList::updateFilter);

    auto *leventEater = new GuideFilterEventEater(this);
    filter_line->installEventFilter(leventEater);
    connect(leventEater, &GuideFilterEventEater::clearSearchLine, filter_line, &QLineEdit::clear);
    connect(filter_line, &QLineEdit::returnPressed, filter_line, &QLineEdit::clear);

    thumbs_refresh->setToolTip(i18n("Refresh All Thumbnails"));
    connect(thumbs_refresh, &QToolButton::clicked, this, &GuidesList::rebuildThumbs);

    guide_add->setToolTip(i18n("Add new guide."));
    guide_add->setWhatsThis(xi18nc("@info:whatsthis", "Add new guide. This will add a guide at the current frame position."));
    guide_delete->setToolTip(i18n("Delete guide."));
    guide_delete->setWhatsThis(xi18nc("@info:whatsthis", "Delete guide. This will erase all selected guides."));
    guide_edit->setToolTip(i18n("Edit selected guide."));
    guide_edit->setWhatsThis(xi18nc("@info:whatsthis", "Edit selected guide. Selecting multiple guides allows changing their category."));
    show_categories->setToolTip(i18n("Filter guide categories."));
    show_categories->setWhatsThis(
        xi18nc("@info:whatsthis", "Filter guide categories. This allows you to show or hide selected guide categories in this dialog and in the timeline."));
    default_category->setToolTip(i18n("Default guide category."));
    default_category->setWhatsThis(xi18nc("@info:whatsthis", "Default guide category. The category used for newly created guides."));
    connect(pCore.get(), &Core::profileChanged, m_proxy, &GuidesProxyModel::refreshDar);
    m_markerRefreshTimer.setSingleShot(true);
    m_markerRefreshTimer.setInterval(500);
    connect(&m_markerRefreshTimer, &QTimer::timeout, this, &GuidesList::fetchMovedThumbs);
}

void GuidesList::refreshDar()
{
    m_proxy->refreshDar();
}

void GuidesList::showAllMarkers(bool enable)
{
    if (enable) {
        if (m_displayMode == ClipMarkers) {
            // Disconnect
            if (auto markerModel = m_model.lock()) {
                disconnect(markerModel.get(), &MarkerListModel::categoriesChanged, this, &GuidesList::rebuildCategories);
            }
        }
        m_displayMode = AllMarkers;
        connect(pCore->projectItemModel().get(), &ProjectItemModel::projectClipsModified, this, &GuidesList::rebuildAllMarkers, Qt::UniqueConnection);
        m_model.reset();
        m_containerProxy = new QConcatenateTablesProxyModel(this);
        auto list = pCore->bin()->getAllClipsMarkers();
        for (auto &m : list) {
            m_containerProxy->addSourceModel(m.get());
        }
        m_markerFilterModel.reset(new MarkerSortModel(this));
        m_markerFilterModel->setSourceModel(m_containerProxy);
        m_markerFilterModel->setSortRole(MarkerListModel::PosRole);
        m_markerFilterModel->sort(0, Qt::AscendingOrder);
        m_proxy->setSourceModel(m_markerFilterModel.get());
        guides_list->setModel(m_proxy);
        guideslist_label->setText(i18n("All Project Markers"));
        guides_list->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(guides_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GuidesList::selectionChanged, Qt::UniqueConnection);
        rebuildCategories();
        show_categories->setOnlyUsed(false);
        switchFilter(!m_lastSelectedMarkerCategories.isEmpty() && !m_lastSelectedMarkerCategories.contains(-1));
        buildMissingThumbs();
    } else {
        disconnect(pCore->projectItemModel().get(), &ProjectItemModel::projectClipsModified, this, &GuidesList::rebuildAllMarkers);
        m_markerFilterModel.reset();
        delete m_containerProxy;
        if (pCore->monitorManager()->isActive(Kdenlive::ClipMonitor)) {
            m_displayMode = ClipMarkers;
            std::shared_ptr<ProjectClip> clip = pCore->bin()->getFirstSelectedClip();
            setClipMarkerModel(clip);
        } else {
            m_displayMode = TimelineMarkers;
            m_sortModel = nullptr;
            auto project = pCore->currentDoc();
            const QUuid uuid = project->activeUuid;
            setModel(project->getGuideModel(uuid), project->getFilteredGuideModel(uuid));
        }
    }
    m_importGuides->setEnabled(!enable);
    m_exportGuides->setEnabled(!enable);
}

void GuidesList::clear()
{
    if (m_displayMode == AllMarkers) {
        // Disable all markers mode
        show_all->toggle();
    }
    setClipMarkerModel(nullptr);
    m_lastSelectedGuideCategories.clear();
    m_lastSelectedMarkerCategories.clear();
    show_categories->setCurrentCategories({-1});
    filter_line->clear();
}

void GuidesList::rebuildAllMarkers()
{
    QList<QAbstractItemModel *> previous = m_containerProxy->sourceModels();
    auto list = pCore->bin()->getAllClipsMarkers();
    for (auto &p : previous) {
        m_containerProxy->removeSourceModel(p);
    }

    for (auto &m : list) {
        m_containerProxy->addSourceModel(m.get());
    }
    if (show_categories->isChecked()) {
        switchFilter(true);
    }
    if (!filter_line->text().isEmpty()) {
        filterView(filter_line->text());
    }
    buildMissingThumbs();
}

void GuidesList::setTimecodeOffset(int offset)
{
    guides_list->hide();
    m_proxy->timecodeOffset = offset;
    guides_list->show();
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
    if (selectedIndexes.size() == 1 || m_displayMode == AllMarkers) {
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
    if (m_displayMode == AllMarkers) {
        QModelIndex ix2 = m_proxy->mapToSource(ix);
        QModelIndex ix3 = m_markerFilterModel->mapToSource(ix2);
        QModelIndex ix4 = m_containerProxy->mapToSource(ix3);
        auto sourceModel = ix4.model();
        if (sourceModel) {
            const MarkerListModel *markerModel = static_cast<const MarkerListModel *>(sourceModel);
            if (markerModel) {
                auto clip = pCore->projectItemModel()->getClipByBinID(markerModel->ownerId());
                if (clip) {
                    clip->getMarkerModel()->editMarkerGui(pos, qApp->activeWindow(), false, clip.get());
                }
            }
        }
    } else if (auto markerModel = m_model.lock()) {
        ProjectClip *clip = nullptr;
        const QString binId = markerModel->ownerId();
        if (!binId.isEmpty()) {
            clip = pCore->projectItemModel()->getClipByBinID(binId).get();
        }
        markerModel->editMarkerGui(pos, qApp->activeWindow(), false, clip);
    }
}

void GuidesList::selectAll()
{
    guides_list->selectAll();
}

void GuidesList::removeGuide()
{
    QModelIndexList selection = guides_list->selectionModel()->selectedIndexes();
    if (selection.isEmpty()) {
        return;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (m_displayMode == AllMarkers) {
        for (auto &ix : selection) {
            QModelIndex ix2 = m_proxy->mapToSource(ix);
            QModelIndex ix3 = m_markerFilterModel->mapToSource(ix2);
            QModelIndex ix4 = m_containerProxy->mapToSource(ix3);
            auto sourceModel = ix4.model();
            if (sourceModel) {
                int frame = m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
                GenTime pos(frame, pCore->getCurrentFps());
                const MarkerListModel *markerModel = static_cast<const MarkerListModel *>(sourceModel);
                if (markerModel) {
                    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(markerModel->ownerId());
                    if (clip) {
                        clip->getMarkerModel()->removeMarker(pos, undo, redo);
                    }
                }
            }
            pCore->pushUndo(undo, redo, i18n("Remove guides"));
        }
    } else if (auto markerModel = m_model.lock()) {
        QList<int> frames;
        for (auto &ix : selection) {
            frames << m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
        }
        for (auto &frame : frames) {
            GenTime pos(frame, pCore->getCurrentFps());
            markerModel->removeMarker(pos, undo, redo);
        }
        pCore->pushUndo(undo, redo, i18n("Remove guides"));
    }
}

void GuidesList::addGuide()
{
    if (m_displayMode == AllMarkers) {
        pCore->triggerAction(QStringLiteral("add_clip_marker"));
        return;
    }
    int frame = pCore->getMonitorPosition(m_displayMode == TimelineMarkers ? Kdenlive::ProjectMonitor : Kdenlive::ClipMonitor);
    if (frame >= 0) {
        GenTime pos(frame, pCore->getCurrentFps());
        if (auto markerModel = m_model.lock()) {
            ProjectClip *clip = nullptr;
            const QString binId = markerModel->ownerId();
            if (!binId.isEmpty()) {
                clip = pCore->projectItemModel()->getClipByBinID(binId).get();
            }
            markerModel->addMultipleMarkersGui(pos, this, true, clip);
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
    if (m_displayMode == AllMarkers) {
        QModelIndex ix2 = m_proxy->mapToSource(ix);
        QModelIndex ix3 = m_markerFilterModel->mapToSource(ix2);
        QModelIndex ix4 = m_containerProxy->mapToSource(ix3);
        auto sourceModel = ix4.model();
        if (sourceModel) {
            const MarkerListModel *markerModel = static_cast<const MarkerListModel *>(sourceModel);
            if (markerModel) {
                qDebug() << "//// MATCH FOR MODEL: " << markerModel->ownerId();
                pCore->bin()->selectClipById(markerModel->ownerId(), pos, QPoint(), true);
            }
        }
    } else {
        pCore->seekMonitor(m_displayMode == TimelineMarkers ? Kdenlive::ProjectMonitor : Kdenlive::ClipMonitor, pos);
    }
}

GuidesList::~GuidesList() = default;

void GuidesList::setClipMarkerModel(std::shared_ptr<ProjectClip> clip)
{
    if (m_displayMode == AllMarkers) {
        // We are in all markers mode, don't switch view
        return;
    }
    m_displayMode = ClipMarkers;
    guides_lock->setVisible(false);
    thumbs_refresh->setVisible(false);
    if (m_displayMode == ClipMarkers) {
        if (auto markerModel = m_model.lock()) {
            if (clip && markerModel->ownerId() == clip->clipId()) {
                // Clip is already displayed
                return;
            }
            // Disconnect
            disconnect(markerModel.get(), &MarkerListModel::categoriesChanged, this, &GuidesList::rebuildCategories);
        }
    }
    if (clip == nullptr) {
        m_sortModel = nullptr;
        m_proxy->setSourceModel(m_sortModel);
        guides_list->setModel(m_proxy);
        guideslist_label->clear();
        setEnabled(false);
        return;
    }
    setEnabled(true);
    guideslist_label->setText(clip->clipName());
    m_sortModel = clip->getFilteredMarkerModel().get();
    m_model = clip->getMarkerModel();
    m_proxy->setSourceModel(m_sortModel);
    guides_list->setModel(m_proxy);
    guides_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(guides_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GuidesList::selectionChanged, Qt::UniqueConnection);
    rebuildCategories();
    if (auto markerModel = m_model.lock()) {
        show_categories->setMarkerModel(markerModel.get());
        show_categories->setCurrentCategories(m_lastSelectedMarkerCategories);
        switchFilter(!m_lastSelectedMarkerCategories.isEmpty() && !m_lastSelectedMarkerCategories.contains(-1));
        connect(markerModel.get(), &MarkerListModel::categoriesChanged, this, &GuidesList::rebuildCategories);
    }
    buildMissingThumbs();
}

void GuidesList::setModel(std::weak_ptr<MarkerListModel> model, std::shared_ptr<MarkerSortModel> viewModel)
{
    if (m_displayMode == AllMarkers) {
        // We are in all markers mode, don't switch view
        return;
    }
    if (m_displayMode == ClipMarkers) {
        // Disconnect
        if (auto markerModel = m_model.lock()) {
            disconnect(markerModel.get(), &MarkerListModel::categoriesChanged, this, &GuidesList::rebuildCategories);
        }
    }
    m_displayMode = TimelineMarkers;
    if (viewModel.get() == m_sortModel) {
        // already displayed
        return;
    }
    m_model = std::move(model);
    m_sortModel = viewModel.get();
    setEnabled(true);

    if (!guides_lock->defaultAction()) {
        QAction *action = pCore->window()->actionCollection()->action("lock_guides");
        guides_lock->setDefaultAction(action);
    }
    guides_lock->setVisible(true);
    thumbs_refresh->setVisible(true);
    m_proxy->setSourceModel(m_sortModel);
    guides_list->setModel(m_proxy);
    guides_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(guides_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GuidesList::selectionChanged, Qt::UniqueConnection);
    if (auto markerModel = m_model.lock()) {
        const QString binId = markerModel->ownerId();
        auto clip = pCore->projectItemModel()->getClipByBinID(binId);
        if (clip) {
            guideslist_label->setText(clip->clipName());
            m_uuid = clip->getSequenceUuid();
        }
        show_categories->setMarkerModel(markerModel.get());
        show_categories->setCurrentCategories(m_lastSelectedGuideCategories);
        switchFilter(!m_lastSelectedGuideCategories.isEmpty() && !m_lastSelectedGuideCategories.contains(-1));
        connect(markerModel.get(), &MarkerListModel::categoriesChanged, this, &GuidesList::rebuildCategories);
        connect(markerModel.get(), &MarkerListModel::dataChanged, this, &GuidesList::checkGuideChange, Qt::UniqueConnection);
    } else {
        m_sortModel = nullptr;
    }
    rebuildCategories();
}

void GuidesList::rebuildCategories()
{
    if (pCore->markerTypes.isEmpty()) {
        // Categories are not ready, abort
        return;
    }
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
        QAction *ac = new QAction(colorIcon, i.value().displayName);
        ac->setData(i.key());
        markerDefaultMenu->addAction(ac);
        if (i.key() == KdenliveSettings::default_marker_type()) {
            default_category->setIcon(colorIcon);
            defaultCategoryFound = true;
        }
    }
    if (!defaultCategoryFound) {
        // Default marker category not found. set it to first one
        QAction *ac = markerDefaultMenu->actions().constFirst();
        if (ac) {
            default_category->setIcon(ac->icon());
            KdenliveSettings::setDefault_marker_type(ac->data().toInt());
        }
    }
}

void GuidesList::refreshDefaultCategory()
{
    int ix = KdenliveSettings::default_marker_type();
    QMenu *menu = default_category->menu();
    if (menu) {
        const QList<QAction *> actions = menu->actions();
        for (const auto *ac : actions) {
            if (ac->data() == ix) {
                default_category->setIcon(ac->icon());
                break;
            }
        }
    }
}

void GuidesList::switchFilter(bool enable)
{
    QList<int> cats = m_displayMode == TimelineMarkers ? m_lastSelectedGuideCategories : m_lastSelectedMarkerCategories;
    if (enable && !cats.contains(-1)) {
        updateFilter(cats);
        show_categories->setChecked(true);
    } else {
        updateFilter({});
        show_categories->setChecked(false);
    }
}

void GuidesList::updateFilter(QList<int> categories)
{
    switch (m_displayMode) {
    case AllMarkers:
        m_markerFilterModel->slotSetFilters(categories);
        m_lastSelectedMarkerCategories = categories;
        break;
    case ClipMarkers: {
        if (auto markerModel = m_model.lock()) {
            const QString binId = markerModel->ownerId();
            if (!binId.isEmpty()) {
                auto clip = pCore->projectItemModel()->getClipByBinID(binId);
                clip->getFilteredMarkerModel()->slotSetFilters(categories);
                m_lastSelectedMarkerCategories = categories;
            }
        }
        break;
    }
    case TimelineMarkers:
    default:
        if (m_sortModel) {
            m_sortModel->slotSetFilters(categories);
            m_lastSelectedGuideCategories = categories;
            Q_EMIT pCore->refreshActiveGuides();
        }
    }
}

void GuidesList::filterView(const QString &text)
{
    if (m_markerFilterModel) {
        m_markerFilterModel->slotSetFilterString(text);
        if (!text.isEmpty() && guides_list->model()->rowCount() > 0) {
            guides_list->setCurrentIndex(guides_list->model()->index(0, 0));
        }
        QModelIndex current = guides_list->currentIndex();
        if (current.isValid()) {
            guides_list->scrollTo(current);
        }
    } else if (m_sortModel) {
        m_sortModel->slotSetFilterString(text);
        if (!text.isEmpty() && guides_list->model()->rowCount() > 0) {
            guides_list->setCurrentIndex(guides_list->model()->index(0, 0));
        }
        QModelIndex current = guides_list->currentIndex();
        if (current.isValid()) {
            guides_list->scrollTo(current);
        }
    }
}

void GuidesList::sortView(QAction *ac)
{
    if (m_markerFilterModel) {
        m_markerFilterModel->slotSetSortColumn(ac->data().toInt());
    } else if (m_sortModel) {
        m_sortModel->slotSetSortColumn(ac->data().toInt());
    }
}

void GuidesList::changeSortOrder(bool descending)
{
    if (m_markerFilterModel) {
        m_markerFilterModel->slotSetSortOrder(descending);
    } else if (m_sortModel) {
        m_sortModel->slotSetSortOrder(descending);
    }
}

void GuidesList::renameTimeline(const QUuid &uuid, const QString &name)
{
    if (m_displayMode != TimelineMarkers || m_uuid != uuid) {
        return;
    }
    guideslist_label->setText(name);
}

void GuidesList::slotShowThumbs(bool show)
{
    m_proxy->m_showThumbs = show;
    KdenliveSettings::setGuidesShowThumbs(show);
    int fontHeight = QFontMetrics(font()).lineSpacing();
    if (show) {
        fontHeight *= 3;
        guides_list->setIconSize(QSize(fontHeight * pCore->getCurrentDar(), fontHeight));
    } else {
        fontHeight -= 2;
        guides_list->setIconSize(QSize(fontHeight, fontHeight));
    }
    // Check for missing Thumbnails
    buildMissingThumbs();
}

void GuidesList::buildMissingThumbs()
{
    if (!KdenliveSettings::guidesShowThumbs()) {
        return;
    };
    if (m_displayMode == ClipMarkers) {
        if (auto markerModel = m_model.lock()) {
            std::vector<int> positions = markerModel->getSnapPoints();
            const QString binId = markerModel->ownerId();
            std::set<int> missingFrames;
            for (auto &p : positions) {
                if (!ThumbnailCache::get()->hasThumbnail(binId, p)) {
                    missingFrames.insert(p);
                }
            }
            if (missingFrames.size() > 0) {
                // Generate missing thumbs
                CacheTask::start(ObjectId(KdenliveObjectType::BinClip, binId.toInt(), QUuid()), missingFrames, 0, 0, 0, this);
            }
            Q_EMIT markerModel->dataChanged(QModelIndex(), QModelIndex(), {Qt::SizeHintRole});
        }
    } else if (m_displayMode == AllMarkers) {
        auto list = pCore->bin()->getAllClipsMarkers();
        for (auto &markerModel : list) {
            std::vector<int> positions = markerModel->getSnapPoints();
            const QString binId = markerModel->ownerId();
            std::set<int> missingFrames;
            for (auto &p : positions) {
                if (!ThumbnailCache::get()->hasThumbnail(binId, p)) {
                    missingFrames.insert(p);
                }
            }
            if (missingFrames.size() > 0) {
                // Generate missing thumbs
                CacheTask::start(ObjectId(KdenliveObjectType::BinClip, binId.toInt(), QUuid()), missingFrames, 0, 0, 0, this);
            }
        }
        Q_EMIT m_proxy->dataChanged(QModelIndex(), QModelIndex(), {Qt::SizeHintRole});
    }
}

void GuidesList::updateJobProgress()
{
    if (m_displayMode == AllMarkers) {
        QList<QAbstractItemModel *> previous = m_containerProxy->sourceModels();
        for (auto &p : previous) {
            Q_EMIT p->dataChanged(p->index(0, 0), p->index(p->rowCount() - 1, 0), {Qt::DecorationRole});
        }
    } else {
        if (auto markerModel = m_model.lock()) {
            Q_EMIT markerModel->dataChanged(markerModel->index(0), markerModel->index(markerModel->rowCount() - 1), {Qt::DecorationRole});
        }
    }
}

void GuidesList::checkGuideChange(const QModelIndex &start, const QModelIndex &, const QList<int> &roles)
{
    if (!KdenliveSettings::guidesShowThumbs()) {
        return;
    }
    if (roles.contains(MarkerListModel::FrameRole)) {
        if (!m_indexesToRefresh.contains(start)) {
            m_indexesToRefresh << start;
        }
        m_markerRefreshTimer.start();
    }
}

void GuidesList::fetchMovedThumbs()
{
    if (auto markerModel = m_model.lock()) {
        std::set<int> missingFrames;
        while (!m_indexesToRefresh.isEmpty()) {
            const QModelIndex ix = m_indexesToRefresh.takeFirst();
            if (ix.isValid()) {
                missingFrames.insert(markerModel->data(ix, MarkerListModel::FrameRole).toInt());
            }
        }
        const QString binId = markerModel->ownerId();
        CacheTask::start(ObjectId(KdenliveObjectType::BinClip, binId.toInt(), QUuid()), missingFrames, 0, 0, 0, this);
    }
}

void GuidesList::rebuildThumbs()
{
    if (auto markerModel = m_model.lock()) {
        std::vector<int> framesToClear = markerModel->getSnapPoints();
        std::set<int> frames(framesToClear.begin(), framesToClear.end());
        const QString binId = markerModel->ownerId();
        ThumbnailCache::get()->invalidateThumbsForClip(binId, frames);
        CacheTask::start(ObjectId(KdenliveObjectType::BinClip, binId.toInt(), QUuid()), frames, 0, 0, 0, this);
    }
}
