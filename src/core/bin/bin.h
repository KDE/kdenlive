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

public:
    Bin(QWidget* parent = 0);
    ~Bin();

public slots:
    void setProject(Project *project);


private:
    ProjectItemModel *m_itemModel;
    QAbstractItemView *m_itemView;
};

#endif
