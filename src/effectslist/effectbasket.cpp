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
#include "kdenlivesettings.h"
#include "effectslistwidget.h"
#include "effectslistview.h"

#include <klocalizedstring.h>

#include <QListWidget>
#include <QMimeData>
#include <QDomDocument>

EffectBasket::EffectBasket(EffectsListView *effectList) :
    QListWidget(effectList)
    , m_effectList(effectList)
{
    setFrameStyle(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    m_effectList->creatFavoriteBasket(this);
    connect(m_effectList, &EffectsListView::reloadBasket, this, &EffectBasket::slotReloadBasket);
    connect(this, &QListWidget::itemActivated, this, &EffectBasket::slotAddEffect);
}

void EffectBasket::slotReloadBasket()
{
    m_effectList->creatFavoriteBasket(this);
}

QMimeData *EffectBasket::mimeData(const QList<QListWidgetItem *> list) const
{
    if (list.isEmpty()) {
        return new QMimeData;
    }
    QDomDocument doc;
    QListWidgetItem *item = list.at(0);
    int type = item->data(EffectsListWidget::TypeRole).toInt();
    QStringList info = item->data(EffectsListWidget::IdRole).toStringList();
    QDomElement effect = EffectsListWidget::itemEffect(type, info);
    doc.appendChild(doc.importNode(effect, true));
    QMimeData *mime = new QMimeData;
    QByteArray data;
    data.append(doc.toString().toUtf8());
    mime->setData(QStringLiteral("kdenlive/effectslist"), data);
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
    int type = item->data(EffectsListWidget::TypeRole).toInt();
    QStringList info = item->data(EffectsListWidget::IdRole).toStringList();
    QDomElement effect = EffectsListWidget::itemEffect(type, info);
    emit addEffect(effect);
}

