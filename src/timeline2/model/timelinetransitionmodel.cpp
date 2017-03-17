/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
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


#include "timelinetransitionmodel.hpp"

#include "trackmodel.hpp"
#include "clipmodel.hpp"
#include "transitionmodel.hpp"
#include "groupsmodel.hpp"
#include "doc/docundostack.hpp"
#include <mlt++/MltTractor.h>
#include <mlt++/MltProfile.h>
#include <QDebug>

TimelineTransitionModel::TimelineTransitionModel(TimelineItemModel *model, QObject *parent) : QIdentityProxyModel(parent)
{
    setSourceModel(model);
}


QModelIndex TimelineTransitionModel::index(int row, int column, const QModelIndex &parent) const
{
    return sourceModel()->index(row, 1, parent);
}
 
// parent is at -1, -1. All others have a parent of -1, -1
QModelIndex TimelineTransitionModel::parent(const QModelIndex &index) const 
{
    return sourceModel()->parent(index);
    // we are at the top, return an invalid parent
    if (index.column() == -1 && index.row() == -1) return QModelIndex();
    else return createIndex(-1,-1);
}
 
QModelIndex TimelineTransitionModel::mapToSource(const QModelIndex &proxyIndex) const 
{
 
    // we use the same co-ords for now, so map to the same
    return sourceModel()->index(proxyIndex.row(),1);//proxyIndex.column());
}
 
    // source model should be the same as ours
QModelIndex TimelineTransitionModel::mapFromSource(const QModelIndex &sourceIndex) const 
{
        return index(sourceIndex.row(), sourceIndex.column());
}
 
    // same columns as source please
int TimelineTransitionModel::columnCount(const QModelIndex &parent) const 
{
    return 1;
}
 
int TimelineTransitionModel::rowCount(const QModelIndex &parent) const
{
    return sourceModel()->rowCount(sourceModel()->index(parent.row(), 1));
}

