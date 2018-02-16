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

#include <QDrag>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QMimeData>
#include <QMutexLocker>
#include <QTreeView>
#include <QVBoxLayout>

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
    , m_range(-1, -1)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(0);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setAcceptDrops(true);
    /*m_builtStack = new BuiltStack(parent);
    m_builtStack->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_lay->addWidget(m_builtStack);
    m_builtStack->setVisible(KdenliveSettings::showbuiltstack());*/
    m_effectsTree = new QTreeView(this);
    m_effectsTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_effectsTree->setHeaderHidden(true);
    m_effectsTree->setRootIsDecorated(false);
    QString style = QStringLiteral("QTreeView {border: none;}");
    // m_effectsTree->viewport()->setAutoFillBackground(false);
    m_effectsTree->setStyleSheet(style);
    m_effectsTree->setVisible(!KdenliveSettings::showbuiltstack());
    m_lay->addWidget(m_effectsTree);
    m_lay->setStretch(1, 10);
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
        int oldRow = event->mimeData()->data(QStringLiteral("kdenlive/effectrow")).toInt();
        qDebug() << "// MOVING EFFECT FROM : " << oldRow << " TO " << row;
        m_model->moveEffect(row, m_model->getEffectStackRow(oldRow));
    } else {
        if (row < m_model->rowCount()) {
            m_model->appendEffect(effectId);
            m_model->moveEffect(row, m_model->getEffectStackRow(m_model->rowCount() - 1));
        } else {
            m_model->appendEffect(effectId);
        }
    }
}

void EffectStackView::setModel(std::shared_ptr<EffectStackModel> model, QPair<int, int> range, const QSize frameSize)
{
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! setmodel";
    m_mutex.lock();
    unsetModel();
    m_model = model;
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
    loadEffects(range);
    connect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
    //m_builtStack->setModel(model, stackOwner());
}

void EffectStackView::loadEffects(QPair<int, int> range, int start, int end)
{
    Q_UNUSED(start)
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! loadEffects";
    QMutexLocker lock(&m_mutex);
    m_range = range;
    int max = m_model->rowCount();
    if (end == -1) {
        end = max;
    }
    int active = m_model->getActiveEffect();
    for (int i = 0; i < end; i++) {
        std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(i);
        QSize size;
        if (item->childCount() > 0) {
            // group, create sub stack
            continue;
        }
        std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
        QImage effectIcon = m_thumbnailer->requestImage(effectModel->getAssetId(), &size, QSize(QStyle::PM_SmallIconSize, QStyle::PM_SmallIconSize));
        CollapsibleEffectView *view = new CollapsibleEffectView(effectModel, range, m_sourceFrameSize, effectIcon, this);
        qDebug() << "__ADDING EFFECT: " << effectModel->filter().get("id") << ", ACT: " << active;
        if (i == active) {
            view->slotActivateEffect(m_model->getIndexFromItem(effectModel));
        }
        view->buttonUp->setEnabled(i > 0);
        view->buttonDown->setEnabled(i < max - 1);
        connect(view, &CollapsibleEffectView::deleteEffect, m_model.get(), &EffectStackModel::removeEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        connect(view, &CollapsibleEffectView::switchHeight, this, &EffectStackView::slotAdjustDelegate, Qt::DirectConnection);
        connect(view, &CollapsibleEffectView::startDrag, this, &EffectStackView::slotStartDrag);
        connect(view, &CollapsibleEffectView::createGroup, m_model.get(), &EffectStackModel::slotCreateGroup);
        connect(view, &CollapsibleEffectView::activateEffect, this, &EffectStackView::slotActivateEffect);
        connect(view, &CollapsibleEffectView::seekToPos, [this](int pos) {
            // at this point, the effects returns a pos relative to the clip. We need to convert it to a global time
            int clipIn = pCore->getItemIn(m_model->getOwnerId());
            emit seekToPos(pos + clipIn);
        });
        connect(this, &EffectStackView::doActivateEffect, view, &CollapsibleEffectView::slotActivateEffect);
        QModelIndex ix = m_model->getIndexFromItem(effectModel);
        m_effectsTree->setIndexWidget(ix, view);
        WidgetDelegate *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegate(ix));
        del->setHeight(ix, view->height());
    }
    updateTreeHeight();
    qDebug() << "MUTEX UNLOCK!!!!!!!!!!!! loadEffects";
}

void EffectStackView::updateTreeHeight()
{
    // For some reason, the treeview height does not update correctly, so enforce it
    int totalHeight = 0;
    for (int j = 0; j < m_model->rowCount(); j++) {
        std::shared_ptr<AbstractEffectItem> item2 = m_model->getEffectStackRow(j);
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item2);
        QModelIndex idx = m_model->getIndexFromItem(eff);
        auto w = m_effectsTree->indexWidget(idx);
        totalHeight += w->height();
    }
    setMinimumHeight(totalHeight);
}

void EffectStackView::slotActivateEffect(std::shared_ptr<EffectItemModel> effectModel)
{
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! slotactivateeffect";
    QMutexLocker lock(&m_mutex);
    m_model->setActiveEffect(effectModel->row());
    QModelIndex activeIx = m_model->getIndexFromItem(effectModel);
    emit doActivateEffect(activeIx);
    qDebug() << "MUTEX UNLOCK!!!!!!!!!!!! slotactivateeffect";
}

void EffectStackView::slotStartDrag(QPixmap pix, std::shared_ptr<EffectItemModel> effectModel)
{
    auto *drag = new QDrag(this);
    drag->setPixmap(pix);
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/effect"), effectModel->getAssetId().toUtf8());
    // TODO this will break if source effect is not on the stack of a timeline clip
    QByteArray effectSource;
    effectSource += QString::number((int)effectModel->getOwnerId().first).toUtf8();
    effectSource += '-';
    effectSource += QString::number((int)effectModel->getOwnerId().second).toUtf8();
    effectSource += '-';
    effectSource += QString::number(effectModel->row()).toUtf8();
    mime->setData(QStringLiteral("kdenlive/effectsource"), effectSource);
    // mime->setData(QStringLiteral("kdenlive/effectrow"), QString::number(effectModel->row()).toUtf8());

    // Assign ownership of the QMimeData object to the QDrag object.
    drag->setMimeData(mime);
    // Start the drag and drop operation
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void EffectStackView::slotAdjustDelegate(std::shared_ptr<EffectItemModel> effectModel, int height)
{
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! adjustdelegate: "<<height;
    QMutexLocker lock(&m_mutex);
    QModelIndex ix = m_model->getIndexFromItem(effectModel);
    WidgetDelegate *del = static_cast<WidgetDelegate *>(m_effectsTree->itemDelegate(ix));
    del->setHeight(ix, height);
    updateTreeHeight();
    qDebug() << "MUTEX UNLOCK!!!!!!!!!!!! adjustdelegate";
}

void EffectStackView::refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles)
    loadEffects(m_range, topLeft.row(), bottomRight.row() + 1);
}

void EffectStackView::unsetModel(bool reset)
{
    // Release ownership of smart pointer
    if (m_model) {
        disconnect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
        // Make sure to delete the delegates
        for (int i = 0; i < m_model->rowCount(); i++) {
            std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(i);
            std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
            QModelIndex ix = m_model->getIndexFromItem(eff);
            auto w = m_effectsTree->indexWidget(ix);
            delete w;
        }
    }
    if (reset) {
        m_model.reset();
    }    
}

void EffectStackView::setRange(int in, int out)
{
    qDebug() << "MUTEX LOCK!!!!!!!!!!!! setrange";
    QMutexLocker lock(&m_mutex);
    m_range.first = in;
    m_range.second = out;
    int max = m_model->rowCount();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(i);
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
        QModelIndex ix = m_model->getIndexFromItem(eff);
        auto w = m_effectsTree->indexWidget(ix);
        static_cast<CollapsibleEffectView *>(w)->setRange(m_range);
    }
}

ObjectId EffectStackView::stackOwner() const
{
    if (m_model) {
        return m_model->getOwnerId();
    }
    return ObjectId(ObjectType::NoItem, -1);
}

/*
void EffectStackView::switchBuiltStack(bool show)
{
    m_builtStack->setVisible(show);
    m_effectsTree->setVisible(!show);
    KdenliveSettings::setShowbuiltstack(show);
}
*/
