/*
Copyright (C) 2012  Till Theato <root@ttill.de>
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

#include "bin.h"
#include "mainwindow.h"
#include "projectitemmodel.h"
#include "projectclip.h"
#include "projectsubclip.h"
#include "projectfolder.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include "project/clipmanager.h"
#include "project/jobs/jobmanager.h"
#include "monitor/monitor.h"
#include "doc/kdenlivedoc.h"
#include "dialogs/clipcreationdialog.h"
#include "core.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/clippropertiescontroller.h"
#include "project/projectcommands.h"
#include "projectsortproxymodel.h"
#include "bincommands.h"
#include "mlt++/Mlt.h"

#include <KToolBar>
#include <KColorScheme>
#include <KMessageBox>
#include <KSplitterCollapserButton>


#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTimeLine>
#include <QSlider>
#include <QMenu>
#include <QDebug>
#include <QUndoCommand>


BinMessageWidget::BinMessageWidget(QWidget *parent) : KMessageWidget(parent) {}
BinMessageWidget::BinMessageWidget(const QString &text, QWidget *parent) : KMessageWidget(text, parent) {}


bool BinMessageWidget::event(QEvent* ev) {
    if (ev->type() == QEvent::Hide || ev->type() == QEvent::Close) emit messageClosing();
    return KMessageWidget::event(ev);
}

SmallJobLabel::SmallJobLabel(QWidget *parent) : QPushButton(parent)
{
    setFixedWidth(0);
    setFlat(true);
    m_timeLine = new QTimeLine(500, this);
    QObject::connect(m_timeLine, SIGNAL(valueChanged(qreal)), this, SLOT(slotTimeLineChanged(qreal)));
    QObject::connect(m_timeLine, SIGNAL(finished()), this, SLOT(slotTimeLineFinished()));
    hide();
}

const QString SmallJobLabel::getStyleSheet(const QPalette &p)
{
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor bg = scheme.background(KColorScheme::LinkBackground).color();
    QColor fg = scheme.foreground(KColorScheme::LinkText).color();
    QString style = QString("QPushButton {margin:3px;padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}").arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(fg.red()).arg(fg.green()).arg(fg.blue());
    
    bg = scheme.background(KColorScheme::ActiveBackground).color();
    fg = scheme.foreground(KColorScheme::ActiveText).color();
    style.append(QString("\nQPushButton:hover {margin:3px;padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}").arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(fg.red()).arg(fg.green()).arg(fg.blue()));
    
    return style;
}

void SmallJobLabel::setAction(QAction *action)
{
    m_action = action;
}

void SmallJobLabel::slotTimeLineChanged(qreal value)
{
    setFixedWidth(qMin(value * 2, qreal(1.0)) * sizeHint().width());
    update();
}

void SmallJobLabel::slotTimeLineFinished()
{
    if (m_timeLine->direction() == QTimeLine::Forward) {
        // Show
        m_action->setVisible(true);
    } else {
        // Hide
        m_action->setVisible(false);
        setText(QString());
    }
}

void SmallJobLabel::slotSetJobCount(int jobCount)
{
    if (jobCount > 0) {
        // prepare animation
        setText(i18np("%1 job", "%1 jobs", jobCount));
        setToolTip(i18np("%1 pending job", "%1 pending jobs", jobCount));
        
        //if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
        if (style()->styleHint(QStyle::SH_Widget_Animate, 0, this)) {
            setFixedWidth(sizeHint().width());
            m_action->setVisible(true);
            return;
        }
        
        if (m_action->isVisible()) {
            setFixedWidth(sizeHint().width());
            update();
            return;
        }
        
        setFixedWidth(0);
        m_action->setVisible(true);
        int wantedWidth = sizeHint().width();
        setGeometry(-wantedWidth, 0, wantedWidth, height());
        m_timeLine->setDirection(QTimeLine::Forward);
        if (m_timeLine->state() == QTimeLine::NotRunning) {
            m_timeLine->start();
        }
    }
    else {
        //if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
        if (style()->styleHint(QStyle::SH_Widget_Animate, 0, this)) {
            setFixedWidth(0);
            m_action->setVisible(false);
            return;
        }
        // hide
        m_timeLine->setDirection(QTimeLine::Backward);
        if (m_timeLine->state() == QTimeLine::NotRunning) {
            m_timeLine->start();
        }
    }
}

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
		//emit editItem(idx);
                return QObject::eventFilter(obj, event);
            }
        }
        else {
            qDebug()<<" +++++++ NO VIEW-------!!";
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
  , m_jobManager(NULL)
  , m_rootFolder(NULL)
  , m_doc(NULL)
  , m_iconSize(160, 90)
  , m_propertiesPanel(NULL)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create toolbar for buttons
    m_toolbar = new KToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconDimensions(style()->pixelMetric(QStyle::PM_SmallIconSize));
    layout->addWidget(m_toolbar);

    // Search line
    m_proxyModel = new ProjectSortProxyModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    QLineEdit *searchLine = new QLineEdit(this);
    searchLine->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    searchLine->setClearButtonEnabled(true);
    connect(searchLine, SIGNAL(textChanged(const QString &)), m_proxyModel, SLOT(slotSetSearchString(const QString &)));
    m_toolbar->addWidget(searchLine);

    // small info button for pending jobs
    m_infoLabel = new SmallJobLabel(this);
    m_infoLabel->setStyleSheet(SmallJobLabel::getStyleSheet(palette()));
    QAction *infoAction = m_toolbar->addWidget(m_infoLabel);
    m_jobsMenu = new QMenu(this);
    connect(m_jobsMenu, SIGNAL(aboutToShow()), this, SLOT(slotPrepareJobsMenu()));
    m_cancelJobs = new QAction(i18n("Cancel All Jobs"), this);
    m_cancelJobs->setCheckable(false);
    connect(this, SIGNAL(checkJobProcess()), this, SLOT(slotCheckJobProcess()));
    m_discardCurrentClipJobs = new QAction(i18n("Cancel Current Clip Jobs"), this);
    m_discardCurrentClipJobs->setCheckable(false);
    m_jobsMenu->addAction(m_cancelJobs);
    m_jobsMenu->addAction(m_discardCurrentClipJobs);
    m_infoLabel->setMenu(m_jobsMenu);
    
    m_infoLabel->setAction(infoAction);

    // Build item view model
    m_itemModel = new ProjectItemModel(this);

    // Connect models
    m_proxyModel->setSourceModel(m_itemModel);
    connect(m_itemModel, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)), m_proxyModel, SLOT(slotDataChanged(const QModelIndex&,const
    QModelIndex&)));
    connect(m_itemModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_itemModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(m_proxyModel, SIGNAL(selectModel(QModelIndex)), this, SLOT(selectProxyModel(QModelIndex)));
    connect(m_itemModel, SIGNAL(markersNeedUpdate(QString,QList<int>)), this, SLOT(slotMarkersNeedUpdate(QString,QList<int>)));
    connect(m_itemModel, SIGNAL(itemDropped(QStringList, const QModelIndex &)), this, SLOT(slotItemDropped(QStringList, const QModelIndex &)));
    connect(m_itemModel, SIGNAL(itemDropped(const QList<QUrl>&, const QModelIndex &)), this, SLOT(slotItemDropped(const QList<QUrl>&, const QModelIndex &)));
    connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(slotItemEdited(QModelIndex,QModelIndex,QVector<int>)));
    connect(m_itemModel, SIGNAL(addClipCut(QString,int,int)), this, SLOT(slotAddClipCut(QString,int,int)));

    // Zoom slider
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMaximumWidth(100);
    m_slider->setMinimumWidth(40);
    m_slider->setRange(0, 10);
    m_slider->setValue(4);
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(slotSetIconSize(int)));
    QWidgetAction * widgetslider = new QWidgetAction(this);
    widgetslider->setDefaultWidget(m_slider);

    // View type
    KSelectAction *listType = new KSelectAction(QIcon::fromTheme("view-list-tree"), i18n("View Mode"), this);
    QAction *treeViewAction = listType->addAction(QIcon::fromTheme("view-list-tree"), i18n("Tree View"));
    treeViewAction->setData(BinTreeView);
    if (m_listType == treeViewAction->data().toInt()) {
        listType->setCurrentAction(treeViewAction);
    }
    QAction *iconViewAction = listType->addAction(QIcon::fromTheme("view-list-icons"), i18n("Icon View"));
    iconViewAction->setData(BinIconView);
    if (m_listType == iconViewAction->data().toInt()) {
        listType->setCurrentAction(iconViewAction);
    }
    listType->setToolBarMode(KSelectAction::MenuMode);
    connect(listType, SIGNAL(triggered(QAction*)), this, SLOT(slotInitView(QAction*)));

    // Settings menu
    QMenu *settingsMenu = new QMenu(i18n("Settings"), this);
    settingsMenu->addAction(listType);
    QMenu *sliderMenu = new QMenu(i18n("Zoom"), this);
    sliderMenu->setIcon(QIcon::fromTheme("file-zoom-in"));
    sliderMenu->addAction(widgetslider);
    settingsMenu->addMenu(sliderMenu);
    QToolButton *button = new QToolButton;
    button->setIcon(QIcon::fromTheme("configure"));
    button->setMenu(settingsMenu);
    button->setPopupMode(QToolButton::InstantPopup);
    m_toolbar->addWidget(button);

    m_eventEater = new EventEater(this);
    connect(m_eventEater, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    connect(m_eventEater, SIGNAL(deleteSelectedClips()), this, SLOT(slotDeleteClip()));
    m_binTreeViewDelegate = new BinItemDelegate(this);
    //connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
    m_splitter = new QSplitter(this);
    m_headerInfo = QByteArray::fromBase64(KdenliveSettings::treeviewheaders().toLatin1());

    connect(m_eventEater, SIGNAL(editItem(QModelIndex)), this, SLOT(slotSwitchClipProperties(QModelIndex)), Qt::UniqueConnection);
    connect(m_eventEater, SIGNAL(showMenu(QString)), this, SLOT(showClipMenu(QString)), Qt::UniqueConnection);

    layout->addWidget(m_splitter);
    m_propertiesPanel = new QWidget(m_splitter);
    m_splitter->addWidget(m_propertiesPanel);
    m_collapser = new KSplitterCollapserButton(m_propertiesPanel, m_splitter);
    connect(m_collapser, SIGNAL(clicked(bool)), this, SLOT(slotRefreshClipProperties()));
    
    // Info widget for failed jobs, other errors
    m_infoMessage = new BinMessageWidget;
    layout->addWidget(m_infoMessage);
    m_infoMessage->setCloseButtonVisible(true);
    connect(m_infoMessage, SIGNAL(messageClosing()), this, SLOT(slotResetInfoMessage()));
    //m_infoMessage->setWordWrap(true);
    m_infoMessage->hide();
    m_logAction = new QAction(i18n("Show Log"), this);
    m_logAction->setCheckable(false);
    connect(m_logAction, SIGNAL(triggered()), this, SLOT(slotShowJobLog()));
}

Bin::~Bin()
{
    delete m_jobManager;
    delete m_infoMessage;
}

void Bin::slotSaveHeaders()
{
    if (m_itemView && m_listType == BinTreeView) {
        // save current treeview state (column width)
        QTreeView *view = static_cast<QTreeView*>(m_itemView);
        m_headerInfo = view->header()->saveState();
        KdenliveSettings::setTreeviewheaders(m_headerInfo.toBase64());
    }
}

Monitor *Bin::monitor()
{
    return m_monitor;
}

const QStringList Bin::getFolderInfo()
{
    QStringList folderInfo;
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return folderInfo;
    }
    QModelIndex ix = indexes.first();
    if (ix.isValid() && m_proxyModel->selectionModel()->isSelected(ix)) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        while (currentItem->itemType() != AbstractProjectItem::FolderItem) {
            currentItem = currentItem->parent();
        }
        if (currentItem == m_rootFolder) {
            // clip was added to root folder, leave folder info empty
        } else {
            folderInfo << currentItem->clipId();
            folderInfo << currentItem->name();
        }
    }
    return folderInfo;
}

void Bin::slotAddClip()
{
    // Check if we are in a folder
    QStringList folderInfo = getFolderInfo();
    ClipCreationDialog::createClipsCommand(pCore->projectManager()->current(), folderInfo, this);
}

void Bin::deleteClip(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
        m_monitor->openClip(NULL);
    }
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    m_jobManager->discardJobs(id);
    AbstractProjectItem *parent = clip->parent();
    parent->removeChild(clip);
    delete clip;
}

ProjectClip *Bin::getFirstSelectedClip()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return NULL;
    }
    foreach (const QModelIndex &ix, indexes) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *clip = qobject_cast<ProjectClip*>(item);
        if (clip) {
            return clip;
        }
    }
    return NULL;
}

void Bin::slotDeleteClip()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QStringList clipIds;
    QStringList subClipIds;
    QStringList foldersIds;
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid()) continue;
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
        if (!item) continue;
        AbstractProjectItem::PROJECTITEMTYPE type = item->itemType();
        switch (type) {
            case AbstractProjectItem::ClipItem:
                clipIds << item->clipId();
                break;
            case AbstractProjectItem::FolderItem:
                foldersIds << item->clipId();
                break;
            case AbstractProjectItem::SubClipItem:
                //TODO
                subClipIds << item->clipId();
                break;
            default:
                break;
        }
    }
    // For some reason, we get duplicates, which is not expected
    //ids.removeDuplicates();
    m_doc->clipManager()->deleteProjectItems(clipIds, foldersIds);
}

void Bin::slotReloadClip()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid()) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip*>(item);
        if (currentItem) {
            m_monitor->openClip(NULL);
            QDomDocument doc;
            QDomElement xml = currentItem->toXml(doc);
            currentItem->setClipStatus(AbstractProjectItem::StatusWaiting);
            
            // We need to set a temporary id before all outdated producers are replaced;
            pCore->projectManager()->current()->renderer()->getFileProperties(xml, currentItem->clipId(), 150, true);
        }
    }
}

ProjectFolder *Bin::rootFolder()
{
    return m_rootFolder;
}

double Bin::projectRatio()
{
    return m_doc->dar();
}

void Bin::setMonitor(Monitor *monitor)
{
    m_monitor = monitor;
    connect(m_eventEater, SIGNAL(focusClipMonitor()), m_monitor, SLOT(slotActivateMonitor()), Qt::UniqueConnection);
}

int Bin::getFreeFolderId()
{
    return m_folderCounter++;
}

int Bin::getFreeClipId()
{
    return m_clipCounter++;
}

int Bin::lastClipId() const
{
    return qMax(0, m_clipCounter - 1);
}

void Bin::setDocument(KdenliveDoc* project)
{
    // Remove clip from Bin's monitor
    m_monitor->openClip(NULL);
    closeEditing();
    setEnabled(false);
    delete m_rootFolder;
    delete m_itemView;
    m_itemView = NULL;
    delete m_jobManager;
    m_clipCounter = 1;
    m_folderCounter = 1;
    m_doc = project;
    int iconHeight = style()->pixelMetric(QStyle::PM_ToolBarIconSize) * 2;
    m_iconSize = QSize(iconHeight * m_doc->dar(), iconHeight);
    m_itemModel->setIconSize(m_iconSize);
    m_jobManager = new JobManager(this, project->fps());
    m_rootFolder = new ProjectFolder(this);
    setEnabled(true);
    connect(this, SIGNAL(producerReady(QString)), m_doc->renderer(), SLOT(slotProcessingDone(QString)));
    connect(m_jobManager, SIGNAL(addClip(QString,QString,QString)), this, SLOT(slotAddUrl(QString,QString,QString)));
    connect(m_proxyAction, SIGNAL(toggled(bool)), m_doc, SLOT(slotProxyCurrentItem(bool)));
    connect(m_jobManager, SIGNAL(jobCount(int)), m_infoLabel, SLOT(slotSetJobCount(int)));
    connect(m_discardCurrentClipJobs, SIGNAL(triggered()), m_jobManager, SLOT(slotDiscardClipJobs()));
    connect(m_cancelJobs, SIGNAL(triggered()), m_jobManager, SLOT(slotCancelJobs()));
    connect(m_jobManager, SIGNAL(updateJobStatus(QString,int,int,QString,QString,QString)), this, SLOT(slotUpdateJobStatus(QString,int,int,QString,QString,QString)));
    
    connect(m_jobManager, SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)), this, SLOT(gotFilterJobResults(QString,int,int,stringMap,stringMap)));
    
    //connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), m_itemView
    //connect(m_itemModel, SIGNAL(updateCurrentItem()), this, SLOT(autoSelect()));
    slotInitView(NULL);
    autoSelect();
}

void Bin::slotAddUrl(QString url, QString,QString)
{
    QList <QUrl>urls;
    urls << QUrl::fromLocalFile(url);
    QStringList folderInfo = getFolderInfo();
    ClipCreationDialog::createClipsCommand(m_doc, urls, folderInfo, this);
}

void Bin::createClip(QDomElement xml)
{
    // Check if clip should be in a folder
    QString groupId = ProjectClip::getXmlProperty(xml, "kdenlive:folderid");
    ProjectFolder *parentFolder = m_rootFolder;
    if (!groupId.isEmpty()) {
        parentFolder = m_rootFolder->folder(groupId);
        if (!parentFolder) {
            // parent folder does not exist, put in root folder
            parentFolder = m_rootFolder;
        }
    }
    ProjectClip *newItem = new ProjectClip(xml, parentFolder);
}

void Bin::slotAddFolder()
{
    // Check parent item
    QModelIndex ix = m_proxyModel->selectionModel()->currentIndex();
    ProjectFolder *parentFolder  = m_rootFolder;
    if (ix.isValid() && m_proxyModel->selectionModel()->isSelected(ix)) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        while (currentItem->itemType() != AbstractProjectItem::FolderItem) {
            currentItem = currentItem->parent();
        }
        if (currentItem->itemType() == AbstractProjectItem::FolderItem) {
            parentFolder = qobject_cast<ProjectFolder *>(currentItem);
        }
    }
    QString newId = QString::number(getFreeFolderId());
    AddBinFolderCommand *command = new AddBinFolderCommand(this, newId, i18n("Folder"), parentFolder->clipId());
    m_doc->commandStack()->push(command);

    // Edit folder name
    ix = getIndexForId(newId, true);
    if (ix.isValid()) {
        m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(ix), QItemSelectionModel::ClearAndSelect);
        m_itemView->edit(m_proxyModel->mapFromSource(ix));
    }
}

QModelIndex Bin::getIndexForId(const QString &id, bool folderWanted) const
{
    QModelIndexList items = m_itemModel->match(m_itemModel->index(0, 0), AbstractProjectItem::DataId, QVariant::fromValue(id), 2, Qt::MatchRecursive);
    for (int i = 0; i < items.count(); i++) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(items.at(i).internalPointer());
        AbstractProjectItem::PROJECTITEMTYPE type = currentItem->itemType();
        if (folderWanted && type == AbstractProjectItem::FolderItem) {
            // We found our folder
            return items.at(i);
        }
        else if (!folderWanted && type == AbstractProjectItem::ClipItem) {
            // We found our clip
            return items.at(i);
        }
    }
    return QModelIndex();
}

void Bin::selectClipById(const QString &id)
{
    QModelIndex ix = getIndexForId(id, false);
    if (ix.isValid()) {
        m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(ix), QItemSelectionModel::ClearAndSelect);
    }
}

void Bin::doAddFolder(const QString &id, const QString &name, const QString &parentId)
{
    ProjectFolder *parentFolder = m_rootFolder->folder(parentId);
    if (!parentFolder) {
        qDebug()<<"  / / ERROR IN PARENT FOLDER";
        return;
    }
    ProjectFolder *newItem = new ProjectFolder(id, name, parentFolder);
    emit storeFolder(parentId + "." + id, name);
}


void Bin::slotLoadFolders(QMap<QString,QString> foldersData)
{
    // Folder parent is saved in folderId, separated by a dot. for example "1.3" means parent folder id is "1" and new folder id is "3".
    ProjectFolder *parentFolder = m_rootFolder;
    QStringList folderIds = foldersData.keys();
    while (!folderIds.isEmpty()) {
        for (int i = 0; i < folderIds.count(); i++) {
            QString id = folderIds.at(i);
            QString parentId = id.section(".", 0, 0);
            if (parentId == "-1") {
                parentFolder = m_rootFolder;
            }
            else {
                // This is a sub-folder
                parentFolder = m_rootFolder->folder(parentId);
                if (parentFolder == m_rootFolder) {
                    // parent folder not yet created, try later
                    continue;
                }
            }
            // parent was found, create our folder
            QString folderId = id.section(".", 1, 1);
            int numericId = folderId.toInt();
            if (numericId >= m_folderCounter) m_folderCounter = numericId + 1;
            ProjectFolder *newItem = new ProjectFolder(folderId, foldersData.value(id), parentFolder);
            folderIds.removeAll(id);
        }
    }
}

void Bin::removeFolder(const QString &id, QUndoCommand *deleteCommand)
{
    // Check parent item
    ProjectFolder *folder = m_rootFolder->folder(id);
    AbstractProjectItem *parent = folder->parent();
    new AddBinFolderCommand(this, folder->clipId(), folder->name(), parent->clipId(), true, deleteCommand);
}

void Bin::doRemoveFolder(const QString &id)
{
    ProjectFolder *folder = m_rootFolder->folder(id);
    if (!folder) {
        qDebug()<<"  / / FOLDER not found";
        return;
    }
    //TODO: warn user on non-empty folders
    AbstractProjectItem *parent = folder->parent();
    parent->removeChild(folder);
    emit storeFolder(parent->clipId()+ "." + id, QString());
    delete folder;
}


void Bin::emitAboutToAddItem(AbstractProjectItem* item)
{
    m_itemModel->onAboutToAddItem(item);
}

void Bin::emitItemAdded(AbstractProjectItem* item)
{
    m_itemModel->onItemAdded(item);
}

void Bin::emitAboutToRemoveItem(AbstractProjectItem* item)
{
    m_itemModel->onAboutToRemoveItem(item);
}

void Bin::emitItemRemoved(AbstractProjectItem* item)
{
    m_itemModel->onItemRemoved(item);
}

void Bin::rowsInserted(const QModelIndex &/*parent*/, int /*start*/, int end)
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
      const QModelIndex id = m_itemModel->index(end, 0, QModelIndex());
      m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    }
    //selectModel(id);
}

void Bin::rowsRemoved(const QModelIndex &/*parent*/, int start, int /*end*/)
{
    const QModelIndex id = m_itemModel->index(start, 0, QModelIndex());
    m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    //selectModel(id);
}

void Bin::selectModel(const QModelIndex &id)
{
    m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    /*if (id.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem*>(id.internalPointer());
        if (currentItem) {
            //m_openedProducer = currentItem->clipId();
        }
    }*/
}

void Bin::selectProxyModel(const QModelIndex &id)
{
    if (id.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(id).internalPointer());
	if (currentItem) {
            // Set item as current so that it displays its content in clip monitor
            currentItem->setCurrent(true);
            if (currentItem->itemType() != AbstractProjectItem::FolderItem) {
                m_editAction->setEnabled(true);
                m_reloadAction->setEnabled(true);
                if (m_propertiesPanel->width() > 0) {
                    // if info panel is displayed, update info
                    showClipProperties((ProjectClip *)currentItem);
                    m_deleteAction->setText(i18n("Delete Clip"));
                    m_proxyAction->setText(i18n("Proxy Clip"));
                }
            } else {
                // A folder was selected, disable editing clip
                m_editAction->setEnabled(false);
                m_reloadAction->setEnabled(false);
                m_deleteAction->setText(i18n("Delete Folder"));
                m_proxyAction->setText(i18n("Proxy Folder"));
            }
	    m_deleteAction->setEnabled(true);
        } else {
            m_reloadAction->setEnabled(false);
	    m_editAction->setEnabled(false);
	    m_deleteAction->setEnabled(false);
	}
    }
    else {
        // No item selected in bin
	m_editAction->setEnabled(false);
	m_deleteAction->setEnabled(false);
        // Hide properties panel
        m_collapser->collapse();
        showClipProperties(NULL);
	// Display black bg in clip monitor
	m_monitor->openClip(NULL);
    }
}

void Bin::autoSelect()
{
    /*QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(current).internalPointer());
    if (!currentItem) {
        QModelIndex id = m_proxyModel->index(0, 0, QModelIndex());
        //selectModel(id);
        //m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    }*/
}

QList <ProjectClip *> Bin::selectedClips()
{
    //TODO: handle clips inside folders
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QList <ProjectClip *> list;
    foreach (const QModelIndex &ix, indexes) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip*>(item);
	if (currentItem) {
	    list << currentItem;
	}
    }
    return list;
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
        break;
    default:
        m_itemView = new QTreeView(m_splitter);
        break;
    }
    m_itemView->setMouseTracking(true);
    m_itemView->viewport()->installEventFilter(m_eventEater);
    QSize zoom = m_iconSize;
    zoom = zoom * (m_slider->value() / 4.0);
    m_itemView->setIconSize(zoom);
    m_itemView->setModel(m_proxyModel);
    m_itemView->setSelectionModel(m_proxyModel->selectionModel());
    m_splitter->addWidget(m_itemView);
    m_splitter->insertWidget(2, m_propertiesPanel);
    m_splitter->setSizes(QList <int>() << 4 << 2);
    m_collapser->collapse();

    // setup some default view specific parameters
    if (m_listType == BinTreeView) {
        m_itemView->setItemDelegate(m_binTreeViewDelegate);
        QTreeView *view = static_cast<QTreeView*>(m_itemView);
	view->setSortingEnabled(true);
	view->setHeaderHidden(true);
        if (!m_headerInfo.isEmpty()) {
            view->header()->restoreState(m_headerInfo);
	} else {
            view->header()->resizeSections(QHeaderView::ResizeToContents);
	}
	connect(view->header(), SIGNAL(sectionResized(int,int,int)), this, SLOT(slotSaveHeaders()));
    }
    else if (m_listType == BinIconView) {
	QListView *view = static_cast<QListView*>(m_itemView);
	view->setViewMode(QListView::IconMode);
	view->setMovement(QListView::Static);
	view->setResizeMode(QListView::Adjust);
	view->setUniformItemSizes(true);
    }
    m_itemView->setEditTriggers(QAbstractItemView::DoubleClicked);
    m_itemView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_itemView->setDragDropMode(QAbstractItemView::DragDrop);
    m_itemView->setAcceptDrops(true);
}

void Bin::slotSetIconSize(int size)
{
    if (!m_itemView) {
        return;
    }
    QSize zoom = m_iconSize;
    zoom = zoom * (size / 4.0);
    m_itemView->setIconSize(zoom);
}


void Bin::slotMarkersNeedUpdate(const QString &id, const QList<int> &markers)
{
    // Check if we have a clip timeline that needs update
    /*TimelineWidget *tml = pCore->window()->getTimeline(id);
    if (tml) {
        tml->updateMarkers(markers);
    }*/
    // Update clip monitor
}

void Bin::closeEditing()
{
    //delete m_propertiesPanel;
    //m_propertiesPanel = NULL;
}


void Bin::contextMenuEvent(QContextMenuEvent *event)
{
    bool enableClipActions = false;
    if (m_itemView) {
        QModelIndex idx = m_itemView->indexAt(m_itemView->viewport()->mapFromGlobal(event->globalPos()));
        if (idx != QModelIndex()) {
	    // User right clicked on a clip
            AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(idx).internalPointer());
            if (currentItem) {
		enableClipActions = true;
		m_proxyAction->blockSignals(true);
                ProjectClip *clip = qobject_cast<ProjectClip*>(currentItem);
		if (clip) {
                    m_proxyAction->setChecked(clip->hasProxy());
                    QList<QAction *> transcodeActions = m_transcodeAction->actions();
                    QStringList data;
                    QString condition;
                    for (int i = 0; i < transcodeActions.count(); ++i) {
                        data = transcodeActions.at(i)->data().toStringList();
                        if (data.count() > 3) {
                            condition = data.at(4);
                            if (condition.startsWith("vcodec"))
                                transcodeActions.at(i)->setEnabled(clip->hasCodec(condition.section('=', 1, 1), false));
                            else if (condition.startsWith("acodec"))
                                transcodeActions.at(i)->setEnabled(clip->hasCodec(condition.section('=', 1, 1), true));
                        }
                    }
                }
		m_proxyAction->blockSignals(false);
            }
        }
    }
    // Enable / disable clip actions
    m_proxyAction->setEnabled(enableClipActions);
    m_transcodeAction->setEnabled(enableClipActions);
    m_editAction->setEnabled(enableClipActions);
    m_reloadAction->setEnabled(enableClipActions);
    m_clipsActionsMenu->setEnabled(enableClipActions);
    m_extractAudioAction->setEnabled(enableClipActions);
    // Show menu
    m_menu->exec(event->globalPos());
}


void Bin::slotRefreshClipProperties()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &ix, indexes) {
        if (ix.isValid()) {
            AbstractProjectItem *clip = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
            if (clip && clip->itemType() == AbstractProjectItem::ClipItem) {
                showClipProperties(qobject_cast<ProjectClip *>(clip));
                break;
            }
        }
    }
}

void Bin::slotSwitchClipProperties()
{
    QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    slotSwitchClipProperties(current);
}

void Bin::slotSwitchClipProperties(const QModelIndex &ix)
{
    if (ix.isValid()) {
        if (m_collapser->isWidgetCollapsed()) {
            AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
            ProjectClip *clip = qobject_cast<ProjectClip*>(item);
            if (clip) {
                m_collapser->restore();
                showClipProperties(clip);
            }
            else m_collapser->collapse();
            /*else if (clip->isFolder() && m_listType == BinIconView) {
                // Double clicking on a folder enters it in icon view
                m_itemView->setRootIndex(ix);
            }*/
        }
        else m_collapser->collapse();
    }
    else m_collapser->collapse();
}

void Bin::showClipProperties(ProjectClip *clip)
{
    closeEditing();
    QString panelId = m_propertiesPanel->property("clipId").toString();
    if (!clip || m_propertiesPanel->width() == 0) {
        m_propertiesPanel->setProperty("clipId", QVariant());
        foreach (QWidget * w, m_propertiesPanel->findChildren<ClipPropertiesController*>()) {
            delete w;
        }
        return;
    }
    if (panelId == clip->clipId()) {
        // the properties panel is already displaying current clip, do nothing
        return;
    }
    
    // Cleanup widget for new content
    foreach (QWidget * w, m_propertiesPanel->findChildren<ClipPropertiesController*>()) {
            delete w;
    }
    m_propertiesPanel->setProperty("clipId", clip->clipId());
    QVBoxLayout *lay = (QVBoxLayout*) m_propertiesPanel->layout();
    if (lay == 0) {
        lay = new QVBoxLayout(m_propertiesPanel);
        m_propertiesPanel->setLayout(lay);
    }
    ClipPropertiesController *panel = clip->buildProperties(m_propertiesPanel);
    connect(panel, SIGNAL(updateClipProperties(const QString &, QMap<QString, QString>, QMap<QString, QString>)), this, SLOT(slotEditClipCommand(const QString &, QMap<QString, QString>, QMap<QString, QString>)));
    lay->addWidget(panel);
}


void Bin::slotEditClipCommand(const QString &id, QMap<QString, QString>oldProps, QMap<QString, QString>newProps)
{
    EditClipCommand *command = new EditClipCommand(m_doc, id, oldProps, newProps, true);
    m_doc->commandStack()->push(command);
}

void Bin::reloadClip(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    QDomDocument doc;
    QDomElement xml = clip->toXml(doc);
    pCore->projectManager()->current()->renderer()->getFileProperties(xml, id, 150, true);
}

void Bin::refreshEditedClip()
{
    const QString id = m_propertiesPanel->property("clipId").toString();
    /*pCore->projectManager()->current()->bin()->refreshThumnbail(id);
    pCore->projectManager()->current()->binMonitor()->refresh();*/
}

void Bin::slotThumbnailReady(const QString &id, const QImage &img)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) clip->setThumbnail(img);
}

ProjectClip *Bin::getBinClip(const QString &id)
{
    ProjectClip *clip = NULL;
    if (id.contains("_")) {
        clip = m_rootFolder->clip(id.section("_", 0, 0));
    }
    else {
        clip = m_rootFolder->clip(id);
    }
    return clip;
}

void Bin::setWaitingStatus(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) clip->setClipStatus(AbstractProjectItem::StatusWaiting);
}

void Bin::slotProducerReady(requestClipInfo info, ClipController *controller)
{
    ProjectClip *clip = m_rootFolder->clip(info.clipId);
    if (clip) {
        if (!clip->hasProxy()) {
            // Check for file modifications
            ClipType t = clip->clipType();
            if (t == AV || t == Audio || t == Image || t == Video || t == Playlist) {
                m_doc->watchFile(clip->url());
            }
        }
	if (controller) clip->setProducer(controller, info.replaceProducer);
        QString currentClip = m_monitor->activeClipId();
        if (currentClip.isEmpty()) {
            //No clip displayed in monitor, check if item is selected
            QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
            foreach (const QModelIndex &ix, indexes) {
                ProjectClip *currentItem = static_cast<ProjectClip *>(m_proxyModel->mapToSource(ix).internalPointer());
                if (currentItem->clipId() == info.clipId) {
                    // Item was selected, show it in monitor
                    currentItem->setCurrent(true);
                    break;
                }
            }
        }
        else if (currentClip == info.clipId) {
            m_monitor->openClip(NULL);
            clip->setCurrent(true);
            //m_monitor->openClip(controller);
        }
    }
    else {
	// Clip not found, create it
        QString groupId = controller->property("kdenlive:folderid");
        ProjectFolder *parentFolder;
        if (!groupId.isEmpty()) {
            parentFolder = m_rootFolder->folder(groupId);
            if (!parentFolder) {
                // parent folder does not exist, put in root folder
                parentFolder = m_rootFolder;
            }
            if (groupId.toInt() >= m_folderCounter) m_folderCounter = groupId.toInt() + 1;
        }
        else parentFolder = m_rootFolder;
        ProjectClip *newItem = new ProjectClip(info.clipId, controller, parentFolder);
        if (info.clipId.toInt() >= m_clipCounter) m_clipCounter = info.clipId.toInt() + 1;
    }
    emit producerReady(info.clipId);
}

void Bin::openProducer(ClipController *controller)
{
    m_monitor->openClip(controller);
}

void Bin::openProducer(ClipController *controller, int in, int out)
{
    m_monitor->openClipZone(controller, in, out);
}

void Bin::emitItemUpdated(AbstractProjectItem* item)
{
    emit itemUpdated(item);
}


void Bin::setupGeneratorMenu(const QHash<QString,QMenu*>& menus)
{
    if (!m_menu) {
        //qDebug()<<"Warning, menu was not created, something is wrong";
        return;
    }
    if (!menus.contains("addMenu") && ! menus.value("addMenu") )
        return;

    QMenu *menu = m_addButton->menu();
    if (menus.contains("addMenu") && menus.value("addMenu")){
        QMenu* addMenu=menus.value("addMenu");
        menu->addMenu(addMenu);
        m_addButton->setMenu(menu);
        if (addMenu->isEmpty())
            addMenu->setEnabled(false);
    }
    if (menus.contains("extractAudioMenu") && menus.value("extractAudioMenu") ){
        QMenu* extractAudioMenu = menus.value("extractAudioMenu");
        m_menu->addMenu(extractAudioMenu);
        m_extractAudioAction = extractAudioMenu;
    }
    if (menus.contains("transcodeMenu") && menus.value("transcodeMenu") ){
        QMenu* transcodeMenu = menus.value("transcodeMenu");
        m_menu->addMenu(transcodeMenu);
        if (transcodeMenu->isEmpty())
            transcodeMenu->setEnabled(false);
        m_transcodeAction = transcodeMenu;
    }
    if (menus.contains("clipActionsMenu") && menus.value("clipActionsMenu") ){
        QMenu* stabilizeMenu=menus.value("clipActionsMenu");
        m_menu->addMenu(stabilizeMenu);
        if (stabilizeMenu->isEmpty())
            stabilizeMenu->setEnabled(false);
        m_clipsActionsMenu = stabilizeMenu;

    }
    if (m_reloadAction) m_menu->addAction(m_reloadAction);
    if (m_proxyAction) m_menu->addAction(m_proxyAction);
    if (menus.contains("inTimelineMenu") && menus.value("inTimelineMenu")){
        QMenu* inTimelineMenu=menus.value("inTimelineMenu");
        m_menu->addMenu(inTimelineMenu);
        inTimelineMenu->setEnabled(false);
    }
    m_menu->addAction(m_editAction);
    m_menu->addAction(m_openAction);
    m_menu->addAction(m_deleteAction);
    m_menu->insertSeparator(m_deleteAction);
}

void Bin::setupMenu(QMenu *addMenu, QAction *defaultAction, QHash <QString, QAction*> actions)
{
    // Setup actions
    m_editAction = actions.value("properties");
    m_toolbar->addAction(m_editAction);

    m_deleteAction = actions.value("delete");
    m_toolbar->addAction(m_deleteAction);

    m_openAction = actions.value("open");
    m_reloadAction = actions.value("reload");
    m_proxyAction = actions.value("proxy");

    QMenu *m = new QMenu;
    m->addActions(addMenu->actions());
    m_addButton = new QToolButton;
    m_addButton->setMenu(m);
    m_addButton->setDefaultAction(defaultAction);
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(m_addButton);
    m_menu = new QMenu();
    m_menu->addActions(addMenu->actions());
}

const QString Bin::getDocumentProperty(const QString &key)
{
    return m_doc->getDocumentProperty(key);
}

const QSize Bin::getRenderSize()
{
    return m_doc->getRenderSize();
}

JobManager *Bin::jobManager()
{
    return m_jobManager;
}

void Bin::slotUpdateJobStatus(const QString&id, int jobType, int status, const QString &label, const QString &actionName, const QString &details)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setJobStatus((AbstractClipJob::JOBTYPE) jobType, (ClipJobStatus) status);
    }
    if (status == JobCrashed) {
        QList<QAction *> actions = m_infoMessage->actions();
        if (m_infoMessage->isHidden()) {
            m_infoMessage->setText(label);
            m_infoMessage->setWordWrap(m_infoMessage->text().length() > 35);
            m_infoMessage->setMessageType(KMessageWidget::Warning);
        }

        if (!actionName.isEmpty()) {
            QAction *action = NULL;
            QList< KActionCollection * > collections = KActionCollection::allCollections();
            for (int i = 0; i < collections.count(); ++i) {
                KActionCollection *coll = collections.at(i);
                action = coll->action(actionName);
                if (action) break;
            }
            if (action && !actions.contains(action)) m_infoMessage->addAction(action);
        }
        if (!details.isEmpty()) {
            m_errorLog.append(details);
            if (!actions.contains(m_logAction)) m_infoMessage->addAction(m_logAction);
        }
        m_infoMessage->animatedShow();
    }
}

void Bin::displayMessage(const QString &text, KMessageWidget::MessageType type)
{
    if (m_infoMessage->isHidden()) {
        m_infoMessage->setText(text);
        m_infoMessage->setWordWrap(m_infoMessage->text().length() > 35);
        m_infoMessage->setMessageType(type);
        m_infoMessage->animatedShow();
    }
}

void Bin::slotShowJobLog()
{
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout;
    QTextEdit t(&d);
    for (int i = 0; i < m_errorLog.count(); ++i) {
        if (i > 0) t.insertHtml("<br><hr /><br>");
        t.insertPlainText(m_errorLog.at(i));
    }
    t.setReadOnly(true);
    l->addWidget(&t);
    mainWidget->setLayout(l);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    d.setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    mainLayout->addWidget(buttonBox);
    d.connect(buttonBox, SIGNAL(rejected()), &d, SLOT(accept()));
    d.exec();
}

void Bin::gotProxy(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        QDomDocument doc;
        QDomElement xml = clip->toXml(doc);
        pCore->projectManager()->current()->renderer()->getFileProperties(xml, id, 150, true);
    }
}

void Bin::reloadProducer(const QString &id, QDomElement xml)
{
    pCore->projectManager()->current()->renderer()->getFileProperties(xml, id, 150, true);
}

void Bin::refreshMonitor(const QString &id)
{
    if (m_monitor->activeClipId() == id)
        m_monitor->refreshMonitor();
}

void Bin::discardJobs(const QString &id, AbstractClipJob::JOBTYPE type)
{
    m_jobManager->discardJobs(id, type);
}


void Bin::slotStartCutJob(const QString &id)
{
    startJob(id, AbstractClipJob::CUTJOB);
}

void Bin::startJob(const QString &id, AbstractClipJob::JOBTYPE type)
{
    QList <ProjectClip *> clips;
    ProjectClip *clip = getBinClip(id);
    if (clip && !hasPendingJob(id, type)) {
        // Launch job
        clips << clip;
        m_jobManager->prepareJobs(clips, type);
    }
}

bool Bin::hasPendingJob(const QString &id, AbstractClipJob::JOBTYPE type)
{
    return m_jobManager->hasPendingJob(id, type);
}

void Bin::slotCreateProjectClip()
{
    QAction* act = qobject_cast<QAction *>(sender());
    if (act == 0) {
        // Cannot access triggering action, something is wrong
        qDebug()<<"// Error in clip creation action";
        return;
    }
    ClipType type = (ClipType) act->data().toInt();
    QStringList folderInfo = getFolderInfo();
    switch (type) {
      case Color:
          ClipCreationDialog::createColorClip(pCore->projectManager()->current(), folderInfo, this);
          break;
      case SlideShow:
          ClipCreationDialog::createSlideshowClip(pCore->projectManager()->current(), folderInfo, this);
          break;
      case Text:
          ClipCreationDialog::createTitleClip(pCore->projectManager()->current(), folderInfo, QString(), this);
          break;
      case TextTemplate:
          ClipCreationDialog::createTitleTemplateClip(pCore->projectManager()->current(), folderInfo, QString(), this);
          break;
      default:
          break;
    }
}

void Bin::slotItemDropped(QStringList ids, const QModelIndex &parent)
{
    AbstractProjectItem *parentItem;
    if (parent.isValid()) {
        parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        while (parentItem->itemType() != AbstractProjectItem::FolderItem) {
            parentItem = parentItem->parent();
        }
    }
    else {
        parentItem = m_rootFolder;
    }
    QUndoCommand *moveCommand = new QUndoCommand();
    moveCommand->setText(i18np("Move Clip", "Move Clips", ids.count()));
    foreach(const QString &id, ids) {
        ProjectClip *currentItem = m_rootFolder->clip(id);
        AbstractProjectItem *currentParent = currentItem->parent();
        if (currentParent != parentItem) {
            // Item was dropped on a different folder
            new MoveBinClipCommand(this, id, currentParent->clipId(), parentItem->clipId(), moveCommand);
        }
    }
    m_doc->commandStack()->push(moveCommand);
}

void Bin::doMoveClip(const QString &id, const QString &newParentId)
{
    ProjectClip *currentItem = m_rootFolder->clip(id);
    AbstractProjectItem *currentParent = currentItem->parent();
    ProjectFolder *newParent = m_rootFolder->folder(newParentId);
    currentParent->removeChild(currentItem);
    currentItem->setParent(newParent);
    currentItem->updateParentInfo(newParentId, newParent->name());
}

void Bin::droppedUrls(QList <QUrl> urls, const QMap<QString,QString> properties)
{
    QModelIndex current = m_proxyModel->mapToSource(m_proxyModel->selectionModel()->currentIndex());
    slotItemDropped(urls, current);
}

void Bin::slotItemDropped(const QList<QUrl>&urls, const QModelIndex &parent)
{
    QStringList folderInfo;
    if (parent.isValid()) {
        // Check if drop occured on a folder
        AbstractProjectItem *parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        while (parentItem->itemType() != AbstractProjectItem::FolderItem) {
            parentItem = parentItem->parent();
        }
        if (parentItem != m_rootFolder) {
            folderInfo << parentItem->name();
            folderInfo << parentItem->clipId();
        }
    }
    //TODO: verify if urls exist, check for folders
    ClipCreationDialog::createClipsCommand(pCore->projectManager()->current(), urls, folderInfo, this);
}

void Bin::slotItemEdited(QModelIndex ix,QModelIndex,QVector<int>)
{
    // An item name was edited
    if (!ix.isValid()) return;
    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(ix.internalPointer());
    if (currentItem && currentItem->itemType() == AbstractProjectItem::FolderItem) {
        //TODO: Use undo command for this
        AbstractProjectItem *parentFolder = currentItem->parent();
        emit storeFolder(parentFolder->clipId() + "." + currentItem->clipId(), currentItem->name());
    }
}


void Bin::slotStartClipJob(bool enable)
{
    QAction* act = qobject_cast<QAction *>(sender());
    if (act == 0) {
        // Cannot access triggering action, something is wrong
        qDebug()<<"// Error in clip job action";
        return;
    }
    startClipJob(act->data().toStringList());
}

void Bin::startClipJob(const QStringList &params)
{
    QStringList data = params;
    if (data.isEmpty()) {
        qDebug()<<"// Error in clip job action";
        return;
    }
    AbstractClipJob::JOBTYPE jobType = (AbstractClipJob::JOBTYPE) data.takeFirst().toInt();
    QList <ProjectClip *>clips = selectedClips();
    m_jobManager->prepareJobs(clips, jobType, data);
}

void Bin::slotCancelRunningJob(const QString &id, const QMap<QString, QString> &newProps)
{
    if (newProps.isEmpty()) return;
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    QMap <QString, QString> oldProps;
    QMapIterator<QString, QString> i(newProps);
    while (i.hasNext()) {
        i.next();
        QString value = newProps.value(i.key());
        oldProps.insert(i.key(), value);
    }
    if (newProps == oldProps) return;
    EditClipCommand *command = new EditClipCommand(m_doc, id, oldProps, newProps, true);
    m_doc->commandStack()->push(command);
}


void Bin::slotPrepareJobsMenu()
{
    ProjectClip *item = getFirstSelectedClip();
    if (item) {
        QString id = item->clipId();
        m_discardCurrentClipJobs->setData(id);
        QStringList jobs = m_jobManager->getPendingJobs(id);
        m_discardCurrentClipJobs->setEnabled(!jobs.isEmpty());
    } else {
        m_discardCurrentClipJobs->setData(QString());
        m_discardCurrentClipJobs->setEnabled(false);
    }
}

void Bin::slotAddClipCut(const QString&id, int in, int out)
{
    AddBinClipCutCommand *command = new AddBinClipCutCommand(this, id, in, out, true);
    m_doc->commandStack()->push(command);
}

void Bin::loadSubClips(const QString&id, const QMap <QString,QString> data)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    QMapIterator<QString, QString> i(data);
    while (i.hasNext()) {
        i.next();
        if (!i.value().contains(";")) { 
            // Problem, the zone has no in/out points
            continue;
        }
        int in = i.value().section(";", 0, 0).toInt();
        int out = i.value().section(";", 1, 1).toInt();
        new ProjectSubClip(clip, in, out, i.key());
    }
}

void Bin::addClipCut(const QString&id, int in, int out)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    // Check that we don't already have that subclip
    ProjectSubClip *sub = clip->getSubClip(in, out);
    if (sub) {
        // A subclip with same zone already exists
        return;
    }
    sub = new ProjectSubClip(clip, in, out);
}

void Bin::removeClipCut(const QString&id, int in, int out)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    ProjectSubClip *sub = clip->getSubClip(in, out);
    if (sub) {
        clip->removeChild(sub);
        sub->discard();
        delete sub;
    }
}

void Bin::slotStartFilterJob(const ItemInfo &info, const QString&id, QMap <QString, QString> &filterParams, QMap <QString, QString> &consumerParams, QMap <QString, QString> &extraParams)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;

    QMap <QString, QString> producerParams = QMap <QString, QString> ();
    producerParams.insert("in", QString::number((int) info.cropStart.frames(m_doc->fps())));
    producerParams.insert("out", QString::number((int) (info.cropStart + info.cropDuration).frames(m_doc->fps())));
    extraParams.insert("clipStartPos", QString::number((int) info.startPos.frames(m_doc->fps())));
    extraParams.insert("clipTrack", QString::number(info.track));

    m_jobManager->prepareJobFromTimeline(clip, producerParams, filterParams, consumerParams, extraParams);
}

void Bin::focusBinView() const
{
    m_itemView->setFocus();
}


void Bin::slotOpenClip()
{
    ProjectClip *clip = getFirstSelectedClip();
    if (!clip) return;
    if (clip->clipType() == Image) {
      if (KdenliveSettings::defaultimageapp().isEmpty())
          KMessageBox::sorry(QApplication::activeWindow(), i18n("Please set a default application to open images in the Settings dialog"));
      else
          QProcess::startDetached(KdenliveSettings::defaultimageapp(), QStringList() << clip->url().path());
   }
   if (clip->clipType() == Audio) {
      if (KdenliveSettings::defaultaudioapp().isEmpty())
          KMessageBox::sorry(QApplication::activeWindow(), i18n("Please set a default application to open audio files in the Settings dialog"));
      else
          QProcess::startDetached(KdenliveSettings::defaultaudioapp(), QStringList() << clip->url().path());
    }
}

