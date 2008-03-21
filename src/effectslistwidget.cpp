/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "QApplication"
#include "QMouseEvent"

#include "KDebug"

#include "effectslistwidget.h"
#include "effectslist.h"

#define EFFECT_VIDEO 1
#define EFFECT_AUDIO 2
#define EFFECT_CUSTOM 3

EffectsListWidget::EffectsListWidget(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent)
        : KListWidget(parent), m_audioList(audioEffectList), m_videoList(videoEffectList), m_customList(customEffectList) {
    //setSelectionMode(QAbstractItemView::ExtendedSelection);
    //setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(true);
    setAlternatingRowColors(true);
    setSortingEnabled(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    initList();
}

EffectsListWidget::~EffectsListWidget() {
}

void EffectsListWidget::initList() {
    clear();
    QStringList names = m_videoList->effectNames();
    QListWidgetItem *item;
    foreach(QString str, names) {
        item = new QListWidgetItem(str, this);
        item->setData(Qt::UserRole, QString::number((int) EFFECT_VIDEO));
    }

    names = m_audioList->effectNames();
    foreach(QString str, names) {
        item = new QListWidgetItem(str, this);
        item->setData(Qt::UserRole, QString::number((int) EFFECT_AUDIO));
    }

    names = m_customList->effectNames();
    foreach(QString str, names) {
        item = new QListWidgetItem(str, this);
        item->setData(Qt::UserRole, QString::number((int) EFFECT_CUSTOM));
    }
}

QDomElement EffectsListWidget::currentEffect() {
    return itemEffect(currentItem());
}

QDomElement EffectsListWidget::itemEffect(QListWidgetItem *item) {
    QDomElement effect;
    if (!item) return effect;
    switch (item->data(Qt::UserRole).toInt()) {
    case 1:
        effect = m_videoList->getEffectByName(item->text());
        break;
    case 2:
        effect = m_audioList->getEffectByName(item->text());
        break;
    default:
        effect = m_customList->getEffectByName(item->text());
        break;
    }
    return effect;
}


QString EffectsListWidget::currentInfo() {
    QListWidgetItem *item = currentItem();
    if (!item) return QString();
    QString info;
    switch (item->data(Qt::UserRole).toInt()) {
    case 1:
        info = m_videoList->getInfo(item->text());
        break;
    case 2:
        info = m_audioList->getInfo(item->text());
        break;
    default:
        info = m_customList->getInfo(item->text());
        break;
    }
    return info;
}

// virtual
void EffectsListWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        this->m_DragStartPosition = event->pos();
        m_dragStarted = true;
    }
    KListWidget::mousePressEvent(event);
}

// virtual
void EffectsListWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragStarted) return;
    if ((event->pos() - m_DragStartPosition).manhattanLength()
            < QApplication::startDragDistance())
        return;

    {
        QListWidgetItem *clickItem = itemAt(event->pos());
        if (clickItem) {
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData;
            QList <QListWidgetItem *> list;
            list = selectedItems();
            QDomDocument doc;
            foreach(QListWidgetItem *item, list) {
                doc.appendChild(doc.importNode(itemEffect(item), true));
            }
            QByteArray data;
            data.append(doc.toString().toUtf8());
            mimeData->setData("kdenlive/effectslist", data);
            drag->setMimeData(mimeData);
            //QPixmap pix = qVariantValue<QPixmap>(clickItem->data(Qt::DecorationRole));
            //drag->setPixmap(pix);
            //drag->setHotSpot(QPoint(0, 50));
            drag->start(Qt::MoveAction);
        }
        //event->accept();
    }
}

void EffectsListWidget::dragMoveEvent(QDragMoveEvent * event) {
    event->setDropAction(Qt::IgnoreAction);
    //if (item) {
    event->setDropAction(Qt::MoveAction);
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
    //}
}



#include "effectslistwidget.moc"
