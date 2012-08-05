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
#include <QListView>
#include <QHeaderView>
#include <KToolBar>
#include <KSelectAction>
#include <KLocale>


Bin::Bin(QWidget* parent) :
    QWidget(parent),
    m_itemModel(NULL),
    m_itemView(NULL),
    m_listType(BinTreeView),
    m_iconSize(60)
{
    // TODO: proper ui, search line, add menu, ...
    QVBoxLayout *layout = new QVBoxLayout(this);
    KToolBar *tb = new KToolBar(this);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setIconDimensions(style()->pixelMetric(QStyle::PM_SmallIconSize));
    layout->addWidget(tb);
    QSlider *sl = new QSlider(Qt::Horizontal, this);
    sl->setMaximumWidth(100);
    sl->setValue(m_iconSize / 2);
    connect(sl, SIGNAL(valueChanged(int)), this, SLOT(slotSetIconSize(int)));
    tb->addWidget(sl);
    KSelectAction *listType = new KSelectAction(KIcon("view-list-tree"), i18n("View Mode"), this);
    KAction *a = listType->addAction(KIcon("view-list-tree"), i18n("Tree View"));
    a->setData(BinTreeView);
    if (m_listType == a->data().toInt()) listType->setCurrentAction(a);
    a = listType->addAction(KIcon("view-list-icons"), i18n("Icon View"));
    a->setData(BinIconView);
    if (m_listType == a->data().toInt()) listType->setCurrentAction(a);
    listType->setToolBarMode(KSelectAction::MenuMode);
    connect(listType, SIGNAL(triggered(QAction*)), this, SLOT(slotInitView(QAction *)));
    tb->addAction(listType);
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

    m_itemModel = new ProjectItemModel(project->bin(), this);
    slotInitView(NULL);
}

void Bin::slotInitView(QAction *action)
{
    if (action) {
	int viewType = action->data().toInt();
	if (viewType == m_listType) return;
	if (m_listType == BinTreeView) {
	    // save current treeview state (column width)
	    QTreeView *view = static_cast<QTreeView*>(m_itemView);
	    m_headerInfo = view->header()->saveState();
	}
	m_listType = (BinViewType) viewType;
    }
    if (m_itemView) {
        delete m_itemView;
    }
    switch (m_listType) {
      case BinIconView:
	  m_itemView = new QListView();
	  static_cast<QListView*>(m_itemView)->setViewMode(QListView::IconMode);
	  break;
      default:
	  m_itemView = new QTreeView();
	  break;
    }

    layout()->addWidget(m_itemView);
    m_itemView->setIconSize(QSize(m_iconSize, m_iconSize));
    m_itemView->setModel(m_itemModel);
    m_itemView->setSelectionModel(m_itemModel->selectionModel());

    // setup some default view specific parameters
    if (m_listType == BinTreeView) {
	QTreeView *view = static_cast<QTreeView*>(m_itemView);
	if (!m_headerInfo.isEmpty())
	    view->header()->restoreState(m_headerInfo);
    }
}

void Bin::slotSetIconSize(int size)
{
    if (!m_itemView) return;
    m_iconSize = size * 2;
    m_itemView->setIconSize(QSize(m_iconSize, m_iconSize));
}

#include "bin.moc"
