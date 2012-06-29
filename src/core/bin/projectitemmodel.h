/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PROJECTITEMMODEL_H
#define PROJECTITEMMODEL_H

#include <QAbstractItemModel>

class BinModel;
class AbstractProjectItem;
class QItemSelectionModel;

/**
 * @class ProjectItemModel
 * @brief Acts as an adaptor to be able to use BinModel with item views.
 */

class ProjectItemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ProjectItemModel(BinModel *binModel, QObject* parent = 0);
    ~ProjectItemModel();

    QItemSelectionModel *selectionModel();

    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

private slots:
    void onCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);

    void onAboutToAddItem(AbstractProjectItem *item);
    void onItemAdded(AbstractProjectItem *item);
    void onAboutToRemoveItem(AbstractProjectItem *item);
    void onItemRemoved(AbstractProjectItem *item);

private:
    BinModel *m_binModel;
    QItemSelectionModel *m_selection;
};

#endif
