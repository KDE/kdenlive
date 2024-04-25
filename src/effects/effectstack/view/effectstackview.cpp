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
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"

#include <QDir>
#include <QDrag>
#include <QFormLayout>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainter>
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>

#include "utils/KMessageBox_KdenliveCompat.h"
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
        s.setHeight(m_height.value(index));
    }
    return s;
}

void WidgetDelegate::setHeight(const QModelIndex &index, int height)
{
    m_height[index] = height;
    Q_EMIT sizeHintChanged(index);
}

int WidgetDelegate::height(const QModelIndex &index) const
{
    return m_height.value(index);
}

void WidgetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    if (index.row() == dragRow && !opt.rect.isNull()) {
        QPen pen(QPalette().highlight().color());
        pen.setWidth(4);
        painter->setPen(pen);
        painter->drawLine(opt.rect.topLeft(), opt.rect.topRight());
    }

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
    /*m_builtStack = new BuiltStack(parent);
    m_builtStack->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_lay->addWidget(m_builtStack);
    m_builtStack->setVisible(KdenliveSettings::showbuiltstack());*/
    m_effectsTree = new QTreeView(this);
    m_effectsTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    m_effectsTree->setHeaderHidden(true);
    m_effectsTree->setRootIsDecorated(false);
    m_effectsTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QString style = QStringLiteral("QTreeView {border: none;}");
    // m_effectsTree->viewport()->setAutoFillBackground(false);
    m_effectsTree->setStyleSheet(style);
    m_effectsTree->setVisible(!KdenliveSettings::showbuiltstack());
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
    event->accept();
    dragRow = -1;
    repaint();
}

void EffectStackView::dragMoveEvent(QDragMoveEvent *event)
{
    dragRow = m_model->rowCount();
    for (int i = 0; i < m_model->rowCount(); i++) {
        auto item = m_model->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // TODO: group
            continue;
        }
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
        QModelIndex ix = m_model->getIndexFromItem(eff);
        QWidget *w = m_effectsTree->indexWidget(ix);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (w && w->geometry().contains(event->pos())) {
#else
        if (w && w->geometry().contains(event->position().toPoint())) {
#endif
            if (event->source() == this) {
                QString sourceData = event->mimeData()->data(QStringLiteral("kdenlive/effectsource"));
                int oldRow = sourceData.section(QLatin1Char(','), 2, 2).toInt();
                if (i == oldRow + 1) {
                    dragRow = -1;
                    break;
                }
            }
            dragRow = i;
            break;
        }
    }
    if (dragRow == m_model->rowCount() && event->source() == this) {
        QString sourceData = event->mimeData()->data(QStringLiteral("kdenlive/effectsource"));
        int oldRow = sourceData.section(QLatin1Char(','), 2, 2).toInt();
        if (dragRow == oldRow + 1) {
            dragRow = -1;
        }
    }
    repaint();
}

void EffectStackView::dropEvent(QDropEvent *event)
{
    qDebug() << ":::: DROP BEGIN EVENT....";
    if (dragRow < 0) {
        return;
    }
    QString effectId = event->mimeData()->data(QStringLiteral("kdenlive/effect"));
    if (event->source() == this) {
        QString sourceData = event->mimeData()->data(QStringLiteral("kdenlive/effectsource"));
        int oldRow = sourceData.section(QLatin1Char(','), 2, 2).toInt();
        qDebug() << "// MOVING EFFECT FROM : " << oldRow << " TO " << dragRow;
        if (dragRow == oldRow || (dragRow == m_model->rowCount() && oldRow == dragRow - 1)) {
            return;
        }
        QMetaObject::invokeMethod(m_model.get(), "moveEffectByRow", Qt::QueuedConnection, Q_ARG(int, dragRow), Q_ARG(int, oldRow));
    } else {
        bool added = false;
        if (dragRow < m_model->rowCount()) {
            if (m_model->appendEffect(effectId) && m_model->rowCount() > 0) {
                added = true;
                m_model->moveEffect(dragRow, m_model->getEffectStackRow(m_model->rowCount() - 1));
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
    dragRow = -1;
    event->acceptProposedAction();
    qDebug() << ":::: DROP END EVENT....";
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
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! setmodel";
    m_mutex.lock();
    QItemSelectionModel *m = m_effectsTree->selectionModel();
    unsetModel(false);
    m_effectsTree->setModel(nullptr);
    m_model.reset();
    if (m) {
        delete m;
    }
    m_effectsTree->setFixedHeight(0);
    m_model = std::move(model);
    m_sourceFrameSize = frameSize;
    m_effectsTree->setModel(m_model.get());
    m_effectsTree->setColumnHidden(1, true);
    m_effectsTree->setAcceptDrops(true);
    m_effectsTree->setDragDropMode(QAbstractItemView::DragDrop);
    m_effectsTree->setDragEnabled(true);
    m_effectsTree->setUniformRowHeights(false);
    m_mutex.unlock();
    qDebug() << "MUTEX UNLOCK!!!!!!!!!!!! setmodel";
    loadEffects();
    m_scrollTimer.start();
    connect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
    connect(m_model.get(), &EffectStackModel::enabledStateChanged, this, &EffectStackView::changeEnabledState);
    connect(m_model.get(), &EffectStackModel::currentChanged, this, &EffectStackView::activateEffect, Qt::DirectConnection);
    connect(this, &EffectStackView::removeCurrentEffect, m_model.get(), &EffectStackModel::removeCurrentEffect);
    // m_builtStack->setModel(model, stackOwner());
}

void EffectStackView::activateEffect(const QModelIndex &ix, bool active)
{
    m_effectsTree->setCurrentIndex(ix);
    auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
    if (w) {
        w->slotActivateEffect(active);
    }
}

void EffectStackView::changeEnabledState()
{
    int max = m_model->rowCount();
    int currentActive = m_model->getActiveEffect();
    if (currentActive < max && currentActive > -1) {
        auto item = m_model->getEffectStackRow(currentActive);
        QModelIndex ix = m_model->getIndexFromItem(item);
        auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        w->updateScene();
    }
    Q_EMIT updateEnabledState();
}

void EffectStackView::loadEffects()
{
    // QMutexLocker lock(&m_mutex);
    int max = m_model->rowCount();
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! loadEffects COUNT: " << max;
    if (max == 0) {
        // blank stack
        ObjectId item = m_model->getOwnerId();
        pCore->getMonitor(item.type == KdenliveObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor)
            ->slotShowEffectScene(MonitorSceneDefault);
        updateTreeHeight();
        return;
    }
    int active = qBound(0, m_model->getActiveEffect(), max - 1);
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
        const QString assetName = EffectsRepository::get()->getName(effectModel->getAssetId());
        view = new CollapsibleEffectView(assetName, effectModel, m_sourceFrameSize, this);
        connect(view, &CollapsibleEffectView::deleteEffect, this, &EffectStackView::slotDeleteEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        connect(view, &CollapsibleEffectView::reloadEffect, this, &EffectStackView::reloadEffect);
        connect(view, &CollapsibleEffectView::switchHeight, this, &EffectStackView::slotAdjustDelegate, Qt::DirectConnection);
        connect(view, &CollapsibleEffectView::startDrag, this, &EffectStackView::slotStartDrag);
        connect(view, &CollapsibleEffectView::activateEffect, this, [=](int row) { m_model->setActiveEffect(row); });
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

        connect(pCore.get(), &Core::updateEffectZone, view, [=](const QPoint p, bool withUndo) {
            // Update current effect zone
            if (view->isActive()) {
                view->updateInOut({p.x(), p.y()}, withUndo);
            }
        });
        QModelIndex ix = m_model->getIndexFromItem(effectModel);
        if (active == i) {
            effectModel->setActive(true);
            activeIndex = ix;
        }
        m_effectsTree->setIndexWidget(ix, view);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegate(ix));
#else
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegateForIndex(ix));
#endif
        del->setHeight(ix, view->height());
        view->buttonUp->setEnabled(i > 0);
        view->buttonDown->setEnabled(i < max - 1);
    }
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
    if (KdenliveSettings::applyEffectParamsToGroup()) {
        pCore->removeGroupEffect(m_model->getOwnerId(), effect->getAssetId());
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
        QModelIndex idx = m_model->getIndexFromItem(eff);
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

void EffectStackView::slotStartDrag(const QPixmap pix, const QString assetId, ObjectId sourceObject, int row, bool singleTarget)
{
    auto *drag = new QDrag(this);
    drag->setPixmap(pix);
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/effect"), assetId.toUtf8());
    QByteArray effectSource;
    effectSource += QString::number(int(sourceObject.type)).toUtf8();
    effectSource += ',';
    effectSource += QString::number(int(sourceObject.itemId)).toUtf8();
    effectSource += ',';
    effectSource += QString::number(row).toUtf8();
    effectSource += ',';
    if (sourceObject.type == KdenliveObjectType::BinClip) {
        effectSource += QByteArray();
    } else {
        // Keep a reference to the timeline model
        effectSource += pCore->currentTimelineId().toString().toUtf8();
    }
    if (singleTarget) {
        effectSource += QStringLiteral(",1").toUtf8();
    } else {
        effectSource += QStringLiteral(",0").toUtf8();
    }
    mime->setData(QStringLiteral("kdenlive/effectsource"), effectSource);

    // Assign ownership of the QMimeData object to the QDrag object.
    drag->setMimeData(mime);
    // Start the drag and drop operation
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void EffectStackView::slotAdjustDelegate(const std::shared_ptr<EffectItemModel> &effectModel, int newHeight)
{
    if (!m_model) {
        return;
    }
    QModelIndex ix = m_model->getIndexFromItem(effectModel);
    if (ix.isValid()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegate(ix));
#else
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegateForIndex(ix));
#endif
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

void EffectStackView::refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles)
    if (!topLeft.isValid() || !bottomRight.isValid()) {
        loadEffects();
        return;
    }
    for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
        for (int j = topLeft.column(); j <= bottomRight.column(); ++j) {
            CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(m_model->index(i, j, topLeft.parent())));
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
        m_model->setEffectStackEnabled(enable);
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
            QModelIndex ix = m_model->getIndexFromItem(item);
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
        QString name = stackName->text();
        QString description = stackDescription->toPlainText();
        if (name.trimmed().isEmpty()) {
            KMessageBox::error(this, i18n("No name provided, effect stack not saved."));
            return;
        }

        QString effectfilename = name + QStringLiteral(".xml");

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
        
        QDomElement describtionNode = doc.createElement(QString("description"));
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
        for (int i = 0; i <= m_model->rowCount(); ++i) {
            CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(m_model->index(i, 0, QModelIndex())));
            if (w) {
                effect.appendChild(doc.importNode(w->toXml().documentElement(), true));
            }
        }
        QFile file(dir.absoluteFilePath(effectfilename));
        if (file.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream out(&file);
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            out.setCodec("UTF-8");
    #endif
            out << doc.toString();
        } else {
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", file.fileName()));
        }
        file.close();
        Q_EMIT reloadEffect(dir.absoluteFilePath(effectfilename));
    }
}

/*
void EffectStackView::switchBuiltStack(bool show)
{
    m_builtStack->setVisible(show);
    m_effectsTree->setVisible(!show);
    KdenliveSettings::setShowbuiltstack(show);
}
*/

void EffectStackView::slotGoToKeyframe(bool next)
{
    int max = m_model->rowCount();
    int currentActive = m_model->getActiveEffect();
    if (currentActive < max && currentActive > -1) {
        auto item = m_model->getEffectStackRow(currentActive);
        QModelIndex ix = m_model->getIndexFromItem(item);
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
        QModelIndex ix = m_model->getIndexFromItem(item);
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
        QModelIndex ix = m_model->getIndexFromItem(item);
        auto *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        w->sendStandardCommand(command);
    }
}
