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
#include "collapsibleeffectview.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "assets/view/assetparameterview.hpp"
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"

#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFontDatabase>
#include <QTreeView>
#include <QDrag>

WidgetDelegate::WidgetDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
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
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
    // QRect r = QStyle::alignedRect(opt.direction, Qt::AlignVCenter | Qt::AlignLeft, opt.decorationSize, r1);

    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
}

EffectStackView::EffectStackView(QWidget *parent) : QWidget(parent)
    , m_thumbnailer(new AssetIconProvider(true))
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(0);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setAcceptDrops(true);
    m_effectsTree = new QTreeView(this);
    m_effectsTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_effectsTree->setHeaderHidden(true);
    m_effectsTree->setRootIsDecorated(false);
    QString style = QStringLiteral("QTreeView {border: none;}");
    //m_effectsTree->viewport()->setAutoFillBackground(false);
    m_effectsTree->setStyleSheet(style);
    m_lay->addWidget(m_effectsTree);
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
            //TODO: group
            continue;
        }
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
        QModelIndex ix = m_model->getIndexFromItem(eff);
        QWidget *w = m_effectsTree->indexWidget(ix);
        if (w && w->geometry().contains(event->pos())) {
            qDebug()<<"// DROPPED ON EFF: "<<eff->getAssetId();
            row = i;
            break;
        }
    }
    if (event->source() == this) {
        int oldRow = event->mimeData()->data(QStringLiteral("kdenlive/effectrow")).toInt();
        qDebug()<<"// MOVING EFFECT FROM : "<<oldRow<<" TO "<<row;
        m_model->moveEffect(row, m_model->getEffectStackRow(oldRow));
    } else {
        if (row < m_model->rowCount()) {
            m_model->appendEffect(effectId, property("clipId").toInt());
            m_model->moveEffect(row, m_model->getEffectStackRow(m_model->rowCount() - 1));
        } else {
            m_model->appendEffect(effectId, property("clipId").toInt());
        }
    }
}

void EffectStackView::setModel(std::shared_ptr<EffectStackModel>model)
{
    unsetModel();
    m_model = model;
    m_effectsTree->setModel(m_model.get());
    m_effectsTree->setItemDelegateForColumn(0, new WidgetDelegate(this));
    m_effectsTree->setColumnHidden(1, true);
    m_effectsTree->setAcceptDrops(true);
    m_effectsTree->setDragDropMode(QAbstractItemView::DragDrop);
    m_effectsTree->setDragEnabled(true);
    m_effectsTree->setUniformRowHeights(false);
    loadEffects();
    connect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
}

void EffectStackView::loadEffects(int start, int end)
{
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
        QImage effectIcon = m_thumbnailer->requestImage(effectModel->getAssetId(), &size, QSize(QStyle::PM_SmallIconSize,QStyle::PM_SmallIconSize));
        CollapsibleEffectView *view = new CollapsibleEffectView(effectModel, effectIcon, this);
        qDebug()<<"__ADDING EFFECT: "<<effectModel->filter().get("id")<<", ACT: "<<active;
        if (i == active) {
            view->slotActivateEffect(m_model->getIndexFromItem(effectModel));
        }
        view->buttonUp->setEnabled(i > 0);
        view->buttonDown->setEnabled(i < max - 1);
        connect(view, &CollapsibleEffectView::deleteEffect, m_model.get(), &EffectStackModel::removeEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        connect(view, &CollapsibleEffectView::switchHeight, this, &EffectStackView::slotAdjustDelegate);
        connect(view, &CollapsibleEffectView::startDrag, this, &EffectStackView::slotStartDrag);
        connect(view, &CollapsibleEffectView::createGroup, m_model.get(), &EffectStackModel::slotCreateGroup);
        connect(view, &CollapsibleEffectView::activateEffect, this, &EffectStackView::slotActivateEffect);
        connect(this, &EffectStackView::doActivateEffect, view, &CollapsibleEffectView::slotActivateEffect);
        QModelIndex ix = m_model->getIndexFromItem(effectModel);
        m_effectsTree->setIndexWidget(ix, view);

    }
}

void EffectStackView::slotActivateEffect(std::shared_ptr<EffectItemModel> effectModel)
{
    m_model->setActiveEffect(effectModel->row());
    QModelIndex activeIx = m_model->getIndexFromItem(effectModel);
    emit doActivateEffect(activeIx);
}

void EffectStackView::slotStartDrag(QPixmap pix, std::shared_ptr<EffectItemModel> effectModel)
{
    auto *drag = new QDrag(this);
    drag->setPixmap(pix);
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/effect"), effectModel->getAssetId().toUtf8());
    mime->setData(QStringLiteral("kdenlive/effectsource"), QString::number(effectModel->getParentId()).toUtf8());
    mime->setData(QStringLiteral("kdenlive/effectrow"), QString::number(effectModel->row()).toUtf8());

    // Assign ownership of the QMimeData object to the QDrag object.
    drag->setMimeData(mime);
    // Start the drag and drop operation
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void EffectStackView::slotAdjustDelegate(std::shared_ptr<EffectItemModel> effectModel, int height)
{
    QModelIndex ix = m_model->getIndexFromItem(effectModel);
    WidgetDelegate *del = static_cast <WidgetDelegate *>(m_effectsTree->itemDelegate(ix));
    del->setHeight(ix, height);
}

void EffectStackView::refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    loadEffects(topLeft.row(), bottomRight.row() + 1);
}

void EffectStackView::unsetModel(bool reset)
{
    // Release ownership of smart pointer
    if (reset) {
        m_model.reset();
    }
}

