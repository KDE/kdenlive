/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "guideslist.h"
#include "bin/bin.h"
#include "bin/model/markersortmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "mainwindow.h"

#include <KLocalizedString>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
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
    connect(guide_save, &QToolButton::clicked, this, &GuidesList::saveGuides);
    connect(configure, &QToolButton::clicked, this, &GuidesList::configureGuides);
    connect(filter_line, &QLineEdit::textChanged, this, &GuidesList::filterView);
    QMenu *menu = new QMenu(this);
    QAction *ac = new QAction(i18n("add multiple guides"), this);
    connect(ac, &QAction::triggered, this, &GuidesList::addMutipleGuides);
    menu->addAction(ac);
    guide_add->setMenu(menu);

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
    guide_delete->setToolTip(i18n("Dereturn leftTime < rightTime;lete guide."));
    guide_delete->setWhatsThis(xi18nc("@info:whatsi18nthis", "Delete guide. This will erase all selected guides."));
    guide_save->setToolTip(i18n("Export guides."));
    guide_save->setWhatsThis(
        xi18nc("@info:whatsthis", "Export guide. This allows you to copy the guides data in a text format for use in web platforms for example."));
    configure->setToolTip(i18n("Configure project guide categories."));
    configure->setWhatsThis(xi18nc(
        "@info:whatsthis", "Configure guide categories. This allows you to customize the guide categories used in this project (add, remove, rename, ...)."));
    show_categories->setToolTip(i18n("Filter guide categories."));
    show_categories->setWhatsThis(
        xi18nc("@info:whatsthis", "Filter guide categories. This allows you to show or hide selected guide categories in this dialog and in the timeline."));
}

void GuidesList::configureGuides()
{
    pCore->window()->slotEditProjectSettings(2);
}

void GuidesList::saveGuides()
{
    if (auto markerModel = m_model.lock()) {
        markerModel->exportGuidesGui(this, GenTime());
    }
}

void GuidesList::editGuide(const QModelIndex &ix)
{
    if (!ix.isValid()) return;
    int frame = m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
    GenTime pos(frame, pCore->getCurrentFps());
    if (auto markerModel = m_model.lock()) {
        markerModel->editMarkerGui(pos, qApp->activeWindow(), false);
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
    int frame = pCore->getTimelinePosition();
    if (frame >= 0) {
        GenTime pos(frame, pCore->getCurrentFps());
        if (auto markerModel = m_model.lock()) {
            markerModel->addMarker(pos, i18n("guide"));
        }
    }
}

void GuidesList::addMutipleGuides()
{
    int frame = pCore->getTimelinePosition();
    if (frame >= 0) {
        GenTime pos(frame, pCore->getCurrentFps());
        if (auto markerModel = m_model.lock()) {
            markerModel->addMultipleMarkersGui(pos, qApp->activeWindow(), true);
        }
    }
}

void GuidesList::selectionChanged(const QItemSelection &selected, const QItemSelection &)
{
    if (selected.isEmpty()) {
        return;
    }
    const QModelIndex ix = selected.indexes().first();
    if (!ix.isValid()) {
        return;
    }
    int pos = m_proxy->data(ix, MarkerListModel::FrameRole).toInt();
    pCore->seekMonitor(Kdenlive::ProjectMonitor, pos);
}

GuidesList::~GuidesList() = default;

void GuidesList::setModel(std::weak_ptr<MarkerListModel> model, MarkerSortModel *viewModel)
{
    m_model = std::move(model);
    if (auto markerModel = m_model.lock()) {
        m_sortModel = viewModel;
        m_proxy->setSourceModel(viewModel);
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
    QMapIterator<int, Core::MarkerCategory> i(pCore->markerTypes);
    while (i.hasNext()) {
        i.next();
        pixmap.fill(i.value().color);
        QIcon colorIcon(pixmap);
        QCheckBox *cb = new QCheckBox(i.value().displayName, this);
        cb->setProperty("index", i.key());
        cb->setIcon(colorIcon);
        catGroup->addButton(cb);
        m_categoriesLayout.addWidget(cb);
    }
    connect(catGroup, &QButtonGroup::buttonToggled, this, &GuidesList::updateFilter);
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
    if (auto markerModel = m_model.lock()) {
        pCore->currentDoc()->setGuidesFilter(filters);
    }
    emit pCore->refreshActiveGuides();
    qDebug() << "::: GOT FILTERS: " << filters;
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
