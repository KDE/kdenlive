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

WidgetDelegate::WidgetDelegate(QObject *parent) :
    QItemDelegate(parent)
{
}

QSize WidgetDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize s = QItemDelegate::sizeHint(option, index);
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
        event->setDropAction(Qt::CopyAction);
        event->setAccepted(true);
    } else {
        event->setAccepted(false);
    }
}

void EffectStackView::dropEvent(QDropEvent *event)
{
    event->accept();
    QString effectId = event->mimeData()->data(QStringLiteral("kdenlive/effect"));
    m_model->appendEffect(effectId, property("clipId").toInt());
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
    for (int i = 0; i < end; i++) {
        std::shared_ptr<EffectItemModel> effectModel = m_model->getEffect(i);
        QSize size;
        QImage effectIcon = m_thumbnailer->requestImage(effectModel->getAssetId(), &size, QSize(QStyle::PM_SmallIconSize,QStyle::PM_SmallIconSize));
        CollapsibleEffectView *view = new CollapsibleEffectView(effectModel, effectIcon, this);
        view->buttonUp->setEnabled(i > 0);
        view->buttonDown->setEnabled(i < max - 1);
        connect(view, &CollapsibleEffectView::deleteEffect, m_model.get(), &EffectStackModel::removeEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        connect(view, &CollapsibleEffectView::switchHeight, this, &EffectStackView::slotAdjustDelegate);
        QModelIndex ix = m_model->getIndexFromItem(effectModel);
        m_effectsTree->setIndexWidget(ix, view);

    }
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

void EffectStackView::mousePressEvent(QMouseEvent *e)
{
    //m_dragPoint = e->globalPos();
    //e->accept();
    qDebug()<<"*** EFFECT STACK CLICK";
}
