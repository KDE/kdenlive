/***************************************************************************
 *   SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   SPDX-License-Identifier: GPL-2.0-or-later
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
