/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "bin.h"
#include "project/binmodel.h"
#include "projectitemmodel.h"
#include "project/project.h"
#include "project/projectmanager.h"
#include "project/clippluginmanager.h"
#include "core.h"
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QTreeView>
#include <QListView>
#include <QHeaderView>
#include <QTimer>
#include <KToolBar>
#include <KSelectAction>
#include <KLocale>
#include <KDebug>


EventEater::EventEater(QObject *parent) : QObject(parent)
{
}

bool EventEater::eventFilter(QObject *obj, QEvent *event)
 {
     if (event->type() == QEvent::MouseButtonDblClick) {
	  QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
	  QAbstractItemView *view = qobject_cast<QAbstractItemView*>(obj->parent());
	  if (view) {
	      QModelIndex idx = view->indexAt(mouseEvent->pos());
	      if (idx == QModelIndex()) {
		  // User double clicked on empty area
		  emit addClip();
	      }
	      else {
		  // User double clicked on a clip
		  //TODO: show clip properties
	      }
	  }
	 else {
	    kDebug()<<" +++++++ NO VIEW-------!!";
	 }
         return true;
     } else {
         return QObject::eventFilter(obj, event);
     }
 }    


Bin::Bin(QWidget* parent) :
    QWidget(parent),
    m_itemModel(NULL),
    m_itemView(NULL),
    m_listType(BinTreeView),
    m_iconSize(60)
{
    // TODO: proper ui, search line, add menu, ...
    QVBoxLayout *layout = new QVBoxLayout(this);

    KToolBar *toolbar = new KToolBar(this);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconDimensions(style()->pixelMetric(QStyle::PM_SmallIconSize));
    layout->addWidget(toolbar);

    QSlider *slider = new QSlider(Qt::Horizontal, this);
    slider->setMaximumWidth(100);
    slider->setRange(0, 80);
    slider->setValue(m_iconSize / 2);
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(slotSetIconSize(int)));
    toolbar->addWidget(slider);
    

    KSelectAction *listType = new KSelectAction(KIcon("view-list-tree"), i18n("View Mode"), this);
    KAction *treeViewAction = listType->addAction(KIcon("view-list-tree"), i18n("Tree View"));
    treeViewAction->setData(BinTreeView);
    if (m_listType == treeViewAction->data().toInt()) {
        listType->setCurrentAction(treeViewAction);
    }
    KAction *iconViewAction = listType->addAction(KIcon("view-list-icons"), i18n("Icon View"));
    iconViewAction->setData(BinIconView);
    if (m_listType == iconViewAction->data().toInt()) {
        listType->setCurrentAction(iconViewAction);
    }
    listType->setToolBarMode(KSelectAction::MenuMode);
    connect(listType, SIGNAL(triggered(QAction*)), this, SLOT(slotInitView(QAction *)));
    toolbar->addAction(listType);
    m_eventEater = new EventEater(this);
    connect(m_eventEater, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    m_binTreeViewDelegate = new ItemDelegate(m_itemView);
    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
}

Bin::~Bin()	
{
}

void Bin::slotAddClip()
{
    pCore->clipPluginManager()->execAddClipDialog();
}

void Bin::setProject(Project* project)
{
    if (m_itemView) {
        delete m_itemView;
	m_itemView = NULL;
    }
    if (m_itemModel) {
        delete m_itemModel;
    }
    
    m_itemModel = new ProjectItemModel(project->bin(), this);
    m_itemModel->setIconSize(m_iconSize);
    connect(m_itemModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(rowsInserted(const QModelIndex &, int , int)));
    connect(m_itemModel, SIGNAL(selectModel(const QModelIndex&)), this, SLOT(selectModel(const QModelIndex&)));
    //connect(m_itemModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), m_itemView
    //connect(m_itemModel, SIGNAL(updateCurrentItem()), this, SLOT(autoSelect()));
    
    slotInitView(NULL);
}

void Bin::rowsInserted(const QModelIndex &parent, int start, int end)
{
    kDebug()<<" * * * * *RWS INST: "<<end;
    QModelIndex id = m_itemModel->index(end, 0, QModelIndex());
    selectModel(id);
    if (id.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(id.internalPointer());
        currentItem->setCurrent(true);
    }
    else {
	kDebug()<<" * * * * *CURRENT IS INVALID!!!";
    }
    
    /*QModelIndex id = m_itemModel->index(end, 0, QModelIndex());
    m_itemView->setCurrentIndex(id);
    /*AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(id.internalPointer());
    currentItem->setCurrent(true);*/
    
    /*QModelIndex parent2 = m_itemModel->selectionModel()->currentIndex();
    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(parent2.internalPointer());
    if (currentItem) currentItem->setCurrent(true);*/
}

void Bin::selectModel(const QModelIndex &parent)
{
    //QTimer::singleShot(100, this, SLOT(autoSelect()));
    kDebug()<<"// SELECTING ITEM: "<<parent.row();
    m_itemView->setCurrentIndex(parent);
}

void Bin::autoSelect()
{
    QModelIndex parent2 = m_itemModel->selectionModel()->currentIndex();
    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(parent2.internalPointer());
    currentItem->setCurrent(true);
}

void Bin::slotInitView(QAction *action)
{
    if (action) {
        int viewType = action->data().toInt();
        if (viewType == m_listType) {
            return;
        }
        if (m_listType == BinTreeView) {
            // save current treeview state (column width)
            QTreeView *view = static_cast<QTreeView*>(m_itemView);
            m_headerInfo = view->header()->saveState();
        }
        m_listType = static_cast<BinViewType>(viewType);
    }
    
    if (m_itemView) {
        delete m_itemView;
    }
    
    switch (m_listType) {
    case BinIconView:
        m_itemView = new QListView(this);
        static_cast<QListView*>(m_itemView)->setViewMode(QListView::IconMode);
        break;
    default:
        m_itemView = new QTreeView(this);
        break;
    }
    m_itemView->setMouseTracking(true);
    m_itemView->viewport()->installEventFilter(m_eventEater);
    m_itemView->setIconSize(QSize(m_iconSize, m_iconSize));
    m_itemView->setModel(m_itemModel);
    m_itemView->setSelectionModel(m_itemModel->selectionModel());

    m_itemView->setDragDropMode(QAbstractItemView::DragDrop);

    layout()->addWidget(m_itemView);

    // setup some default view specific parameters
    if (m_listType == BinTreeView) {
	m_itemView->setItemDelegate(m_binTreeViewDelegate);
        QTreeView *view = static_cast<QTreeView*>(m_itemView);
        if (!m_headerInfo.isEmpty())
            view->header()->restoreState(m_headerInfo);
    }
}

void Bin::slotSetIconSize(int size)
{
    if (!m_itemView) {
        return;
    }
    m_iconSize = size * 2;
    m_itemView->setIconSize(QSize(m_iconSize, m_iconSize));
    m_itemModel->setIconSize(m_iconSize);
}

#include "bin.moc"
