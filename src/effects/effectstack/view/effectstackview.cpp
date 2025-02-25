/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectstackview.hpp"
#include "assets/assetlist/view/asseticonprovider.hpp"
#include "assets/assetpanel.hpp"
#include "assets/view/assetparameterview.hpp"
#include "builtstack.hpp"
#include "collapsibleeffectview.hpp"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackfilter.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "timeline2/model/timelinemodel.hpp"
#include "utils/qstringutils.h"

#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <KMessageBox>
#include <utility>

int dragRow = -1;

WidgetDelegate::WidgetDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize WidgetDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    if (m_height.contains(index)) {
        s.setHeight(height(index));
    }
    return s;
}

void WidgetDelegate::setHeight(const QModelIndex &index, int height)
{
    QWriteLocker lock(&m_lock);
    m_height[index] = height;
    lock.unlock();
    Q_EMIT sizeHintChanged(index);
}

int WidgetDelegate::height(const QModelIndex &index) const
{
    QReadLocker lock(&m_lock);
    return m_height.value(index);
}

void WidgetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
}

EffectStackView::EffectStackView(AssetPanel *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_thumbnailer(new AssetIconProvider(true, this))
{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(0);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    setAcceptDrops(true);

    m_builtStack = new QWidget(this);
    m_lay->addWidget(m_builtStack);
    m_builtStack->setVisible(false);

    m_effectsTree = new QTreeView(this);
    m_effectsTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    m_effectsTree->setHeaderHidden(true);
    m_effectsTree->setRootIsDecorated(false);
    m_effectsTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QString style = QStringLiteral("QTreeView {border: none;}");
    m_effectsTree->viewport()->setAutoFillBackground(false);
    m_effectsTree->setStyleSheet(style);
    m_effectsTree->setItemDelegateForColumn(0, new WidgetDelegate(this));
    m_lay->addWidget(m_effectsTree);
    m_lay->addStretch(10);

    m_scrollTimer.setSingleShot(true);
    m_scrollTimer.setInterval(250);
    connect(&m_scrollTimer, &QTimer::timeout, this, &EffectStackView::checkScrollBar);

    m_timerHeight.setSingleShot(true);
    m_timerHeight.setInterval(50);
}

EffectStackView::~EffectStackView()
{
    delete m_thumbnailer;
}

void EffectStackView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effect"))) {
        dragPos = event->position().toPoint();
        if (event->source() == this) {
            event->setDropAction(Qt::MoveAction);
        } else {
            event->setDropAction(Qt::CopyAction);
        }
        event->setAccepted(true);
    } else {
        event->setAccepted(false);
    }
}

void EffectStackView::dragLeaveEvent(QDragLeaveEvent *event)
{
    dragPos = QPoint();
    event->accept();
    if (dragRow > -1 && dragRow < m_model->rowCount()) {
        auto item = m_model->getEffectStackRow(dragRow);
        if (item->childCount() > 0) {
            // TODO: group
        } else {
            std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
            setDropTargetEffect(m_model->getIndexFromItem(eff), false);
        }
    }
    dragRow = -1;
}

void EffectStackView::dragMoveEvent(QDragMoveEvent *event)
{
    int prevTarget = dragRow;
    dragRow = m_model->rowCount();
    for (int i = 0; i < m_model->rowCount(); i++) {
        auto item = m_model->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // TODO: group
            continue;
        }
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
        QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(eff));
        QWidget *w = m_effectsTree->indexWidget(ix);
        QPoint mousePos;
        mousePos = event->position().toPoint();
        if (w && w->geometry().contains(mousePos)) {
            dragRow = i;
            if (i == m_model->rowCount() - 1) {
                // Special case for last item in the stack, if we are lower than 2/3, drop after
                if (mousePos.y() > w->geometry().y() + (2 * w->geometry().height()) / 3) {
                    dragRow = m_model->rowCount();
                }
            }
            break;
        }
    }
    if (dragRow > -1 && event->source() == this) {
        const QString sourceData = event->mimeData()->data(QStringLiteral("kdenlive/effectsource"));
        int oldRow = sourceData.section(QLatin1Char(','), 2, 2).toInt();
        if (dragRow == oldRow || dragRow == oldRow + 1) {
            // Don't drag on itself
            dragRow = -1;
        }
    }
    dragPos = event->position().toPoint();

    if (prevTarget != dragRow) {
        if (prevTarget > -1 && prevTarget < m_model->rowCount()) {
            auto item = m_model->getEffectStackRow(prevTarget);
            if (item->childCount() > 0) {
                // TODO: group
            } else {
                std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
                setDropTargetEffect(m_model->getIndexFromItem(eff), false);
            }
        }
        if (dragRow > -1 && dragRow < m_model->rowCount()) {
            auto item = m_model->getEffectStackRow(dragRow);
            if (item->childCount() > 0) {
                // TODO: group
            } else {
                std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
                setDropTargetEffect(m_model->getIndexFromItem(eff), true);
            }
        }
    }
    update();
    Q_EMIT checkDragScrolling();
}

void EffectStackView::dropEvent(QDropEvent *event)
{
    dragPos = QPoint();
    if (dragRow < 0) {
        return;
    }
    QString effectId = event->mimeData()->data(QStringLiteral("kdenlive/effect"));
    if (event->source() == this) {
        const QString sourceData = event->mimeData()->data(QStringLiteral("kdenlive/effectsource"));
        int oldRow = sourceData.section(QLatin1Char(','), 2, 2).toInt();
        if (dragRow == oldRow || dragRow > m_model->rowCount()) {
            return;
        }
        QMetaObject::invokeMethod(m_model.get(), "moveEffectByRow", Qt::QueuedConnection, Q_ARG(int, dragRow), Q_ARG(int, oldRow));
    } else {
        bool added = false;
        if (dragRow < m_model->rowCount()) {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            bool result = m_model->appendEffectWithUndo(effectId, undo, redo).first;
            if (result) {
                added = true;
                m_model->moveEffectWithUndo(dragRow, m_model->getEffectStackRow(m_model->rowCount() - 1), undo, redo);
            }
            if (result) {
                pCore->pushUndo(undo, redo, i18n("Add effect %1", EffectsRepository::get()->getName(effectId)));
            }
        } else {
            if (m_model->appendEffect(effectId) && m_model->rowCount() > 0) {
                added = true;
                m_model->setActiveEffect(m_model->rowCount() - 1);
            }
        }
        if (added) {
            m_scrollTimer.start();
        }
    }
    if (dragRow > -1 && dragRow < m_model->rowCount()) {
        auto item = m_model->getEffectStackRow(dragRow);
        if (item->childCount() > 0) {
            // TODO: group
        } else {
            std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
            setDropTargetEffect(m_model->getIndexFromItem(eff), false);
        }
    }
    dragRow = -1;
    event->acceptProposedAction();
}

void EffectStackView::paintEvent(QPaintEvent *event)
{
    if (dragRow == m_model->rowCount()) {
        QWidget::paintEvent(event);
        QPainter p(this);
        QPen pen(palette().highlight().color());
        pen.setWidth(4);
        p.setPen(pen);
        p.drawLine(0, m_effectsTree->height(), width(), m_effectsTree->height());
    }
}

void EffectStackView::setModel(std::shared_ptr<EffectStackModel> model, const QSize frameSize)
{
    m_mutex.lock();
    QItemSelectionModel *m = m_effectsTree->selectionModel();
    PlaylistState::ClipState currentState = PlaylistState::Unknown;
    if (m_model) {
        currentState = pCore->getItemState(m_model->getOwnerId());
    }
    unsetModel(false);
    m_effectsTree->setModel(nullptr);
    m_model.reset();
    if (m) {
        delete m;
    }
    PlaylistState::ClipState newState = PlaylistState::Unknown;
    if (model) {
        newState = pCore->getItemState(model->getOwnerId());
    }
    m_effectsTree->setFixedHeight(0);
    m_model = std::move(model);
    if (KdenliveSettings::enableBuiltInEffects() && currentState != newState) {
        // Rebuilt the builtin effect stack
        QLayout *lay = m_builtStack->layout();
        if (lay) {
            while (QLayoutItem *item = lay->takeAt(0)) {
                delete item;
            }
            delete m_flipLabel;
            delete m_flipH;
            delete m_flipV;
            delete m_removeBg;
            delete m_samProgressBar;
            delete m_samAbortButton;
            m_flipLabel = nullptr;
            m_flipH = nullptr;
            m_flipV = nullptr;
            m_removeBg = nullptr;
            m_samProgressBar = nullptr;
            m_samAbortButton = nullptr;
            delete lay;
        }
        if (newState != PlaylistState::AudioOnly) {
            // Add Flip widget
            QFormLayout *layout = new QFormLayout(m_builtStack);
            m_flipH = new QPushButton(this);
            m_flipH->setToolTip(i18n("Horizontal Flip"));
            m_flipH->setIcon(QIcon::fromTheme("object-flip-horizontal"));
            m_flipH->setCheckable(true);
            m_flipH->setFlat(true);
            m_flipV = new QPushButton(this);
            m_flipV->setToolTip(i18n("Vertical Flip"));
            m_flipV->setIcon(QIcon::fromTheme("object-flip-vertical"));
            m_flipV->setCheckable(true);
            m_flipV->setFlat(true);
            QHBoxLayout *lay = new QHBoxLayout;
            lay->addWidget(m_flipH);
            lay->addWidget(m_flipV);
            lay->addStretch(10);
            m_removeBg = new QPushButton(i18n("Remove Background"), this);
            m_removeBg->setToolTip(i18n("Remove background using AI model"));
            m_samProgressBar = new QProgressBar(this);
            m_samProgressBar->setVisible(false);
            m_samAbortButton = new QToolButton(this);
            m_samAbortButton->setAutoRaise(true);
            m_samAbortButton->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
            m_samAbortButton->setVisible(false);
            lay->addWidget(m_removeBg);
            lay->addWidget(m_samAbortButton);
            lay->addWidget(m_samProgressBar);
            m_flipLabel = new QLabel(i18n("Flip"));
            layout->addRow(m_flipLabel, lay);
            m_builtStack->setVisible(true);
            connect(m_flipH, &QPushButton::clicked, this, [this](bool checked) {
                if (checked) {
                    QMap<QString, QString> params;
                    params.insert(QStringLiteral("kdenlive:builtin"), QStringLiteral("1"));
                    params.insert(QStringLiteral("kdenlive:hiddenbuiltin"), QStringLiteral("1"));
                    m_model->appendEffect(QStringLiteral("avfilter.hflip"), false, params);
                } else {
                    auto item = m_model->getAssetModelById("avfilter.hflip");
                    if (item) {
                        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(item);
                        m_model->removeEffect(sourceEffect);
                    }
                }
            });
            connect(m_flipV, &QPushButton::clicked, this, [this](bool checked) {
                if (checked) {
                    QMap<QString, QString> params;
                    params.insert(QStringLiteral("kdenlive:builtin"), QStringLiteral("1"));
                    params.insert(QStringLiteral("kdenlive:hiddenbuiltin"), QStringLiteral("1"));
                    m_model->appendEffect(QStringLiteral("avfilter.vflip"), false, params);
                } else {
                    auto item = m_model->getAssetModelById("avfilter.vflip");
                    if (item) {
                        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(item);
                        m_model->removeEffect(sourceEffect);
                    }
                }
            });
            connect(m_removeBg, &QPushButton::clicked, this, &EffectStackView::launchSam);
            connect(m_samAbortButton, &QToolButton::clicked, this, &EffectStackView::abortSam);
        } else {
            m_builtStack->setVisible(false);
        }
    }
    if (m_flipH) {
        // Adjust flip state
        if (m_model->hasBuiltInEffect(QStringLiteral("avfilter.hflip"))) {
            m_flipH->setChecked(true);
        }
        if (m_model->hasBuiltInEffect(QStringLiteral("avfilter.vflip"))) {
            m_flipV->setChecked(true);
        }
    }
    m_sourceFrameSize = frameSize;
    m_filter.reset(new EffectStackFilter(this));
    m_filter->setSourceModel(m_model.get());
    m_effectsTree->setModel(m_filter.get());
    m_effectsTree->setColumnHidden(1, true);
    m_effectsTree->setAcceptDrops(true);
    m_effectsTree->setDragDropMode(QAbstractItemView::DragDrop);
    m_effectsTree->setDragEnabled(true);
    m_effectsTree->setUniformRowHeights(false);
    m_mutex.unlock();
    loadEffects();
    connect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
    connect(m_model.get(), &EffectStackModel::enabledStateChanged, this, &EffectStackView::changeEnabledState);
    connect(m_model.get(), &EffectStackModel::currentChanged, this, &EffectStackView::activateEffect, Qt::DirectConnection);
    connect(this, &EffectStackView::removeCurrentEffect, m_model.get(), &EffectStackModel::removeCurrentEffect);
    m_scrollTimer.start();
    // m_builtStack->setModel(model, stackOwner());
}

void EffectStackView::activateEffect(const QModelIndex &ix, bool active)
{
    const QModelIndex mapped = m_filter->mapFromSource(ix);
    m_effectsTree->setCurrentIndex(mapped);
    auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(mapped));
    if (w) {
        w->slotActivateEffect(active);
    }
}

void EffectStackView::setDropTargetEffect(const QModelIndex &ix, bool active)
{
    const QModelIndex mapped = m_filter->mapFromSource(ix);
    m_effectsTree->setCurrentIndex(mapped);
    auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(mapped));
    if (w) {
        w->slotSetTargetEffect(active);
    }
}

void EffectStackView::changeEnabledState()
{
    int max = m_model->rowCount();
    int currentActive = m_model->getActiveEffect();
    if (currentActive < max && currentActive > -1) {
        auto item = m_model->getEffectStackRow(currentActive);
        QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(item));
        auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        if (w) {
            w->updateScene();
        }
    }
    Q_EMIT updateEnabledState();
}

void EffectStackView::loadEffects()
{
    QMutexLocker lock(&m_mutex);
    m_model->plugBuiltinEffects();
    int max = m_model->rowCount();
    int active = 0;
    if (max > 1) {
        active = qBound(0, m_model->getActiveEffect(), max - 1);
    } else if (max == 0) {
        // blank stack
        lock.unlock();
        ObjectId item = m_model->getOwnerId();
        pCore->getMonitor(item.type == KdenliveObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor)
            ->slotShowEffectScene(MonitorSceneDefault);
        updateTreeHeight();
        return;
    }
    bool hasLift = false;
    QModelIndex activeIndex;
    connect(&m_timerHeight, &QTimer::timeout, this, &EffectStackView::updateTreeHeight, Qt::UniqueConnection);
    for (int i = 0; i < max; i++) {
        std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // group, create sub stack
            continue;
        }
        std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
        CollapsibleEffectView *view = nullptr;
        // We need to rebuild the effect view
        if (effectModel->getAssetId() == QLatin1String("lift_gamma_gain")) {
            hasLift = true;
        }
        if (!effectModel->isAssetEnabled() && !KdenliveSettings::enableBuiltInEffects() && effectModel->filter().get_int("disable") == 1 &&
            effectModel->filter().get_int("kdenlive:builtin") == 1) {
            // Disabled built in effect, don't display
            continue;
        }
        const QString assetName = EffectsRepository::get()->getName(effectModel->getAssetId());
        view = new CollapsibleEffectView(assetName, effectModel, m_sourceFrameSize, this);
        connect(view, &CollapsibleEffectView::deleteEffect, this, &EffectStackView::slotDeleteEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        connect(view, &CollapsibleEffectView::reloadEffect, this, &EffectStackView::reloadEffect);
        connect(view, &CollapsibleEffectView::switchHeight, this, &EffectStackView::slotAdjustDelegate, Qt::DirectConnection);
        connect(view, &CollapsibleEffectView::collapseAllEffects, this, &EffectStackView::slotCollapseAllEffects);
        connect(view, &CollapsibleEffectView::activateEffect, this, [=](int row) { m_model->setActiveEffect(row); });
        connect(view, &CollapsibleEffectView::effectNamesUpdated, this,
                [=]() { Q_EMIT m_model->dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectNamesRole}); });
        connect(view, &CollapsibleEffectView::createGroup, m_model.get(), &EffectStackModel::slotCreateGroup);
        connect(view, &CollapsibleEffectView::showEffectZone, pCore.get(), &Core::showEffectZone);
        connect(this, &EffectStackView::blockWheelEvent, view, &CollapsibleEffectView::blockWheelEvent);
        connect(this, &EffectStackView::updateEffectsGroupesInstances, view, &CollapsibleEffectView::updateGroupedInstances);
        connect(view, &CollapsibleEffectView::seekToPos, this, [this](int pos) {
            // at this point, the effects returns a pos relative to the clip. We need to convert it to a global time
            int clipIn = pCore->getItemPosition(m_model->getOwnerId());
            Q_EMIT seekToPos(pos + clipIn);
        });
        connect(this, &EffectStackView::switchCollapsedView, view, &CollapsibleEffectView::switchCollapsed);
        // Install event filter to manage drag and drop from the stack view
        view->installEventFilter(this);

        connect(pCore.get(), &Core::updateEffectZone, view, [=](const QPoint p, bool withUndo) {
            // Update current effect zone
            if (view->isActive()) {
                view->updateInOut({p.x(), p.y()}, withUndo);
            }
        });
        QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(effectModel));
        if (!ix.isValid()) {
            continue;
        }
        if (active == i && (m_model->getActiveEffect() > -1 || !effectModel->isBuiltIn())) {
            effectModel->setActive(true);
            activeIndex = ix;
        }
        m_effectsTree->setIndexWidget(ix, view);
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegateForIndex(ix));
        del->setHeight(ix, view->height());
        view->buttonUp->setEnabled(i > 0);
        view->buttonDown->setEnabled(i < max - 1);
    }
    lock.unlock();
    if (!hasLift) {
        updateTreeHeight();
    }
    if (activeIndex.isValid()) {
        m_effectsTree->setCurrentIndex(activeIndex);
        auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(activeIndex));
        if (w) {
            w->slotActivateEffect(true);
        }
    }
    if (hasLift) {
        // Some effects have a complex timed layout, so we need to wait a bit before getting the correct position for the effect
        QTimer::singleShot(100, this, &EffectStackView::slotFocusEffect);
    } else {
        slotFocusEffect();
    }
    qDebug() << "MUTEX UNLOCK!!!!!!!!!!!! loadEffects";
}

void EffectStackView::slotDeleteEffect(const std::shared_ptr<EffectItemModel> &effect)
{
    auto type = m_model->getOwnerId().type;
    if ((type == KdenliveObjectType::TimelineClip || type == KdenliveObjectType::BinClip) && KdenliveSettings::applyEffectParamsToGroup()) {
        pCore->removeGroupEffect(m_model->getOwnerId(), effect->getAssetId(), effect->getId());
    } else {
        m_model->removeEffect(effect);
    }
}

void EffectStackView::updateTreeHeight()
{
    // For some reason, the treeview height does not update correctly, so enforce it
    QMutexLocker lk(&m_mutex);
    if (!m_model) {
        return;
    }
    int totalHeight = 0;
    for (int j = 0; j < m_model->rowCount(); j++) {
        std::shared_ptr<AbstractEffectItem> item2 = m_model->getEffectStackRow(j);
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item2);
        QModelIndex idx = m_filter->mapFromSource(m_model->getIndexFromItem(eff));
        auto w = m_effectsTree->indexWidget(idx);
        if (w) {
            totalHeight += w->height();
        }
    }
    if (totalHeight != m_effectsTree->height()) {
        m_effectsTree->setFixedHeight(totalHeight);
        m_scrollTimer.start();
    }
}

void EffectStackView::startDrag(const QPixmap pix, const QString assetId, ObjectId sourceObject, int row, bool singleTarget)
{
    auto *drag = new QDrag(this);
    drag->setPixmap(pix);
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/effect"), assetId.toUtf8());
    QStringList dragData = {QString::number(int(sourceObject.type)), QString::number(int(sourceObject.itemId)), QString::number(row)};
    if (sourceObject.type == KdenliveObjectType::BinClip) {
        dragData << QString();
    } else {
        // Keep a reference to the timeline model
        dragData << pCore->currentTimelineId().toString();
    }
    if (singleTarget) {
        dragData << QStringLiteral("1");
    } else {
        dragData << QStringLiteral("0");
    }

    const QByteArray effectSource = dragData.join(QLatin1Char(',')).toLatin1();
    mime->setData(QStringLiteral("kdenlive/effectsource"), effectSource);
    // Assign ownership of the QMimeData object to the QDrag object.
    drag->setMimeData(mime);
    // Start the drag and drop operation
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void EffectStackView::slotCollapseAllEffects(bool collapse)
{
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex ix = m_filter->mapFromSource(m_model->index(i, 0, QModelIndex()));
        CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        if (w) {
            w->collapseEffect(collapse);
        } else {
            qDebug() << " / / / EFFECT ROW: " << i << " NOT FOUND!!";
        }
    }
}

void EffectStackView::slotAdjustDelegate(const std::shared_ptr<EffectItemModel> &effectModel, int newHeight)
{
    if (!m_model) {
        return;
    }
    QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(effectModel));
    if (ix.isValid()) {
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegateForIndex(ix));
        if (del) {
            del->setHeight(ix, newHeight);
            m_timerHeight.start();
        }
    }
}

void EffectStackView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_scrollTimer.start();
}

void EffectStackView::refresh(const QModelIndex &topL, const QModelIndex &bottomR, const QVector<int> &roles)
{
    const QVector<int> doNothingRole = {TimelineModel::EffectNamesRole};
    if (roles == doNothingRole) {
        // We just refreshed effect names, eg when an effect is dis/enabled, nothing else to do
        return;
    }
    const QModelIndex topLeft = m_filter->mapFromSource(topL);
    const QModelIndex bottomRight = m_filter->mapFromSource(bottomR);
    if (!topLeft.isValid() || !bottomRight.isValid()) {
        loadEffects();
        return;
    }
    for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
        for (int j = topLeft.column(); j <= bottomRight.column(); ++j) {
            CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(m_filter->index(i, j, topLeft.parent())));
            if (w) {
                Q_EMIT w->refresh();
            }
        }
    }
}

void EffectStackView::unsetModel(bool reset)
{
    // Release ownership of smart pointer
    Kdenlive::MonitorId id = Kdenlive::NoMonitor;
    if (m_model) {
        ObjectId item = m_model->getOwnerId();
        pCore->showEffectZone(item, {0, 0}, false);
        id = item.type == KdenliveObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor;
        disconnect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
        disconnect(m_model.get(), &EffectStackModel::enabledStateChanged, this, &EffectStackView::changeEnabledState);
        disconnect(this, &EffectStackView::removeCurrentEffect, m_model.get(), &EffectStackModel::removeCurrentEffect);
        disconnect(m_model.get(), &EffectStackModel::currentChanged, this, &EffectStackView::activateEffect);
        disconnect(&m_timerHeight, &QTimer::timeout, this, &EffectStackView::updateTreeHeight);
        Q_EMIT pCore->disconnectEffectStack();
        if (reset) {
            QMutexLocker lock(&m_mutex);
            m_model.reset();
            m_effectsTree->setModel(nullptr);
        }
        if (!KdenliveSettings::enableBuiltInEffects() && m_builtStack) {
            QLayout *lay = m_builtStack->layout();
            if (lay) {
                while (QLayoutItem *item = lay->takeAt(0)) {
                    delete item;
                }
                delete m_flipLabel;
                delete m_flipH;
                delete m_flipV;
                delete m_removeBg;
                delete m_samProgressBar;
                delete m_samAbortButton;
                m_flipLabel = nullptr;
                m_flipH = nullptr;
                m_flipV = nullptr;
                m_removeBg = nullptr;
                m_samProgressBar = nullptr;
                m_samAbortButton = nullptr;
                delete lay;
                m_builtStack->setVisible(false);
            }
        }
        pCore->getMonitor(id)->slotShowEffectScene(MonitorSceneDefault);
    }
}

ObjectId EffectStackView::stackOwner() const
{
    if (m_model) {
        return m_model->getOwnerId();
    }
    return ObjectId();
}

bool EffectStackView::addEffect(const QString &effectId)
{
    if (m_model) {
        return m_model->appendEffect(effectId, true);
    }
    return false;
}

bool EffectStackView::isEmpty() const
{
    return m_model == nullptr ? true : m_model->rowCount() == 0;
}

void EffectStackView::enableStack(bool enable)
{
    if (m_model) {
        Fun redo = [model = m_model, enable]() {
            model->setEffectStackEnabled(enable);
            return true;
        };
        Fun undo = [model = m_model, enable]() {
            model->setEffectStackEnabled(!enable);
            return true;
        };
        redo();
        pCore->pushUndo(undo, redo, enable ? i18n("Enable Effect Stack") : i18n("Disable Effect Stack"));
    }
}

bool EffectStackView::isStackEnabled() const
{
    if (m_model) {
        return m_model->isStackEnabled();
    }
    return false;
}

void EffectStackView::switchCollapsed()
{
    if (m_model) {
        int max = m_model->rowCount();
        int active = qBound(0, m_model->getActiveEffect(), max - 1);
        Q_EMIT switchCollapsedView(active);
    }
}

void EffectStackView::slotFocusEffect()
{
    Q_EMIT scrollView(m_effectsTree->visualRect(m_effectsTree->currentIndex()));
}

void EffectStackView::slotSaveStack()
{
    if (m_model->rowCount() == 1) {
        int currentActive = m_model->getActiveEffect();
        if (currentActive > -1) {
            auto item = m_model->getEffectStackRow(currentActive);
            QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(item));
            auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
            w->slotSaveEffect();
            return;
        }
    }
    if (m_model->rowCount() <= 1) {
        KMessageBox::error(this, i18n("No effect selected."));
        return;
    }
    QDialog dialog(this);
    QFormLayout form(&dialog);

    dialog.setWindowTitle(i18nc("@title:window", "Save current Effect Stack"));

    auto *stackName = new QLineEdit(&dialog);
    auto *stackDescription = new QTextEdit(&dialog);
    
    form.addRow(i18n("Name for saved stack:"), stackName);
    form.addRow(i18n("Description:"), stackDescription);


    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);

    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString name = stackName->text().simplified();
        if (name.isEmpty()) {
            KMessageBox::error(this, i18n("No name provided, effect stack not saved."));
            return;
        }
        QString description = stackDescription->toPlainText();
        QString effectfilename = QStringUtils::getCleanFileName(name) + QStringLiteral(".xml");

        if (description.trimmed().isEmpty()) {
            if (KMessageBox::questionTwoActions(this, i18n("No description provided. \nSave effect with no description?"), {}, KStandardGuiItem::save(),
                                                KStandardGuiItem::dontSave()) == KMessageBox::SecondaryAction) {
                return;
            }
        }

        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }

        if (dir.exists(effectfilename)) {
            if (KMessageBox::questionTwoActions(this, i18n("File %1 already exists.\nDo you want to overwrite it?", effectfilename), {},
                                                KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction) {
                return;
            }
        }

        QDomDocument doc;
        
        QDomElement effect = doc.createElement(QStringLiteral("effectgroup"));
        effect.setAttribute(QStringLiteral("id"), name);

        QDomElement describtionNode = doc.createElement(QStringLiteral("description"));
        QDomText descriptionText = doc.createTextNode(description);
        describtionNode.appendChild(descriptionText);
        
        effect.appendChild(describtionNode);
        effect.setAttribute(QStringLiteral("description"), description);

        auto item = m_model->getEffectStackRow(0);
        if (item->isAudio()) {
            effect.setAttribute(QStringLiteral("type"), QStringLiteral("customAudio"));
        }
        effect.setAttribute(QStringLiteral("parentIn"), pCore->getItemIn(m_model->getOwnerId()));
        doc.appendChild(effect);
        for (int i = 0; i < m_model->rowCount(); ++i) {
            QModelIndex ix = m_filter->mapFromSource(m_model->index(i, 0, QModelIndex()));
            CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
            if (w) {
                effect.appendChild(doc.importNode(w->toXml().documentElement(), true));
            } else {
                qDebug() << " / / / EFFECT ROW: " << i << " NOT FOUND!!";
            }
        }
        QFile file(dir.absoluteFilePath(effectfilename));
        if (file.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream out(&file);
            out << doc.toString();
        } else {
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", file.fileName()));
        }
        file.close();
        Q_EMIT reloadEffect(dir.absoluteFilePath(effectfilename));
    }
}

void EffectStackView::slotGoToKeyframe(bool next)
{
    int max = m_model->rowCount();
    int currentActive = m_model->getActiveEffect();
    if (currentActive < max && currentActive > -1) {
        auto item = m_model->getEffectStackRow(currentActive);
        QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(item));
        auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        if (next) {
            w->slotNextKeyframe();
        } else {
            w->slotPreviousKeyframe();
        }
    }
}

void EffectStackView::addRemoveKeyframe()
{
    int max = m_model->rowCount();
    int currentActive = m_model->getActiveEffect();
    if (currentActive < max && currentActive > -1) {
        auto item = m_model->getEffectStackRow(currentActive);
        QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(item));
        auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        w->addRemoveKeyframe();
    }
}

void EffectStackView::sendStandardCommand(int command)
{
    int max = m_model->rowCount();
    int currentActive = m_model->getActiveEffect();
    if (currentActive < max && currentActive > -1) {
        auto item = m_model->getEffectStackRow(currentActive);
        QModelIndex ix = m_filter->mapFromSource(m_model->getIndexFromItem(item));
        auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        w->sendStandardCommand(command);
    }
}

bool EffectStackView::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress) {
        auto me = static_cast<QMouseEvent *>(e);
        m_dragStart = me->globalPosition().toPoint();
        m_dragging = false;
        if (o) {
            auto coll = static_cast<CollapsibleEffectView *>(o);
            if (coll && !coll->isActive()) {
                m_model->setActiveEffect(coll->getEffectRow());
            }
        }
        e->accept();
        return true;
    }
    if (e->type() == QEvent::MouseMove) {
        auto me = static_cast<QMouseEvent *>(e);
        if (!m_dragging && (me->globalPosition().toPoint() - m_dragStart).manhattanLength() > QApplication::startDragDistance()) {
            m_dragging = true;
            if (o) {
                auto coll = static_cast<CollapsibleEffectView *>(o);
                if (coll) {
                    ObjectId item = m_model->getOwnerId();
                    startDrag(coll->getDragPixmap(), coll->getAssetId(), item, coll->getEffectRow(), me->modifiers() & Qt::AltModifier);
                }
            }
        }
        e->accept();
        return true;
    }
    return QWidget::eventFilter(o, e);
}

void EffectStackView::updateSamProgress(int progress)
{
    if (m_removeBg) {
        if (progress == 100) {
            m_samAbortButton->setVisible(false);
            m_samProgressBar->setVisible(false);
            m_removeBg->setVisible(true);
            return;
        }
        if (!m_samProgressBar->isVisible()) {
            m_removeBg->setVisible(false);
            m_samAbortButton->setVisible(true);
            m_samProgressBar->setVisible(true);
        }
        m_samProgressBar->setValue(progress);
    }
}
