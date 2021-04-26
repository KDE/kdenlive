/*
 * Kdenlive TitleClip Pattern
 * Copyright (C) 2020  Rafał Lalik <rafallalik@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "patternsmodel.h"
#include "titledocument.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>


PatternsModel::PatternsModel(QObject *parent) : QAbstractListModel(parent)
{
}

QVariant PatternsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DecorationRole)
        return pixmaps[index.row()].scaled(m_tileSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else if (role == Qt::SizeHintRole)
        return m_tileSize;
    else if (role == Qt::UserRole)
        return patterns.value(index.row());

    return QVariant();
}

int PatternsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : patterns.size();
}

void PatternsModel::addScene(const QString & pattern)
{
    int row = patterns.size();
    beginInsertRows(QModelIndex(), row, row);

    patterns.append(pattern);
    pixmaps.append(paintScene(pattern));
    ++modified_counter;

    endInsertRows();
}

QPixmap PatternsModel::paintScene(const QString & pattern)
{
    QDomDocument doc;
    doc.setContent(pattern);

    QList<QGraphicsItem *> items;
    int width, height, duration, missing;
    TitleDocument::loadFromXml(doc, items, width, height, nullptr, nullptr, nullptr, &duration, missing);

    QGraphicsScene scene(0, 0, width, height);

    if (bkg) {
        auto * bkg_frame = new QGraphicsPixmapItem();
        bkg_frame->setTransform(bkg->transform());
        bkg_frame->setZValue(bkg->zValue());
        bkg_frame->setPixmap(bkg->pixmap());
        scene.addItem(bkg_frame);
    }
    for (QGraphicsItem *item : qAsConst(items)) {
        scene.addItem(item);
    }

    QPixmap pxm(width, height);
    QPainter painter(&pxm);
    painter.setRenderHint(QPainter::Antialiasing);
    scene.render(&painter);

    return pxm;
}

void PatternsModel::repaintScenes()
{
    for (int i = 0; i < patterns.size(); ++i) {
        pixmaps[i] = paintScene(patterns[i]);
    }

    emit dataChanged(index(0), index(patterns.size()-1));
}

void PatternsModel::removeScene(const QModelIndex & index)
{
    beginRemoveRows(QModelIndex(), index.row(), index.row());

    patterns.remove(index.row());
    pixmaps.remove(index.row());

    ++modified_counter;

    endRemoveRows();
}

void PatternsModel::removeAll()
{
    beginRemoveRows(QModelIndex(), 0, patterns.size()-1);

    patterns.clear();
    pixmaps.clear();

    ++modified_counter;

    endRemoveRows();
}

QByteArray PatternsModel::serialize()
{
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    for (const auto &p : qAsConst(patterns)) {
        stream << p;
    }
    modified_counter = 0;

    return encodedData;
}

void PatternsModel::deserialize(const QByteArray& data)
{
    removeAll();

    QByteArray ba = data;
    QDataStream stream(&ba, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        QString p;
        stream >> p;
        addScene(p);
    }
    modified_counter = 0;
}
