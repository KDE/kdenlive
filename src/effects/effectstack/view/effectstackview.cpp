/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "effectstackview.hpp"
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"
#include "assets/assetpanel.hpp"
#include "assets/view/assetparameterview.hpp"
#include "builtstack.hpp"
#include "collapsibleeffectview.hpp"
#include "core.h"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QMimeData>
#include <QMutexLocker>
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QDir>

#include <KMessageBox>
#include <utility>

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
    emit sizeHintChanged(index);
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
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
}

EffectStackView::EffectStackView(AssetPanel *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_thumbnailer(new AssetIconProvider(true))
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
    QString style = QStringLiteral("QTreeView {border: none;}");
    // m_effectsTree->viewport()->setAutoFillBackground(false);
    m_effectsTree->setStyleSheet(style);
    m_effectsTree->setVisible(!KdenliveSettings::showbuiltstack());
    m_lay->addWidget(m_effectsTree);
    m_lay->addStretch(10);

    m_scrollTimer.setSingleShot(true);
    m_scrollTimer.setInterval(250);
    connect(&m_scrollTimer, &QTimer::timeout, this, &EffectStackView::checkScrollBar);

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

void EffectStackView::dropEvent(QDropEvent *event)
{
    event->accept();
    QString effectId = event->mimeData()->data(QStringLiteral("kdenlive/effect"));
    int row = m_model->rowCount();
    for (int i = 0; i < m_model->rowCount(); i++) {
        auto item = m_model->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // TODO: group
            continue;
        }
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
        QModelIndex ix = m_model->getIndexFromItem(eff);
        QWidget *w = m_effectsTree->indexWidget(ix);
        if (w && w->geometry().contains(event->pos())) {
            qDebug() << "// DROPPED ON EFF: " << eff->getAssetId();
            row = i;
            break;
        }
    }
    if (event->source() == this) {
        QString sourceData = event->mimeData()->data(QStringLiteral("kdenlive/effectsource"));
        int oldRow = sourceData.section(QLatin1Char('-'), 2, 2).toInt();
        qDebug() << "// MOVING EFFECT FROM : " << oldRow << " TO " << row;
        if (row == oldRow || (row == m_model->rowCount() && oldRow == row - 1)) {
            return;
        }
        m_model->moveEffect(row, m_model->getEffectStackRow(oldRow));
    } else {
        bool added = false;
        if (row < m_model->rowCount()) {
            if (m_model->appendEffect(effectId) && m_model->rowCount() > 0) {
                added = true;
                m_model->moveEffect(row, m_model->getEffectStackRow(m_model->rowCount() - 1));
            }
        } else {
            if (m_model->appendEffect(effectId) && m_model->rowCount() > 0) {
                added = true;
                std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(m_model->rowCount() - 1);
                if (item) {
                    slotActivateEffect(std::static_pointer_cast<EffectItemModel>(item));
                }
            }
        }
        if (!added) {
            pCore->displayMessage(i18n("Cannot add effect to clip"), InformationMessage);
        } else {
            m_scrollTimer.start();
        }
    }
}

void EffectStackView::setModel(std::shared_ptr<EffectStackModel> model, const QSize frameSize)
{
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! setmodel";
    m_mutex.lock();
    unsetModel(false);
    m_model = std::move(model);
    m_sourceFrameSize = frameSize;
    m_effectsTree->setModel(m_model.get());
    m_effectsTree->setItemDelegateForColumn(0, new WidgetDelegate(this));
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
    connect(this, &EffectStackView::removeCurrentEffect, m_model.get(), &EffectStackModel::removeCurrentEffect);
    // m_builtStack->setModel(model, stackOwner());
}

void EffectStackView::changeEnabledState()
{
    int max = m_model->rowCount();
    int currentActive = m_model->getActiveEffect();
    if (currentActive < max && currentActive > -1) {
        auto item = m_model->getEffectStackRow(currentActive);
        QModelIndex ix = m_model->getIndexFromItem(item);
        CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
        w->updateScene();
    }
    emit updateEnabledState();
}

void EffectStackView::loadEffects()
{
    //QMutexLocker lock(&m_mutex);
    int max = m_model->rowCount();
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! loadEffects COUNT: "<<max;
    if (max == 0) {
        // blank stack
        ObjectId item = m_model->getOwnerId();
        pCore->getMonitor(item.first == ObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor)->slotShowEffectScene(MonitorSceneDefault);
        return;
    }
    int active = qBound(0, m_model->getActiveEffect(), max - 1);
    QModelIndex activeIndex;
    for (int i = 0; i < max; i++) {
        std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(i);
        QSize size;
        if (item->childCount() > 0) {
            // group, create sub stack
            continue;
        }
        std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
        CollapsibleEffectView *view = nullptr;
        // We need to rebuild the effect view
        QImage effectIcon = m_thumbnailer->requestImage(effectModel->getAssetId(), &size, QSize(QStyle::PM_SmallIconSize, QStyle::PM_SmallIconSize));
        view = new CollapsibleEffectView(effectModel, m_sourceFrameSize, effectIcon, this);
        connect(view, &CollapsibleEffectView::deleteEffect, m_model.get(), &EffectStackModel::removeEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        connect(view, &CollapsibleEffectView::reloadEffect, this, &EffectStackView::reloadEffect);
        connect(view, &CollapsibleEffectView::switchHeight, this, &EffectStackView::slotAdjustDelegate, Qt::DirectConnection);
        connect(view, &CollapsibleEffectView::startDrag, this, &EffectStackView::slotStartDrag);
        connect(view, &CollapsibleEffectView::saveStack, this, &EffectStackView::slotSaveStack);
        connect(view, &CollapsibleEffectView::createGroup, m_model.get(), &EffectStackModel::slotCreateGroup);
        connect(view, &CollapsibleEffectView::activateEffect, this, &EffectStackView::slotActivateEffect);
        connect(this, &EffectStackView::blockWheenEvent, view, &CollapsibleEffectView::blockWheenEvent);
        connect(view, &CollapsibleEffectView::seekToPos, this, [this](int pos) {
            // at this point, the effects returns a pos relative to the clip. We need to convert it to a global time
            int clipIn = pCore->getItemPosition(m_model->getOwnerId());
            emit seekToPos(pos + clipIn);
        });
        connect(this, &EffectStackView::switchCollapsedView, view, &CollapsibleEffectView::switchCollapsed);
        QModelIndex ix = m_model->getIndexFromItem(effectModel);
        m_effectsTree->setIndexWidget(ix, view);
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegate(ix));
        del->setHeight(ix, view->height());
        view->buttonUp->setEnabled(i > 0);
        view->buttonDown->setEnabled(i < max - 1);
        if (i == active) {
            activeIndex = ix;
        }
    }
    updateTreeHeight();
    if (activeIndex.isValid()) {
        doActivateEffect(active, activeIndex, true);
    }
    qDebug() << "MUTEX UNLOCK!!!!!!!!!!!! loadEffects";
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
    m_effectsTree->setFixedHeight(totalHeight);
    m_scrollTimer.start();
}

void EffectStackView::slotActivateEffect(const std::shared_ptr<EffectItemModel> &effectModel)
{
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! slotactivateeffect: " << effectModel->row();
    QMutexLocker lock(&m_mutex);
    QModelIndex activeIx = m_model->getIndexFromItem(effectModel);
    doActivateEffect(effectModel->row(), activeIx);
    qDebug() << "MUTEX UNLOCK!!!!!!!!!!!! slotactivateeffect";
}

void EffectStackView::slotStartDrag(const QPixmap &pix, const std::shared_ptr<EffectItemModel> &effectModel)
{
    auto *drag = new QDrag(this);
    drag->setPixmap(pix);
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/effect"), effectModel->getAssetId().toUtf8());
    // TODO this will break if source effect is not on the stack of a timeline clip
    ObjectId source = effectModel->getOwnerId();
    QByteArray effectSource;
    effectSource += QString::number((int)source.first).toUtf8();
    effectSource += '-';
    effectSource += QString::number((int)source.second).toUtf8();
    effectSource += '-';
    effectSource += QString::number(effectModel->row()).toUtf8();
    mime->setData(QStringLiteral("kdenlive/effectsource"), effectSource);
    // mime->setData(QStringLiteral("kdenlive/effectrow"), QString::number(effectModel->row()).toUtf8());

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
        auto *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegate(ix));
        if (del) {
            del->setHeight(ix, newHeight);
            QMetaObject::invokeMethod(this, "updateTreeHeight", Qt::QueuedConnection);
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
                emit w->refresh();
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
        id = item.first == ObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor;
        disconnect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
        disconnect(m_model.get(), &EffectStackModel::enabledStateChanged, this, &EffectStackView::changeEnabledState);
        disconnect(this, &EffectStackView::removeCurrentEffect, m_model.get(), &EffectStackModel::removeCurrentEffect);
    }
    if (reset) {
        QMutexLocker lock(&m_mutex);
        m_model.reset();
        m_effectsTree->setModel(nullptr);
    }
    if (id != Kdenlive::NoMonitor) {
        pCore->getMonitor(id)->slotShowEffectScene(MonitorSceneDefault);
    }
}

ObjectId EffectStackView::stackOwner() const
{
    if (m_model) {
        return m_model->getOwnerId();
    }
    return ObjectId(ObjectType::NoItem, -1);
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
        emit switchCollapsedView(active);
    }
}

void EffectStackView::doActivateEffect(int row, QModelIndex activeIx, bool force)
{
    int currentActive = m_model->getActiveEffect();
    if (row == currentActive && !force) {
        // Effect is already active
        return;
    }
    if (row != currentActive && currentActive > -1 && currentActive < m_model->rowCount()) {
        auto item = m_model->getEffectStackRow(currentActive);
        if (item) {
            QModelIndex ix = m_model->getIndexFromItem(item);
            CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(ix));
            if (w) {
                w->slotActivateEffect(false);
            }
        }
    }
    m_model->setActiveEffect(row);
    CollapsibleEffectView *w = static_cast<CollapsibleEffectView *>(m_effectsTree->indexWidget(activeIx));
    if (w) {
        w->slotActivateEffect(true);
    }
}

void EffectStackView::slotSaveStack()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect Stack"), i18n("Name for saved stack: "));
    if (name.trimmed().isEmpty() || m_model->rowCount() <= 0) {
        return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (dir.exists(name + QStringLiteral(".xml"))) {
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", name + QStringLiteral(".xml"))) == KMessageBox::No) {
            return;
        }
    }

    QDomDocument doc;
    QDomElement effect = doc.createElement(QStringLiteral("effectgroup"));
    effect.setAttribute(QStringLiteral("id"), name);
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
    QFile file(dir.absoluteFilePath(name + QStringLiteral(".xml")));
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&file);
        out << doc.toString();
    }
    file.close();
    emit reloadEffect(dir.absoluteFilePath(name + QStringLiteral(".xml")));
}

/*
void EffectStackView::switchBuiltStack(bool show)
{
    m_builtStack->setVisible(show);
    m_effectsTree->setVisible(!show);
    KdenliveSettings::setShowbuiltstack(show);
}
*/
