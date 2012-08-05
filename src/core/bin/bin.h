/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef KDENLIVE_BIN_H
#define KDENLIVE_BIN_H

#include <QWidget>
#include <kdemacros.h>

class Project;
class QAbstractItemView;
class ProjectItemModel;


class KDE_EXPORT Bin : public QWidget
{
    Q_OBJECT

    /** @brief Defines the view types (icon view, tree view,...)  */
    enum BinViewType {BinTreeView, BinIconView };
      
public:
    Bin(QWidget* parent = 0);
    ~Bin();

public slots:
    void setProject(Project *project);

private slots:
    /** @brief Setup the bin view type (icon view, tree view, ...).
    * @param action The action whose data defines the view type or NULL to keep default view */
    void slotInitView(QAction *action);
    void slotSetIconSize(int size);

private:
    ProjectItemModel *m_itemModel;
    QAbstractItemView *m_itemView;
    /** @brief Default view type (icon, tree, ...) */
    BinViewType m_listType;
    /** @brief Default icon size for the views. */
    int m_iconSize;
    /** @brief Keeps the column width info of the tree view. */
    QByteArray m_headerInfo;
};

#endif
