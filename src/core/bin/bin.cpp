/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "bin.h"
#include "mainwindow.h"
#include "project/binmodel.h"
#include "project/timeline.h"
#include "timelineview/timelinewidget.h"
#include "projectitemmodel.h"
#include "kdenlivesettings.h"
#include <unistd.h>
#include "project/abstractprojectclip.h"
#include "project/project.h"
#include "monitor/monitorview.h"
#include "project/projectmanager.h"
#include "project/clippluginmanager.h"

#include "producersystem/producerdescription.h"
#include "producersystem/producer.h"
#include "project/producerwrapper.h"
#include "producersystem/producerrepository.h"

#include "core.h"
#include <QVBoxLayout>
#include <QEvent>
#include <QSplitter>
#include <QMouseEvent>
#include <QTreeView>
#include <QListView>
#include <QHeaderView>
#include <QTimer>
#include <KToolBar>
#include <KSelectAction>
#include <KDialog>
#include <KApplication>
#include <KLocale>
#include <KDebug>
#include <KColorScheme>
#include <KColorUtils>


EventEater::EventEater(QObject *parent) : QObject(parent)
{
}

bool EventEater::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        emit focusClipMonitor();
        return QObject::eventFilter(obj, event);
    }
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
                QModelIndex idx = view->indexAt(mouseEvent->pos());
                AbstractProjectClip *currentItem = static_cast<AbstractProjectClip *>(idx.internalPointer());
                if (mouseEvent->modifiers() & Qt::ShiftModifier)
                    emit editItem(currentItem->clipId());
                else
                    emit editItemInTimeline(currentItem->clipId(), currentItem->name(), currentItem->baseProducer());
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
    QWidget(parent)
  , m_itemModel(NULL)
  , m_itemView(NULL)
  , m_listType(BinTreeView)
  , m_iconSize(60)
  , m_propertiesPanel(NULL)
  , m_editedProducer(NULL)
{
    // TODO: proper ui, search line, add menu, ...
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_toolbar = new KToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconDimensions(style()->pixelMetric(QStyle::PM_SmallIconSize));
    layout->addWidget(m_toolbar);

    QSlider *slider = new QSlider(Qt::Horizontal, this);
    slider->setMaximumWidth(100);
    slider->setRange(0, 80);
    slider->setValue(m_iconSize / 2);
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(slotSetIconSize(int)));
    m_toolbar->addWidget(slider);
    

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
    connect(listType, SIGNAL(triggered(QAction*)), this, SLOT(slotInitView(QAction*)));
    m_toolbar->addAction(listType);
    m_eventEater = new EventEater(this);
    connect(m_eventEater, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    m_binTreeViewDelegate = new ItemDelegate(m_itemView);
    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
    m_splitter = new QSplitter(this);
    m_headerInfo = KdenliveSettings::treeViewHeaders().toUtf8 ();
    layout->addWidget(m_splitter);
}

Bin::~Bin()	
{
    //TODO: Does not work, header->savestate always returns empty string, why?
    if (m_itemView && m_listType == BinTreeView) {
        // save current treeview state (column width)
        QTreeView *view = static_cast<QTreeView*>(m_itemView);
        m_headerInfo = view->header()->saveState();
        KdenliveSettings::setTreeViewHeaders(QString(m_headerInfo));
    }
}

void Bin::slotAddClip()
{
    pCore->clipPluginManager()->execAddClipDialog();
}

void Bin::setActionMenus(QMenu *producerMenu)
{
    QToolButton *generatorMenu = new QToolButton(this);
    KAction *a = new KAction(KIcon("kdenlive-add-clip"), i18n("Add Clip"), this);
    connect(a, SIGNAL(triggered(bool)), this, SLOT(slotAddClip()));
    generatorMenu->setDefaultAction(a);
    generatorMenu->setMenu(producerMenu);
    generatorMenu->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addSeparator();
    m_toolbar->addWidget(generatorMenu);
}

void Bin::setProject(Project* project)
{
    closeEditing();
    delete m_itemView;
    m_itemView = NULL;
    delete m_itemModel;

    m_itemModel = new ProjectItemModel(project->bin(), this);
    m_itemModel->setIconSize(m_iconSize);
    connect(m_itemModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_itemModel, SIGNAL(selectModel(QModelIndex)), this, SLOT(selectModel(QModelIndex)));
    connect(m_itemModel, SIGNAL(markersNeedUpdate(QString,QList<int>)), this, SLOT(slotMarkersNeedUpdate(QString,QList<int>)));
    connect(m_eventEater, SIGNAL(focusClipMonitor()), project->bin()->monitor(), SLOT(slotFocusClipMonitor()), Qt::UniqueConnection);
    connect(m_eventEater, SIGNAL(editItemInTimeline(QString,QString,ProducerWrapper*)), this, SLOT(slotOpenClipTimeline(QString,QString,ProducerWrapper*)), Qt::UniqueConnection);
    connect(m_eventEater, SIGNAL(editItem(QString)), this, SLOT(showClipProperties(QString)), Qt::UniqueConnection);
    //connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), m_itemView
    //connect(m_itemModel, SIGNAL(updateCurrentItem()), this, SLOT(autoSelect()));
    
    slotInitView(NULL);
    autoSelect();
}

void Bin::rowsInserted(const QModelIndex &/*parent*/, int /*start*/, int end)
{
    const QModelIndex id = m_itemModel->index(end, 0, QModelIndex());
    selectModel(id);
}

void Bin::selectModel(const QModelIndex &id)
{
    m_itemView->setCurrentIndex(id);
    if (id.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(id.internalPointer());
        currentItem->setCurrent(true);
    }
    else {
        kDebug()<<" * * * * *CURRENT IS INVALID!!!";
    }
}

void Bin::autoSelect()
{
    QModelIndex parent2 = m_itemModel->selectionModel()->currentIndex();
    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(parent2.internalPointer());
    if (!currentItem) {
        QModelIndex id = m_itemModel->index(0, 0, QModelIndex());
        selectModel(id);
    }
}

void Bin::slotInitView(QAction *action)
{
    closeEditing();
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
        m_itemView = new QListView(m_splitter);
        static_cast<QListView*>(m_itemView)->setViewMode(QListView::IconMode);
        break;
    default:
        m_itemView = new QTreeView(m_splitter);
        break;
    }
    m_itemView->setMouseTracking(true);
    m_itemView->viewport()->installEventFilter(m_eventEater);
    m_itemView->setIconSize(QSize(m_iconSize, m_iconSize));
    m_itemView->setModel(m_itemModel);
    m_itemView->setSelectionModel(m_itemModel->selectionModel());

    m_itemView->setDragDropMode(QAbstractItemView::DragDrop);
    m_splitter->addWidget(m_itemView);

    // setup some default view specific parameters
    if (m_listType == BinTreeView) {
        m_itemView->setItemDelegate(m_binTreeViewDelegate);
        QTreeView *view = static_cast<QTreeView*>(m_itemView);
        kDebug()<<"/ / / BUILD HEADERS: "<<m_headerInfo;
        if (!m_headerInfo.isEmpty())
            view->header()->restoreState(m_headerInfo);
        else
            view->header()->resizeSections(QHeaderView::ResizeToContents);
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


void Bin::slotOpenClipTimeline(const QString &id, const QString &name, ProducerWrapper *prod)
{
    TimelineWidget *tml = pCore->window()->addTimeline(id, name);
    if (tml == NULL)
        return;
    Timeline *tl = new Timeline(prod, pCore->projectManager()->current());
    tl->loadClip();
    tml->setClipTimeline(tl);
}

void Bin::slotMarkersNeedUpdate(const QString &id, const QList<int> &markers)
{
    // Check if we have a clip timeline that needs update
    TimelineWidget *tml = pCore->window()->getTimeline(id);
    if (tml) {
        tml->updateMarkers(markers);
    }
    // Update clip monitor
}

void Bin::closeEditing()
{
    if (!m_editedProducer)
        return;
    delete m_editedProducer;
    delete m_propertiesPanel;
    m_editedProducer = NULL;
    m_propertiesPanel = NULL;
}

void Bin::showClipProperties(const QString &id)
{
    closeEditing();
    ProducerWrapper *producer = pCore->projectManager()->current()->bin()->clipProducer(id);
    if (!producer) {
        // Cannot find producer to edit
        return;
    }
    ProducerDescription *desc = pCore->producerRepository()->producerDescription(producer->get("mlt_service"));
    if (!desc || desc->parameters().count() == 0) {
        // Don't show properties panel of producer has no parameter
        return;
    }
    Mlt::Properties props(producer->get_properties());
    pCore->clipPluginManager()->filterDescription(props, desc);
    m_editedProducer= new Producer(producer, desc);
    connect(m_editedProducer, SIGNAL(updateClip()), this, SLOT(refreshEditedClip()));
    connect(m_editedProducer, SIGNAL(editingDone()), this, SLOT(closeEditing()));

    // Build edit widget
    m_propertiesPanel = new QWidget(m_splitter);
    QVBoxLayout *l = new QVBoxLayout;
    m_propertiesPanel->setLayout(l);
    m_propertiesPanel->setStyleSheet(getStyleSheet());
    m_editedProducer->createView(id, m_propertiesPanel);
    //m_propertiesPanel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_splitter->addWidget(m_propertiesPanel);
    //TODO: setting stretch does not work, try to fix...
    m_splitter->setStretchFactor(m_splitter->indexOf(m_itemView), 1);
    m_splitter->setStretchFactor(m_splitter->indexOf(m_propertiesPanel), 20);
}

const QString Bin::getStyleSheet()
{
    KColorScheme scheme(QApplication::palette().currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor selected_bg = scheme.decoration(KColorScheme::FocusColor).color();
    QColor hgh = KColorUtils::mix(QApplication::palette().window().color(), selected_bg, 0.2);
    QColor hover_bg = scheme.decoration(KColorScheme::HoverColor).color();
    QColor light_bg = scheme.shade(KColorScheme::LightShade);
    QColor alt_bg = scheme.background(KColorScheme::NormalBackground).color();

    QString stylesheet;

    // effect background
    stylesheet.append(QString("QFrame#decoframe {border-top-left-radius:5px;border-top-right-radius:5px;border-bottom:2px solid palette(mid);border-top:1px solid palette(light);} QFrame#decoframe[active=\"true\"] {background: %1;}").arg(hgh.name()));

    // effect in group background
    stylesheet.append(QString("QFrame#decoframesub {border-top:1px solid palette(light);}  QFrame#decoframesub[active=\"true\"] {background: %1;}").arg(hgh.name()));

    // group background
    stylesheet.append(QString("QFrame#decoframegroup {border-top-left-radius:5px;border-top-right-radius:5px;border:2px solid palette(dark);margin:0px;margin-top:2px;} "));

    // effect title bar
    stylesheet.append(QString("QFrame#frame {margin-bottom:2px;border-top-left-radius:5px;border-top-right-radius:5px;}  QFrame#frame[target=\"true\"] {background: palette(highlight);}"));

    // group effect title bar
    stylesheet.append(QString("QFrame#framegroup {border-top-left-radius:2px;border-top-right-radius:2px;background: palette(dark);}  QFrame#framegroup[target=\"true\"] {background: palette(highlight);} "));

    // draggable effect bar content
    stylesheet.append(QString("QProgressBar::chunk:horizontal {background: palette(button);border-top-left-radius: 4px;border-bottom-left-radius: 4px;} QProgressBar::chunk:horizontal#dragOnly {background: %1;border-top-left-radius: 4px;border-bottom-left-radius: 4px;} QProgressBar::chunk:horizontal:hover {background: %2;}").arg(alt_bg.name()).arg(selected_bg.name()));

    // draggable effect bar
    stylesheet.append(QString("QProgressBar:horizontal {border: 1px solid palette(dark);border-top-left-radius: 4px;border-bottom-left-radius: 4px;border-right:0px;background:%3;padding: 0px;text-align:left center} QProgressBar:horizontal:disabled {border: 1px solid palette(button)} QProgressBar:horizontal#dragOnly {background: %3} QProgressBar:horizontal[inTimeline=\"true\"] { border: 1px solid %1;border-right: 0px;background: %2;padding: 0px;text-align:left center } QProgressBar::chunk:horizontal[inTimeline=\"true\"] {background: %1;}").arg(hover_bg.name()).arg(light_bg.name()).arg(alt_bg.name()));

    // spin box for draggable widget
    stylesheet.append(QString("QAbstractSpinBox#dragBox {border: 1px solid palette(dark);border-top-right-radius: 4px;border-bottom-right-radius: 4px;padding-right:0px;} QAbstractSpinBox::down-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox:disabled#dragBox {border: 1px solid palette(button);} QAbstractSpinBox::up-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox[inTimeline=\"true\"]#dragBox { border: 1px solid %1;} QAbstractSpinBox:hover#dragBox {border: 1px solid %2;} ").arg(hover_bg.name()).arg(selected_bg.name()));

    // group editable labels
    stylesheet.append(QString("MyEditableLabel { background-color: transparent; color: palette(bright-text); border-radius: 2px;border: 1px solid transparent;} MyEditableLabel:hover {border: 1px solid palette(highlight);} "));

    return stylesheet;
}

void Bin::refreshEditedClip()
{
    const QString id = m_propertiesPanel->property("clipId").toString();
    pCore->projectManager()->current()->bin()->refreshThumnbail(id);
    pCore->projectManager()->current()->binMonitor()->refresh();
}

#include "bin.moc"
