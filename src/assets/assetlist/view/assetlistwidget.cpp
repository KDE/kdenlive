/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assetlistwidget.hpp"
#include "assets/assetlist/model/assetfilter.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "assets/assetlist/view/asseticonprovider.hpp"
#include "mltconnection.h"

#include <KAboutData>
#include <KMessageBox>
#include <KMessageWidget>
#include <KNSCore/Entry>
#include <KNSWidgets/Action>
#include <QAction>
#include <QActionGroup>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

class AssetDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        int favorite = index.data(AssetTreeModel::FavoriteRole).toInt();
        QStyledItemDelegate::initStyleOption(option, index);
        if (favorite > 0) {
            QFont font(option->font);
            /*modify fonts*/
            font.setBold(true);
            option->font = font;
            option->fontMetrics = QFontMetrics(font);
        }
    }
};

AssetListWidget::AssetListWidget(bool isEffect, QAction *includeList, QAction *tenBit, QWidget *parent)
    : QWidget(parent)
    , m_isEffect(isEffect)
{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(0);
    m_contextMenu = new QMenu(this);
    QAction *addFavorite = new QAction(i18n("Add to favorites"), this);
    addFavorite->setData(QStringLiteral("favorite"));
    connect(addFavorite, &QAction::triggered, this, [this]() { setFavorite(m_effectsTree->currentIndex(), !isFavorite(m_effectsTree->currentIndex())); });
    m_contextMenu->addAction(addFavorite);
    if (m_isEffect) {
        // Delete effect
        QAction *customAction = new QAction(i18n("Delete custom effect"), this);
        customAction->setData(QStringLiteral("custom"));
        connect(customAction, &QAction::triggered, this, [this]() { deleteCustomEffect(m_effectsTree->currentIndex()); });
        m_contextMenu->addAction(customAction);
        // reload effect
        customAction = new QAction(i18n("Reload custom effect"), this);
        customAction->setData(QStringLiteral("custom"));
        connect(customAction, &QAction::triggered, this, [this]() { reloadCustomEffectIx(m_effectsTree->currentIndex()); });
        m_contextMenu->addAction(customAction);
        // Edit effect
        customAction = new QAction(i18n("Edit Info…"), this);
        customAction->setData(QStringLiteral("custom"));
        connect(customAction, &QAction::triggered, this, [this]() { editCustomAsset(m_effectsTree->currentIndex()); });
        m_contextMenu->addAction(customAction);
        // Edit effect
        customAction = new QAction(i18n("Export XML…"), this);
        customAction->setData(QStringLiteral("custom"));
        connect(customAction, &QAction::triggered, this, [this]() { exportCustomEffect(m_effectsTree->currentIndex()); });
        m_contextMenu->addAction(customAction);
    }
    // Create toolbar for buttons
    m_toolbar = new QToolBar(this);
    int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_toolbar->setIconSize(QSize(iconSize, iconSize));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    QActionGroup *filterGroup = new QActionGroup(this);
    QAction *allEffects = new QAction(this);
    allEffects->setIcon(QIcon::fromTheme(QStringLiteral("show-all-effects")));
    allEffects->setToolTip(m_isEffect ? i18n("Main effects") : i18n("Main compositions"));
    connect(allEffects, &QAction::triggered, this, [this]() { setFilterType(QLatin1String()); });
    allEffects->setCheckable(true);
    allEffects->setChecked(true);
    filterGroup->addAction(allEffects);
    m_toolbar->addAction(allEffects);
    if (m_isEffect) {
        QAction *videoEffects = new QAction(this);
        videoEffects->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-show-video")));
        videoEffects->setToolTip(i18n("Show all video effects"));
        connect(videoEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("video")); });
        videoEffects->setCheckable(true);
        filterGroup->addAction(videoEffects);
        m_toolbar->addAction(videoEffects);
        QAction *audioEffects = new QAction(this);
        audioEffects->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
        audioEffects->setToolTip(i18n("Show all audio effects"));
        connect(audioEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("audio")); });
        audioEffects->setCheckable(true);
        filterGroup->addAction(audioEffects);
        m_toolbar->addAction(audioEffects);
        QAction *customEffects = new QAction(this);
        customEffects->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-custom-effect")));
        customEffects->setToolTip(i18n("Show all custom effects"));
        connect(customEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("custom")); });
        customEffects->setCheckable(true);
        filterGroup->addAction(customEffects);
        m_toolbar->addAction(customEffects);
    } else {
        QAction *transOnly = new QAction(this);
        transOnly->setIcon(QIcon::fromTheme(QStringLiteral("transform-move-horizontal")));
        transOnly->setToolTip(i18n("Show transitions only"));
        connect(transOnly, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("transition")); });
        transOnly->setCheckable(true);
        filterGroup->addAction(transOnly);
        m_toolbar->addAction(transOnly);
        // Add Luma view to toolbar
        QAction *lumasAction = new QAction(QIcon::fromTheme(QStringLiteral("pixelate")), i18n("Show lumas only"), this);
        connect(lumasAction, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("lumas")); });
        lumasAction->setCheckable(true);
        filterGroup->addAction(lumasAction);
        m_toolbar->addAction(lumasAction);
    }
    QAction *favEffects = new QAction(this);
    favEffects->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
    favEffects->setToolTip(i18n("Show favorite items"));
    connect(favEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("favorites")); });
    favEffects->setCheckable(true);
    filterGroup->addAction(favEffects);
    m_toolbar->addAction(favEffects);

    // Add view mode toggle button
    QAction *toggleView = new QAction(this);
    toggleView->setIcon(QIcon::fromTheme(QStringLiteral("view-list-icons")));
    toggleView->setToolTip(i18n("Toggle between list and icon view"));
    toggleView->setCheckable(true);
    connect(toggleView, &QAction::triggered, this, &AssetListWidget::toggleViewMode);
    m_toolbar->addAction(toggleView);

    m_lay->addWidget(m_toolbar);
    QWidget *empty = new QWidget(this);
    empty->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(empty);

    // Filter button
    m_filterButton = new QToolButton(this);
    m_filterButton->setCheckable(true);
    m_filterButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_filterButton->setIcon(QIcon::fromTheme(QStringLiteral("view-filter")));
    m_filterButton->setToolTip(i18n("Filter"));
    m_filterButton->setWhatsThis(xi18nc("@info:whatsthis", "Filter the assets list. Click on the filter icon to toggle the filter display. Click on "
                                                           "the arrow icon to open a list of possible filter settings."));
    m_filterButton->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));

    // Menu
    QMenu *more = new QMenu(this);
    m_filterButton->setMenu(more);

    // Include list
    more->addAction(includeList);

    // 10 bit filter
    more->addAction(tenBit);
    if (m_isEffect) {
        m_filterButton->setChecked(KdenliveSettings::effectsFilter());
    } else {
        m_filterButton->setChecked(KdenliveSettings::transitionsFilter());
    }

    connect(includeList, &QAction::triggered, this, [this, tenBit](bool enable) {
        KdenliveSettings::setEnableAssetsIncludeList(enable);
        if (enable) {
            if (!m_filterButton->isChecked()) {
                QSignalBlocker bk(m_filterButton);
                m_filterButton->setChecked(true);
            }
        } else if (m_filterButton->isChecked() && !tenBit->isChecked()) {
            QSignalBlocker bk(m_filterButton);
            m_filterButton->setChecked(false);
        }
        m_proxyModel->updateIncludeList();
    });

    connect(tenBit, &QAction::toggled, this, [this, includeList](bool enabled) {
        KdenliveSettings::setTenbitpipeline(enabled);
        if (enabled) {
            if (!m_filterButton->isChecked()) {
                QSignalBlocker bk(m_filterButton);
                m_filterButton->setChecked(true);
            }
        } else if (m_filterButton->isChecked() && !includeList->isChecked()) {
            QSignalBlocker bk(m_filterButton);
            m_filterButton->setChecked(false);
        }
        switchTenBitFilter();
    });

    connect(m_filterButton, &QToolButton::toggled, this, [this]() {
        switchTenBitFilter();
        m_proxyModel->updateIncludeList();
    });

    if (m_isEffect) {
        KNSWidgets::Action *downloadAction = new KNSWidgets::Action(i18n("Download New Effects..."), QStringLiteral(":data/kdenlive_effects.knsrc"), this);
        m_toolbar->addAction(downloadAction);
        connect(downloadAction, &KNSWidgets::Action::dialogFinished, this, [&](const QList<KNSCore::Entry> &changedEntries) {
            if (changedEntries.count() > 0) {
                for (auto &ent : changedEntries) {
                    if (ent.status() == KNSCore::Entry::Status::Deleted) {
                        reloadTemplates();
                    } else {
                        QStringList files = ent.installedFiles();
                        for (auto &f : files) {
                            reloadCustomEffect(f);
                        }
                    }
                }
                if (!m_searchLine->text().isEmpty()) {
                    setFilterName(m_searchLine->text());
                }
            }
        });
    } else {
        KNSWidgets::Action *downloadAction = new KNSWidgets::Action(i18n("Download New Wipes..."), QStringLiteral(":data/kdenlive_wipes.knsrc"), this);
        m_toolbar->addAction(downloadAction);
        connect(downloadAction, &KNSWidgets::Action::dialogFinished, this, [&](const QList<KNSCore::Entry> &changedEntries) {
            if (changedEntries.count() > 0) {
                MltConnection::refreshLumas();
            }
        });
    }

    m_toolbar->addWidget(m_filterButton);

    // Asset Info
    QAction *showInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-about")), QString(), this);
    showInfo->setCheckable(true);
    showInfo->setToolTip(m_isEffect ? i18n("Show/hide description of the effects") : i18n("Show/hide description of the compositions"));
    m_toolbar->addAction(showInfo);
    m_lay->addWidget(m_toolbar);

    // Search line
    m_searchLine = new QLineEdit(this);
    m_searchLine->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_searchLine->setClearButtonEnabled(true);
    m_searchLine->setPlaceholderText(i18n("Search…"));
    m_searchLine->setFocusPolicy(Qt::ClickFocus);
    m_searchLine->installEventFilter(this);
    connect(m_searchLine, &QLineEdit::textChanged, this, [this](const QString &str) { setFilterName(str); });
    m_lay->addWidget(m_searchLine);

    setAcceptDrops(true);

    // Create stacked widget to hold both views
    m_effectsView = new QStackedWidget(this);

    // Tree view
    m_effectsTree = new QTreeView(this);
    m_effectsTree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_effectsTree->setHeaderHidden(true);
    m_effectsTree->setAlternatingRowColors(true);
    m_effectsTree->setRootIsDecorated(true);
    m_effectsTree->setDragEnabled(true);
    m_effectsTree->setDragDropMode(QAbstractItemView::DragDrop);
    m_effectsTree->setItemDelegate(new AssetDelegate);
    connect(m_effectsTree, &QTreeView::doubleClicked, this, &AssetListWidget::activate);
    m_effectsTree->installEventFilter(this);
    m_effectsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_effectsTree, &QTreeView::customContextMenuRequested, this, &AssetListWidget::onCustomContextMenu);

    // Icon view
    m_effectsIcon = new QListView(this);
    m_effectsIcon->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_effectsIcon->setViewMode(QListView::IconMode);
    m_effectsIcon->setResizeMode(QListView::Adjust);
    m_effectsIcon->setUniformItemSizes(true);
    m_effectsIcon->setDragEnabled(true);
    m_effectsIcon->setDragDropMode(QAbstractItemView::DragDrop);
    m_effectsIcon->setSpacing(10);
    m_effectsIcon->setWordWrap(true);
    connect(m_effectsIcon, &QListView::doubleClicked, this, &AssetListWidget::activate);
    m_effectsIcon->installEventFilter(this);
    m_effectsIcon->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_effectsIcon, &QListView::customContextMenuRequested, this, &AssetListWidget::onCustomContextMenu);

    // Add views to stacked widget
    m_effectsView->addWidget(m_effectsTree);
    m_effectsView->addWidget(m_effectsIcon);

    auto *viewSplitter = new QSplitter(Qt::Vertical, this);
    viewSplitter->addWidget(m_effectsView);
    QTextBrowser *textEdit = new QTextBrowser(this);
    textEdit->setReadOnly(true);
    textEdit->setAcceptRichText(true);
    textEdit->setOpenLinks(false);
    connect(textEdit, &QTextBrowser::anchorClicked, pCore.get(), &Core::openDocumentationLink);
    m_infoDocument = new QTextDocument(this);
    textEdit->setDocument(m_infoDocument);
    viewSplitter->addWidget(textEdit);
    viewSplitter->addWidget(textEdit);
    m_lay->addWidget(viewSplitter);
    viewSplitter->setStretchFactor(0, 4);
    viewSplitter->setStretchFactor(1, 2);
    viewSplitter->setSizes({50, 0});

    if (pCore->debugMode) {
        tenBit->setEnabled(false);
        includeList->setEnabled(false);
        KMessageWidget *mw = new KMessageWidget(this);
        mw->setMessageType(KMessageWidget::Warning);
        mw->setText(i18n("You have enabled unsupported assets"));
        mw->setCloseButtonVisible(false);
        m_lay->addWidget(mw);
    }
    connect(showInfo, &QAction::triggered, this, [showInfo, viewSplitter]() {
        if (showInfo->isChecked()) {
            viewSplitter->setSizes({50, 20});
        } else {
            viewSplitter->setSizes({50, 0});
        }
    });

    // Initialize icon provider for the list view
    m_assetIconProvider = new AssetIconProvider(m_isEffect, this);
}

AssetListWidget::~AssetListWidget() {}

bool AssetListWidget::eventFilter(QObject *watched, QEvent *event)
{
    // To avoid shortcut conflicts between the media browser and main app, we dis/enable actions when we gain/lose focus
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            setFilterName(QString());
            m_effectsTree->setFocus();
        }
        break;
    case QEvent::KeyPress: {
        const auto key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            activate(m_effectsTree->currentIndex());
            setFilterName(QString());
            event->accept();
            return true;
        } else if (watched == m_searchLine && (key == Qt::Key_Down || key == Qt::Key_Up)) {
            QModelIndex current = m_effectsTree->currentIndex();
            QModelIndex ix;
            if (key == Qt::Key_Down) {
                ix = current.siblingAtRow(current.row() + 1);
                if (!ix.isValid()) {
                    ix = current.parent().siblingAtRow(current.parent().row() + 1);
                    if (ix.isValid()) {
                        ix = m_proxyModel->index(0, 0, ix);
                    }
                }
            } else {
                if (current.row() > 0) {
                    ix = current.siblingAtRow(current.row() - 1);
                } else {
                    ix = current.parent().siblingAtRow(current.parent().row() - 1);
                    if (ix.isValid()) {
                        QModelIndex child = m_proxyModel->index(0, 0, ix);
                        int row = 1;
                        while (child.isValid()) {
                            ix = child;
                            child = child.siblingAtRow(row);
                            row++;
                        }
                    }
                }
            }
            if (ix.isValid()) {
                m_effectsTree->setCurrentIndex(ix);
            }
        }
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QString AssetListWidget::getName(const QModelIndex &index) const
{
    return m_model->getName(m_proxyModel->mapToSource(index));
}

bool AssetListWidget::isFavorite(const QModelIndex &index) const
{
    return m_model->isFavorite(m_proxyModel->mapToSource(index), m_isEffect);
}

void AssetListWidget::setFavorite(const QModelIndex &index, bool favorite)
{
    m_model->setFavorite(m_proxyModel->mapToSource(index), favorite, m_isEffect);
    Q_EMIT m_proxyModel->dataChanged(index, index, QVector<int>());
    m_proxyModel->reloadFilterOnFavorite();
    Q_EMIT reloadFavorites();
}

void AssetListWidget::deleteCustomEffect(const QModelIndex &index)
{
    m_model->deleteEffect(m_proxyModel->mapToSource(index));
}

QString AssetListWidget::getDescription(const QModelIndex &index) const
{
    return m_model->getDescription(m_isEffect, m_proxyModel->mapToSource(index));
}

void AssetListWidget::setFilterName(const QString &pattern)
{
    QItemSelectionModel *sel = m_effectsTree->selectionModel();
    QModelIndex current = m_proxyModel->getModelIndex(sel->currentIndex());
    m_proxyModel->setFilterName(!pattern.isEmpty(), pattern);
    if (!pattern.isEmpty()) {
        QVariantList mapped = m_proxyModel->getCategories();
        for (auto &ix : mapped) {
            m_effectsTree->setExpanded(ix.toModelIndex(), true);
        }
        QModelIndex ix = m_proxyModel->firstVisibleItem(m_proxyModel->mapFromSource(current));
        sel->setCurrentIndex(ix, QItemSelectionModel::ClearAndSelect);
        m_effectsTree->scrollTo(ix);
    } else {
        m_effectsTree->scrollTo(m_proxyModel->mapFromSource(current));
    }
}

QVariantMap AssetListWidget::getMimeData(const QString &assetId) const
{
    QVariantMap mimeData;
    mimeData.insert(getMimeType(assetId), assetId);
    if (isAudio(assetId)) {
        mimeData.insert(QStringLiteral("type"), QStringLiteral("audio"));
    }
    return mimeData;
}

void AssetListWidget::activate(const QModelIndex &ix)
{
    if (!ix.isValid()) {
        return;
    }
    const QString assetId = m_model->data(m_proxyModel->mapToSource(ix), AssetTreeModel::IdRole).toString();
    if (assetId != QLatin1String("root")) {
        Q_EMIT activateAsset(getMimeData(assetId));
    } else {
        m_effectsTree->setExpanded(ix, !m_effectsTree->isExpanded(ix));
    }
}

bool AssetListWidget::showDescription() const
{
    return KdenliveSettings::showeffectinfo();
}

void AssetListWidget::setShowDescription(bool show)
{
    KdenliveSettings::setShoweffectinfo(show);
    Q_EMIT showDescriptionChanged();
}

// static
bool AssetListWidget::isCustomType(AssetListType::AssetType itemType)
{
    return (itemType == AssetListType::AssetType::Custom || itemType == AssetListType::AssetType::CustomAudio ||
            itemType == AssetListType::AssetType::TemplateCustom || itemType == AssetListType::AssetType::TemplateCustomAudio);
}

// static
bool AssetListWidget::isAudioType(AssetListType::AssetType type)
{
    return (type == AssetListType::AssetType::Audio || type == AssetListType::AssetType::CustomAudio || type == AssetListType::AssetType::TemplateAudio ||
            type == AssetListType::AssetType::TemplateCustomAudio);
};

void AssetListWidget::onCustomContextMenu(const QPoint &pos)
{
    QModelIndex index = m_effectsTree->indexAt(pos);
    if (index.isValid()) {
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        const QString assetId = m_model->data(sourceIndex, AssetTreeModel::IdRole).toString();
        if (assetId != QStringLiteral("root")) {
            QList<QAction *> actions = m_contextMenu->actions();
            bool isFavorite = m_model->data(sourceIndex, AssetTreeModel::FavoriteRole).toBool();
            auto itemType = m_model->data(sourceIndex, AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
            bool isCustom = isCustomType(itemType);
            for (auto &ac : actions) {
                if (ac->data().toString() == QLatin1String("custom")) {
                    ac->setEnabled(isCustom);
                } else if (ac->data().toString() == QLatin1String("favorite")) {
                    ac->setText(isFavorite ? i18n("Remove from favorites") : i18n("Add to favorites"));
                }
            }
            m_contextMenu->exec(m_effectsTree->viewport()->mapToGlobal(pos));
        }
    }
}

void AssetListWidget::updateAssetInfo(const QModelIndex &current, const QModelIndex &)
{
    if (current.isValid()) {
        QString description = getDescription(current);
        const QString id = m_model->data(m_proxyModel->mapToSource(current), AssetTreeModel::IdRole).toString();
        if (id.isEmpty() || id == QStringLiteral("root")) {
            m_infoDocument->clear();
            return;
        }
        auto type = m_model->data(m_proxyModel->mapToSource(current), AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
        if (!isCustomType(type)) {
            // Add link to our documentation
            const QString link = buildLink(id, type);
            if (!description.isEmpty()) {
                description.append(QStringLiteral("<br/><a title=\"%1\" href=\"%2\">%3</a>").arg(i18nc("@info:tooltip", "Online documentation"), link, id));
            } else {
                description = QStringLiteral("<a title=\"%1\" href=\"%2\">%3</a>").arg(i18nc("@info:tooltip", "Online documentation"), link, id);
            }
        }
        m_infoDocument->setHtml(description);
    } else {
        m_infoDocument->clear();
    }
}

// static
const QString AssetListWidget::buildLink(const QString &id, AssetListType::AssetType type)
{
    QString prefix;
    if (type == AssetListType::AssetType::Audio) {
        prefix = QStringLiteral("effect_link");
    } else if (type == AssetListType::AssetType::Video) {
        prefix = QStringLiteral("effect_link");
    } else if (type == AssetListType::AssetType::AudioComposition || type == AssetListType::AssetType::AudioTransition) {
        prefix = QStringLiteral("transition_link");
    } else if (type == AssetListType::AssetType::VideoShortComposition || type == AssetListType::AssetType::VideoComposition ||
               type == AssetListType::AssetType::VideoTransition) {
        prefix = QStringLiteral("transition_link");
    } else {
        prefix = QStringLiteral("other");
    }
    return QStringLiteral("https://docs.kdenlive.org/%1/%2?mtm_campaign=inapp_asset_link&mtm_kwd=%3&mtm_campaign=%4")
        .arg(prefix, id, id, KAboutData::applicationData().version());
}

bool AssetListWidget::infoPanelIsFocused()
{
    return m_textEdit->hasFocus();
}

void AssetListWidget::processCopy()
{
    m_textEdit->copy();
}

void AssetListWidget::toggleViewMode()
{
    // Switch between tree view (index 0) and icon view (index 1)
    int newIndex = (m_effectsView->currentIndex() == 0) ? 1 : 0;
    m_effectsView->setCurrentIndex(newIndex);

    // Sync selection between views
    if (newIndex == 0) {
        // Switching to tree view
        if (m_effectsIcon->selectionModel()->hasSelection()) {
            QModelIndex iconIndex = m_effectsIcon->selectionModel()->currentIndex();
            m_effectsTree->selectionModel()->setCurrentIndex(iconIndex, QItemSelectionModel::ClearAndSelect);
        }
    } else {
        // Switching to icon view
        if (m_effectsTree->selectionModel()->hasSelection()) {
            QModelIndex treeIndex = m_effectsTree->selectionModel()->currentIndex();
            m_effectsIcon->selectionModel()->setCurrentIndex(treeIndex, QItemSelectionModel::ClearAndSelect);
        }
    }
}

bool AssetListWidget::isIconView() const
{
    return m_effectsView->currentIndex() == 1;
}
