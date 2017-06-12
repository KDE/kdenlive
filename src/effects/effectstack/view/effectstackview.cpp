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

EffectStackView::EffectStackView(QWidget *parent) : QWidget(parent)
    , m_thumbnailer(new AssetIconProvider(true))
{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(2);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setAcceptDrops(true);
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
    int max = m_model->rowCount();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<EffectItemModel> effectModel = m_model->getEffect(i);
        QSize size;
        QImage effectIcon = m_thumbnailer->requestImage(effectModel->getAssetId(), &size, QSize(QStyle::PM_SmallIconSize,QStyle::PM_SmallIconSize));
        CollapsibleEffectView *view = new CollapsibleEffectView(effectModel, effectIcon, this);
        connect(view, &CollapsibleEffectView::deleteEffect, m_model.get(), &EffectStackModel::removeEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        m_lay->addWidget(view);
        m_widgets.push_back(view);
    }
    connect(m_model.get(), &EffectStackModel::dataChanged, this, &EffectStackView::refresh);
    m_lay->addStretch();
}

void EffectStackView::refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    unsetModel(false);
    int max = m_model->rowCount();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<EffectItemModel> effectModel = m_model->getEffect(i);
        QSize size;
        QImage effectIcon = m_thumbnailer->requestImage(effectModel->getAssetId(), &size, QSize(QStyle::PM_SmallIconSize,QStyle::PM_SmallIconSize));
        CollapsibleEffectView *view = new CollapsibleEffectView(effectModel, effectIcon, this);
        connect(view, &CollapsibleEffectView::deleteEffect, m_model.get(), &EffectStackModel::removeEffect);
        connect(view, &CollapsibleEffectView::moveEffect, m_model.get(), &EffectStackModel::moveEffect);
        m_lay->addWidget(view);
        m_widgets.push_back(view);
    }
    m_lay->addStretch();
}

void EffectStackView::unsetModel(bool reset)
{
    // clear layout
    m_widgets.clear();
    QLayoutItem *child;
    while ((child = m_lay->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child->spacerItem();
    }

    // Release ownership of smart pointer
    if (reset) {
        m_model.reset();
    }
}


