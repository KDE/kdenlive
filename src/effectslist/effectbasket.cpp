/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "effectbasket.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include <klocalizedstring.h>

#include <QListWidget>
#include <QMimeData>

EffectBasket::EffectBasket(QWidget *parent)
    : QListWidget(parent)
{
    setFrameStyle(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    slotReloadBasket();
    connect(this, &QListWidget::itemActivated, this, &EffectBasket::slotAddEffect);
}

void EffectBasket::slotReloadBasket()
{
    clear();
    for (const QString &effectId : KdenliveSettings::favorite_effects()) {
        if (EffectsRepository::get()->exists(effectId)) {
            QListWidgetItem *it = new QListWidgetItem(EffectsRepository::get()->getName(effectId));
            it->setData(Qt::UserRole, effectId);
            addItem(it);
        }
    }
    sortItems();
}

QMimeData *EffectBasket::mimeData(const QList<QListWidgetItem *> list) const
{
    if (list.isEmpty()) {
        return new QMimeData;
    }
    QDomDocument doc;
    QListWidgetItem *item = list.at(0);
    QString effectId = item->data(Qt::UserRole).toString();
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/effect"), effectId.toUtf8());
    return mime;
}

void EffectBasket::showEvent(QShowEvent *event)
{
    QListWidget::showEvent(event);
    if (!currentItem()) {
        setCurrentRow(0);
    }
}

void EffectBasket::slotAddEffect(QListWidgetItem *item)
{
    QString assetId = item->data(Qt::UserRole).toString();
    QVariantMap mimeData;
    mimeData.insert(QStringLiteral("kdenlive/effect"), assetId);
    emit activateAsset(mimeData);
}
