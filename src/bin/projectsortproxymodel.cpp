/*
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "projectsortproxymodel.h"

#include <QItemSelectionModel>


ProjectSortProxyModel::ProjectSortProxyModel(QObject *parent)
     : QSortFilterProxyModel(parent)
{
    m_selection = new QItemSelectionModel(this);
    connect(m_selection, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(onCurrentRowChanged(QItemSelection,QItemSelection)));
}

bool ProjectSortProxyModel::filterAcceptsRow(int sourceRow,
         const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    return (sourceModel()->data(index0).toString().contains(m_searchString));
}

QItemSelectionModel* ProjectSortProxyModel::selectionModel()
{
    return m_selection;
}

void ProjectSortProxyModel::slotSetSearchString(const QString &str)
{ 
    m_searchString = str; 
    invalidateFilter(); 
}

void ProjectSortProxyModel::onCurrentRowChanged(const QItemSelection& current, const QItemSelection& previous)
{
    Q_UNUSED(previous)
    QModelIndex id;
    if (!current.isEmpty()) id = current.indexes().first();
    emit selectModel(id);
}

void ProjectSortProxyModel::slotDataChanged(const QModelIndex &ix1, const QModelIndex &ix2)
{
    emit dataChanged(ix1, ix2);
}

