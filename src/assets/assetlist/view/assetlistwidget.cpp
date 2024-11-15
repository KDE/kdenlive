/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assetlistwidget.hpp"
#include "assets/assetlist/model/assetfilter.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "assets/assetlist/view/asseticonprovider.hpp"
#include "mltconnection.h"

#include <KNSCore/Entry>
#include <KNSWidgets/Action>
#include <QAction>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolBar>
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

AssetListWidget::AssetListWidget(bool isEffect, QWidget *parent)
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
    QAction *allEffects = new QAction(this);
    allEffects->setIcon(QIcon::fromTheme(QStringLiteral("show-all-effects")));
    allEffects->setToolTip(m_isEffect ? i18n("Main effects") : i18n("Main compositions"));
    connect(allEffects, &QAction::triggered, this, [this]() { setFilterType(QLatin1String()); });
    m_toolbar->addAction(allEffects);
    if (m_isEffect) {
        QAction *videoEffects = new QAction(this);
        videoEffects->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-show-video")));
        videoEffects->setToolTip(i18n("Show all video effects"));
        connect(videoEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("video")); });
        m_toolbar->addAction(videoEffects);
        QAction *audioEffects = new QAction(this);
        audioEffects->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
        audioEffects->setToolTip(i18n("Show all audio effects"));
        connect(audioEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("audio")); });
        m_toolbar->addAction(audioEffects);
        QAction *customEffects = new QAction(this);
        customEffects->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-custom-effect")));
        customEffects->setToolTip(i18n("Show all custom effects"));
        connect(customEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("custom")); });
        m_toolbar->addAction(customEffects);
    } else {
        QAction *transOnly = new QAction(this);
        transOnly->setIcon(QIcon::fromTheme(QStringLiteral("transform-move-horizontal")));
        transOnly->setToolTip(i18n("Show transitions only"));
        connect(transOnly, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("transition")); });
        m_toolbar->addAction(transOnly);
    }
    QAction *favEffects = new QAction(this);
    favEffects->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
    favEffects->setToolTip(i18n("Show favorite items"));
    connect(favEffects, &QAction::triggered, this, [this]() { setFilterType(QStringLiteral("favorites")); });
    m_toolbar->addAction(favEffects);
    m_lay->addWidget(m_toolbar);
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
    QWidget *empty = new QWidget(this);
    empty->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(empty);
    // Include list
    QAction *includeList = new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), QString(), this);
    includeList->setCheckable(true);
    // Disable include list on startup until ready
    KdenliveSettings::setEnableAssetsIncludeList(false);
    includeList->setChecked(KdenliveSettings::enableAssetsIncludeList());
    connect(includeList, &QAction::triggered, this, [this](bool enable) {
        KdenliveSettings::setEnableAssetsIncludeList(enable);
        m_proxyModel->updateIncludeList();
    });
    includeList->setToolTip(i18n("Only show reviewed assets"));
    m_toolbar->addAction(includeList);
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
    auto *viewSplitter = new QSplitter(Qt::Vertical, this);
    viewSplitter->addWidget(m_effectsTree);
    QTextBrowser *textEdit = new QTextBrowser(this);
    textEdit->setReadOnly(true);
    textEdit->setAcceptRichText(true);
    textEdit->setOpenLinks(false);
    connect(textEdit, &QTextBrowser::anchorClicked, pCore.get(), &Core::openDocumentationLink);
    m_infoDocument = new QTextDocument(this);
    textEdit->setDocument(m_infoDocument);
    viewSplitter->addWidget(textEdit);
    m_lay->addWidget(viewSplitter);
    viewSplitter->setStretchFactor(0, 4);
    viewSplitter->setStretchFactor(1, 2);
    viewSplitter->setSizes({50, 0});
    connect(showInfo, &QAction::triggered, this, [showInfo, viewSplitter]() {
        if (showInfo->isChecked()) {
            viewSplitter->setSizes({50, 20});
        } else {
            viewSplitter->setSizes({50, 0});
        }
    });
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
    return m_model->isFavorite(m_proxyModel->mapToSource(index), isEffect());
}

void AssetListWidget::setFavorite(const QModelIndex &index, bool favorite)
{
    m_model->setFavorite(m_proxyModel->mapToSource(index), favorite, isEffect());
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
    return m_model->getDescription(isEffect(), m_proxyModel->mapToSource(index));
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
                description.append(
                    QStringLiteral("<br/><a title=\"%1\" href=\"%2\">&#128279; %3</a>").arg(i18nc("@info:tooltip", "Online documentation"), link, id));
            } else {
                description = QStringLiteral("<a title=\"%1\" href=\"%2\">&#128279; %3</a>").arg(i18nc("@info:tooltip", "Online documentation"), link, id);
            }
        }
        m_infoDocument->setHtml(description);
    } else {
        m_infoDocument->clear();
    }
}

const QString AssetListWidget::buildLink(const QString id, AssetListType::AssetType type)
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
    return QStringLiteral("https://docs.kdenlive.org/%1/%2?mtm_campaign=inapp_asset_link&mtm_kwd=%1").arg(prefix, id, id);
}
