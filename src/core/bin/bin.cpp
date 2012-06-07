/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "bin.h"
#include "projectitemmodel.h"
#include "project/project.h"
#include "project/projectmanager.h"
#include "core.h"
#include <QVBoxLayout>
#include <QTreeView>


Bin::Bin(QWidget* parent) :
    QWidget(parent),
    m_itemModel(NULL),
    m_itemView(NULL)
{
    // TODO: proper ui, search line, add menu, ...
    QVBoxLayout *layout = new QVBoxLayout(this);

    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));

}

Bin::~Bin()
{
    if (m_itemView) {
        delete m_itemView;
    }
    if (m_itemModel) {
        delete m_itemModel;
    }
}

void Bin::setProject(Project* project)
{
    if (m_itemView) {
        delete m_itemView;
    }
    if (m_itemModel) {
        delete m_itemModel;
    }

    m_itemModel = new ProjectItemModel(project, this);

    m_itemView = new QTreeView();
    layout()->addWidget(m_itemView);
    m_itemView->setModel(m_itemModel);
    m_itemView->setSelectionModel(m_itemModel->selectionModel());
}

#include "bin.moc"
