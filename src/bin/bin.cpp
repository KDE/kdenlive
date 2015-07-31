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
#include "projectfolderup.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include "project/clipmanager.h"
#include "project/dialogs/slideshowclip.h"
#include "project/jobs/jobmanager.h"
#include "monitor/monitor.h"
#include "doc/kdenlivedoc.h"
#include "dialogs/clipcreationdialog.h"
#include "titler/titlewidget.h"
#include "core.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/clippropertiescontroller.h"
#include "project/projectcommands.h"
#include "project/invaliddialog.h"
#include "projectsortproxymodel.h"
#include "bincommands.h"
#include "mlt++/Mlt.h"

#include <KToolBar>
#include <KColorScheme>
#include <KMessageBox>
#include <KSplitterCollapserButton>
#include <KMessageBox>

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTimeLine>
#include <QSlider>
#include <QMenu>
#include <QDebug>
#include <QtConcurrent>
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
            if (!idx.isValid()) {
                // User double clicked on empty area
                emit addClip();
            }
            else {
                /*AbstractProjectItem *item = static_cast<AbstractProjectItem*>(idx.internalPointer());
                if (item->itemType() == AbstractProjectItem::FolderItem) qDebug()<<"*****************  FLD CLK ****************";*/
		emit itemDoubleClicked(idx, mouseEvent->pos());
                //return QObject::eventFilter(obj, event);
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
  , isLoading(false)
  , m_itemModel(NULL)
  , m_itemView(NULL)
  , m_rootFolder(NULL)
  , m_folderUp(NULL)
  , m_jobManager(NULL)
  , m_doc(NULL)
  , m_listType((BinViewType) KdenliveSettings::binMode())
  , m_iconSize(160, 90)
  , m_propertiesPanel(NULL)
  , m_blankThumb()
  , m_invalidClipDialog(NULL)
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
    searchLine->setFocusPolicy(Qt::ClickFocus);
    connect(searchLine, SIGNAL(textChanged(const QString &)), m_proxyModel, SLOT(slotSetSearchString(const QString &)));
    m_toolbar->addWidget(searchLine);

    // Hack, create toolbar spacer
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_toolbar->addWidget(spacer);

    // small info button for pending jobs
    m_infoLabel = new SmallJobLabel(this);
    m_infoLabel->setStyleSheet(SmallJobLabel::getStyleSheet(palette()));
    QAction *infoAction = m_toolbar->addWidget(m_infoLabel);
    m_jobsMenu = new QMenu(this);
    connect(m_jobsMenu, SIGNAL(aboutToShow()), this, SLOT(slotPrepareJobsMenu()));
    m_cancelJobs = new QAction(i18n("Cancel All Jobs"), this);
    m_cancelJobs->setCheckable(false);
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
    connect(m_itemModel, SIGNAL(itemDropped(QStringList, const QModelIndex &)), this, SLOT(slotItemDropped(QStringList, const QModelIndex &)));
    connect(m_itemModel, SIGNAL(itemDropped(const QList<QUrl>&, const QModelIndex &)), this, SLOT(slotItemDropped(const QList<QUrl>&, const QModelIndex &)));
    connect(m_itemModel, SIGNAL(effectDropped(QString, const QModelIndex &)), this, SLOT(slotEffectDropped(QString, const QModelIndex &)));
    connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(slotItemEdited(QModelIndex,QModelIndex,QVector<int>)));
    connect(m_itemModel, SIGNAL(addClipCut(QString,int,int)), this, SLOT(slotAddClipCut(QString,int,int)));

    // Zoom slider
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMaximumWidth(100);
    m_slider->setMinimumWidth(40);
    m_slider->setRange(0, 10);
    m_slider->setValue(KdenliveSettings::bin_zoom());
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
    
    // Column show / hide actions
    m_showDate = new QAction(i18n("Show date"), this);
    m_showDate->setCheckable(true);
    connect(m_showDate, SIGNAL(triggered(bool)), this, SLOT(slotShowDateColumn(bool)));
    m_showDesc = new QAction(i18n("Show description"), this);
    m_showDesc->setCheckable(true);
    connect(m_showDesc, SIGNAL(triggered(bool)), this, SLOT(slotShowDescColumn(bool)));
    settingsMenu->addAction(m_showDate);
    settingsMenu->addAction(m_showDesc);
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

    connect(m_eventEater, SIGNAL(itemDoubleClicked(QModelIndex,QPoint)), this, SLOT(slotItemDoubleClicked(QModelIndex,QPoint)), Qt::UniqueConnection);

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
    connect(this, SIGNAL(requesteInvalidRemoval(QString,QUrl)), this, SLOT(slotQueryRemoval(QString,QUrl)));
}

Bin::~Bin()
{
    blockSignals(true);
    setEnabled(false);
    foreach (QWidget * w, m_propertiesPanel->findChildren<ClipPropertiesController*>()) {
            delete w;
    }
    if (m_rootFolder) {
        while (!m_rootFolder->isEmpty()) {
            AbstractProjectItem *child = m_rootFolder->at(0);
            m_rootFolder->removeChild(child);
            delete child;
        }
    }
    delete m_rootFolder;
    delete m_itemView;
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

const QStringList Bin::getFolderInfo(QModelIndex selectedIx)
{
    QStringList folderInfo;
    QModelIndexList indexes;
    if (selectedIx.isValid()) {
        indexes << selectedIx;
    } else {
        indexes = m_proxyModel->selectionModel()->selectedIndexes();
    }
    if (indexes.isEmpty()) {
        return folderInfo;
    }
    QModelIndex ix = indexes.first();
    if (ix.isValid() && (m_proxyModel->selectionModel()->isSelected(ix) || selectedIx.isValid())) {
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
    ClipCreationDialog::createClipsCommand(m_doc, folderInfo, this);
}

void Bin::deleteClip(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
	emit openClip(NULL);
    }
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    m_jobManager->discardJobs(id);
    AbstractProjectItem *parent = clip->parent();
    parent->removeChild(clip);
    delete clip;
    if (m_listType == BinTreeView) {
        static_cast<QTreeView*>(m_itemView)->resizeColumnToContents(0);
    }
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
    ProjectSubClip *sub;
    QString subId;
    QPoint zone;
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid() || ix.column() != 0) continue;
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
                subId = item->clipId();
                sub = static_cast<ProjectSubClip*>(item);
                zone = sub->zone();
                subId.append(":" + QString::number(zone.x()) + ":" + QString::number(zone.y()));
                subClipIds << subId;
                break;
            default:
                break;
        }
    }
    // For some reason, we get duplicates, which is not expected
    //ids.removeDuplicates();
    m_doc->clipManager()->deleteProjectItems(clipIds, foldersIds, subClipIds);
}

void Bin::slotReloadClip()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip*>(item);
        if (currentItem) {
	    emit openClip(NULL);
            QDomDocument doc;
            QDomElement xml = currentItem->toXml(doc);
            if (!xml.isNull()) {
                currentItem->setClipStatus(AbstractProjectItem::StatusWaiting);
                // We need to set a temporary id before all outdated producers are replaced;
                m_doc->renderer()->getFileProperties(xml, currentItem->clipId(), 150, true);
            }
        }
    }
}

void Bin::slotDuplicateClip()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip*>(item);
        if (currentItem) {
            QStringList folderInfo = getFolderInfo(ix);
            QDomDocument doc;
            QDomElement xml = currentItem->toXml(doc);
            if (!xml.isNull()) ClipCreationDialog::createClipFromXml(m_doc, xml, folderInfo, this);
        }
    }
}

ProjectFolder *Bin::rootFolder()
{
    return m_rootFolder;
}

QUrl Bin::projectFolder() const
{
    return m_doc->projectFolder();
}

void Bin::setMonitor(Monitor *monitor)
{
    m_monitor = monitor;
    connect(m_monitor, SIGNAL(addClipToProject(QUrl)), this, SLOT(slotAddClipToProject(QUrl)));
    connect(m_monitor, SIGNAL(refreshCurrentClip()), this, SLOT(slotOpenCurrent()));
    connect(this, SIGNAL(openClip(ClipController*,int,int)), m_monitor, SLOT(slotOpenClip(ClipController*,int,int)));
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
    emit openClip(NULL);
    closeEditing();
    setEnabled(false);

    // Cleanup previous project
    if (m_rootFolder) {
        while (!m_rootFolder->isEmpty()) {
            AbstractProjectItem *child = m_rootFolder->at(0);
            m_rootFolder->removeChild(child);
            delete child;
        }
    }
    delete m_rootFolder;
    delete m_itemView;
    m_itemView = NULL;
    delete m_jobManager;
    m_clipCounter = 1;
    m_folderCounter = 1;
    m_doc = project;
    int iconHeight = QFontInfo(font()).pixelSize() * 3.5;
    m_iconSize = QSize(iconHeight * m_doc->dar(), iconHeight);
    m_jobManager = new JobManager(this);
    m_rootFolder = new ProjectFolder(this);
    setEnabled(true);
    connect(this, SIGNAL(producerReady(QString)), m_doc->renderer(), SLOT(slotProcessingDone(QString)));
    connect(m_jobManager, SIGNAL(addClip(QString)), this, SLOT(slotAddUrl(QString)));
    connect(m_proxyAction, SIGNAL(toggled(bool)), m_doc, SLOT(slotProxyCurrentItem(bool)));
    connect(m_jobManager, SIGNAL(jobCount(int)), m_infoLabel, SLOT(slotSetJobCount(int)));
    connect(m_discardCurrentClipJobs, SIGNAL(triggered()), m_jobManager, SLOT(slotDiscardClipJobs()));
    connect(m_cancelJobs, SIGNAL(triggered()), m_jobManager, SLOT(slotCancelJobs()));
    connect(m_jobManager, SIGNAL(updateJobStatus(QString,int,int,QString,QString,QString)), this, SLOT(slotUpdateJobStatus(QString,int,int,QString,QString,QString)));
    
    connect(m_jobManager, SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)), this, SLOT(slotGotFilterJobResults(QString,int,int,stringMap,stringMap)));
    
    //connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), m_itemView
    //connect(m_itemModel, SIGNAL(updateCurrentItem()), this, SLOT(autoSelect()));
    slotInitView(NULL);
    autoSelect();
}

void Bin::slotAddUrl(QString url, QMap <QString, QString> data)
{
    QList <QUrl>urls;
    urls << QUrl::fromLocalFile(url);
    QStringList folderInfo = getFolderInfo();
    ClipCreationDialog::createClipsCommand(m_doc, urls, folderInfo, this, data);
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
    new ProjectClip(xml, m_blankThumb, parentFolder);
    if (m_listType == BinTreeView) {
        static_cast<QTreeView*>(m_itemView)->resizeColumnToContents(0);
    }
}

QString Bin::slotAddFolder(const QString &folderName)
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
    AddBinFolderCommand *command = new AddBinFolderCommand(this, newId, folderName.isEmpty() ? i18n("Folder") : folderName, parentFolder->clipId());
    m_doc->commandStack()->push(command);

    // Edit folder name
    if (!folderName.isEmpty()) {
	// We already have a name, no need to edit
	return newId;
    }
    ix = getIndexForId(newId, true);
    if (ix.isValid()) {
        m_proxyModel->selectionModel()->clearSelection();
        int row =ix.row();
        for (int i = 0; i < m_rootFolder->supportedDataCount(); i++) {
            const QModelIndex id = m_itemModel->index(row, i, QModelIndex());
            if (id.isValid()) {
                m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
            }
        }
        m_itemView->edit(m_proxyModel->mapFromSource(ix));
    }
    return newId;
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

void Bin::selectClipById(const QString &clipId)
{
    QModelIndex ix = getIndexForId(clipId, false);
    if (ix.isValid()) {
        m_proxyModel->selectionModel()->clearSelection();
        int row =ix.row();
        for (int i = 0; i < m_rootFolder->supportedDataCount(); i++) {
            const QModelIndex id = m_itemModel->index(row, i, QModelIndex());
            if (id.isValid()) {
                m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
            }
        }
    }
}

void Bin::doAddFolder(const QString &id, const QString &name, const QString &parentId)
{
    ProjectFolder *parentFolder = m_rootFolder->folder(parentId);
    if (!parentFolder) {
        qDebug()<<"  / / ERROR IN PARENT FOLDER";
        return;
    }
    //FIXME(style): constructor actually adds the new pointer to parent's children
    new ProjectFolder(id, name, parentFolder);
    emit storeFolder(id, parentId, QString(), name);
}

void Bin::renameFolder(const QString &id, const QString &name)
{
    ProjectFolder *folder = m_rootFolder->folder(id);
    if (!folder || !folder->parent()) {
        qDebug()<<"  / / ERROR IN PARENT FOLDER";
        return;
    }
    folder->setName(name);
    emit itemUpdated(folder);
    emit storeFolder(id, folder->parent()->clipId(), QString(), name);
}


void Bin::slotLoadFolders(QMap<QString,QString> foldersData)
{
    // Folder parent is saved in folderId, separated by a dot. for example "1.3" means parent folder id is "1" and new folder id is "3".
    ProjectFolder *parentFolder = m_rootFolder;
    QStringList folderIds = foldersData.keys();
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
                // parent folder not yet created, create unnamed placeholder
                parentFolder = new ProjectFolder(parentId, QString(), parentFolder);
            }
        }
        // parent was found, create our folder
        QString folderId = id.section(".", 1, 1);
        int numericId = folderId.toInt();
        if (numericId >= m_folderCounter) m_folderCounter = numericId + 1;
        // Check if placeholder folder was created
        ProjectFolder *placeHolder = parentFolder->folder(folderId);
        if (placeHolder) {
            // Rename placeholder
            placeHolder->setName(foldersData.value(id));
        }
        else {
            // Create new folder
            //FIXME(style): constructor actually adds the new pointer to parent's children
            new ProjectFolder(folderId, foldersData.value(id), parentFolder);
        }
    }
}

void Bin::removeFolder(const QString &id, QUndoCommand *deleteCommand)
{
    // Check parent item
    ProjectFolder *folder = m_rootFolder->folder(id);
    AbstractProjectItem *parent = folder->parent();
    if (folder->count() > 0) {
        // Folder has clips inside, warn user
        if (KMessageBox::warningContinueCancel(this, i18np("Folder contains a clip, delete anyways ?", "Folder contains %1 clips, delete anyways ?", folder->count())) != KMessageBox::Continue) {
            return;
        }
        QStringList clipIds;
        QStringList folderIds;
        // TODO: manage subclips
        for (int i = 0; i < folder->count(); i++) {
            AbstractProjectItem *child = folder->at(i);
            switch (child->itemType()) {
              case AbstractProjectItem::ClipItem:
                  clipIds << child->clipId();
                  break;
              case AbstractProjectItem::FolderItem:
                  folderIds << child->clipId();
                  break;
              default:
                  break;
            }
        }
        foreach(const QString &folderId, folderIds) {
            removeFolder(folderId, deleteCommand);
        }
        m_doc->clipManager()->deleteProjectItems(clipIds, folderIds, QStringList(), deleteCommand);
    }
    new AddBinFolderCommand(this, folder->clipId(), folder->name(), parent->clipId(), true, deleteCommand);
}

void Bin::removeSubClip(const QString &id, QUndoCommand *deleteCommand)
{
    // Check parent item
    QString clipId = id;
    int in = clipId.section(":", 1, 1).toInt();
    int out = clipId.section(":", 2, 2).toInt();
    clipId = clipId.section(":", 0, 0);
    new AddBinClipCutCommand(this, clipId, in, out, false, deleteCommand);
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
    emit storeFolder(id, parent->clipId(), QString(), QString());
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

void Bin::rowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)

    if (!m_proxyModel->selectionModel()->hasSelection()) {
        for (int i = 0; i < m_rootFolder->supportedDataCount(); i++) {
            const QModelIndex id = m_itemModel->index(end, i, QModelIndex());
            if (id.isValid()) {
                m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
            }
        }
    }
}

void Bin::rowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent)
    Q_UNUSED(end)

    QModelIndex id = m_itemModel->index(start, 0, QModelIndex());
    if (!id.isValid() && start > 0) {
        start--;
    }
    for (int i = 0; i < m_rootFolder->supportedDataCount(); i++) {
        id = m_itemModel->index(start, i, QModelIndex());
        if (id.isValid()) {
            m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
        }
    }
}

void Bin::selectProxyModel(const QModelIndex &id)
{
    if (isLoading) return;
    if (id.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(id).internalPointer());
	if (currentItem) {
            // Set item as current so that it displays its content in clip monitor
            currentItem->setCurrent(true);
            if (currentItem->itemType() != AbstractProjectItem::FolderItem) {
                m_editAction->setEnabled(true);
                m_reloadAction->setEnabled(true);
                m_duplicateAction->setEnabled(true);
                ClipType type = static_cast<ProjectClip*>(currentItem)->clipType();
                m_openAction->setEnabled(type == Image || type == Audio);
                if (m_propertiesPanel->width() > 0) {
                    // if info panel is displayed, update info
                    showClipProperties(static_cast<ProjectClip*>(currentItem));
                    m_deleteAction->setText(i18n("Delete Clip"));
                    m_proxyAction->setText(i18n("Proxy Clip"));
                }
            } else {
                // A folder was selected, disable editing clip
                m_editAction->setEnabled(false);
                m_openAction->setEnabled(false);
                m_reloadAction->setEnabled(false);
                m_duplicateAction->setEnabled(false);
                m_deleteAction->setText(i18n("Delete Folder"));
                m_proxyAction->setText(i18n("Proxy Folder"));
            }
	    m_deleteAction->setEnabled(true);
        } else {
            m_reloadAction->setEnabled(false);
            m_duplicateAction->setEnabled(false);
	    m_editAction->setEnabled(false);
            m_openAction->setEnabled(false);
	    m_deleteAction->setEnabled(false);
	}
    }
    else {
        // No item selected in bin
	m_editAction->setEnabled(false);
        m_openAction->setEnabled(false);
	m_deleteAction->setEnabled(false);
        // Hide properties panel
        m_collapser->collapse();
        showClipProperties(NULL);
	emit masterClipSelected(NULL, m_monitor);
	// Display black bg in clip monitor
	emit openClip(NULL);
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
	if (!ix.isValid() || ix.column() != 0) {
	    continue;
	}
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
        KdenliveSettings::setBinMode(viewType);
        if (viewType == m_listType) {
            return;
        }
        if (m_listType == BinTreeView) {
            // save current treeview state (column width)
            QTreeView *view = static_cast<QTreeView*>(m_itemView);
            m_headerInfo = view->header()->saveState();
        }
        else {
            // remove the current folderUp item if any
            if (m_folderUp) {
                delete m_folderUp;
                m_folderUp = NULL;
            }
        }
        m_listType = static_cast<BinViewType>(viewType);
    }

    if (m_itemView) {
        delete m_itemView;
    }

    switch (m_listType) {
    case BinIconView:
        m_itemView = new QListView(m_splitter);
        m_folderUp = new ProjectFolderUp(NULL);
        break;
    default:
        m_itemView = new QTreeView(m_splitter);
        break;
    }
    m_itemView->setMouseTracking(true);
    m_itemView->viewport()->installEventFilter(m_eventEater);
    QSize zoom = m_iconSize * (m_slider->value() / 4.0);
    m_itemView->setIconSize(zoom);
    QPixmap pix(zoom);
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
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
        view->setWordWrap(true);
        connect(m_proxyModel, SIGNAL(layoutAboutToBeChanged()), this, SLOT(slotSetSorting()));
        m_proxyModel->setDynamicSortFilter(true);
        if (!m_headerInfo.isEmpty()) {
            view->header()->restoreState(m_headerInfo);
	} else {
            view->header()->resizeSections(QHeaderView::ResizeToContents);
            view->resizeColumnToContents(0);
            view->setColumnHidden(2, true);
	}
        m_showDate->setChecked(!view->isColumnHidden(1));
        m_showDesc->setChecked(!view->isColumnHidden(2));
	connect(view->header(), SIGNAL(sectionResized(int,int,int)), this, SLOT(slotSaveHeaders()));
    }
    else if (m_listType == BinIconView) {
	QListView *view = static_cast<QListView*>(m_itemView);
	view->setViewMode(QListView::IconMode);
	view->setMovement(QListView::Static);
	view->setResizeMode(QListView::Adjust);
	view->setUniformItemSizes(true);
    }
    m_itemView->setEditTriggers(QAbstractItemView::NoEditTriggers); //DoubleClicked);
    m_itemView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_itemView->setDragDropMode(QAbstractItemView::DragDrop);
    m_itemView->setAlternatingRowColors(true);
    m_itemView->setAcceptDrops(true);
    m_itemView->setFocus();
}

void Bin::slotSetIconSize(int size)
{
    if (!m_itemView) {
        return;
    }
    KdenliveSettings::setBin_zoom(size);
    QSize zoom = m_iconSize;
    zoom = zoom * (size / 4.0);
    m_itemView->setIconSize(zoom);
    QPixmap pix(zoom);
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
}


void Bin::closeEditing()
{
    //delete m_propertiesPanel;
    //m_propertiesPanel = NULL;
}


void Bin::contextMenuEvent(QContextMenuEvent *event)
{
    bool enableClipActions = false;
    ClipType type = Unknown;
    if (m_itemView) {
        QModelIndex idx = m_itemView->indexAt(m_itemView->viewport()->mapFromGlobal(event->globalPos()));
        if (idx.isValid()) {
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
                    QString audioCodec = clip->codec(true);
                    QString videoCodec = clip->codec(false);
                    type = clip->clipType();
                    bool noCodecInfo = false;
                    if (audioCodec.isEmpty() && videoCodec.isEmpty()) {
                        noCodecInfo = true;
                    }
                    for (int i = 0; i < transcodeActions.count(); ++i) {
                        data = transcodeActions.at(i)->data().toStringList();
                        if (data.count() > 4) {
                            condition = data.at(4);
                            if (condition.isEmpty()) {
                                transcodeActions.at(i)->setEnabled(true);
                                continue;
                            }
                            if (noCodecInfo) {
                                // No audio / video codec, this is an MLT clip, disable conditionnal transcoding
                                transcodeActions.at(i)->setEnabled(false);
                                continue;
                            }
                            if (condition.startsWith("vcodec"))
                                transcodeActions.at(i)->setEnabled(condition.section('=', 1, 1) == videoCodec);
                            else if (condition.startsWith("acodec"))
                                transcodeActions.at(i)->setEnabled(condition.section('=', 1, 1) == audioCodec);
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
    m_openAction->setEnabled(type == Image || type == Audio);
    m_reloadAction->setEnabled(enableClipActions);
    m_duplicateAction->setEnabled(enableClipActions);
    m_clipsActionsMenu->setEnabled(enableClipActions);
    m_extractAudioAction->setEnabled(enableClipActions);
    // Show menu
    m_menu->exec(event->globalPos());
}


void Bin::slotRefreshClipProperties()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid() || ix.column() != 0) {
	    continue;
	}
        AbstractProjectItem *clip = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        if (clip && clip->itemType() == AbstractProjectItem::ClipItem) {
            showClipProperties(qobject_cast<ProjectClip *>(clip));
            break;
        }
    }
}


void Bin::slotItemDoubleClicked(const QModelIndex &ix, const QPoint pos)
{
    AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());  
    if (m_listType == BinIconView) {
        if (item->count() > 0 || item->itemType() == AbstractProjectItem::FolderItem) {
            m_folderUp->setParent(item);
            m_itemView->setRootIndex(ix);
            return;
        }
        if (item == m_folderUp) {
            AbstractProjectItem *parentItem = item->parent();
            QModelIndex parent = getIndexForId(parentItem->parent()->clipId(), parentItem->parent()->itemType() == AbstractProjectItem::FolderItem);
            if (parentItem->parent() != m_rootFolder) {
                // We are entering a parent folder
                m_folderUp->setParent(parentItem->parent());
            }
            else m_folderUp->setParent(NULL);
            m_itemView->setRootIndex(m_proxyModel->mapFromSource(parent));
            return;
        }
    }
    else {
        if (item->count() > 0) {
            QTreeView *view = static_cast<QTreeView*>(m_itemView);
            view->setExpanded(ix, !view->isExpanded(ix));
            return;
        }
    }
    if (ix.isValid()) {
        QRect IconRect = m_itemView->visualRect(ix);
        IconRect.setSize(m_itemView->iconSize());
        if (!pos.isNull() && ((ix.column() == 2 && item->itemType() == AbstractProjectItem::ClipItem) || !IconRect.contains(pos))) {
            // User clicked outside icon, trigger rename
            m_itemView->edit(ix);
            return;
        }
        slotSwitchClipProperties(ix);
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
        // User clicked in the icon, open clip properties
        if (m_collapser->isWidgetCollapsed()) {
            AbstractProjectItem *item = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(ix).internalPointer());
            ProjectClip *clip = qobject_cast<ProjectClip*>(item);
            if (clip) {
                m_collapser->restore();
                showClipProperties(clip);
            }
            else m_collapser->collapse();
        }
        else m_collapser->collapse();
    }
    else m_collapser->collapse();
}

void Bin::showClipProperties(ProjectClip *clip)
{
    closeEditing();
    // Special case: text clips open title widget
    if (clip && clip->clipType() == Text) {
        // Cleanup widget for new content
        foreach (QWidget * w, m_propertiesPanel->findChildren<ClipPropertiesController*>()) {
            delete w;
        }
        showTitleWidget(clip);
        m_collapser->collapse();
        return;
    }
    if (clip) qDebug()<<"+ + +SHOWING CLP PROPS: "<<clip->clipType();
    if (clip && clip->clipType() == SlideShow) {
        // Cleanup widget for new content
        foreach (QWidget * w, m_propertiesPanel->findChildren<ClipPropertiesController*>()) {
            delete w;
        }
        showSlideshowWidget(clip);
        m_collapser->collapse();
        return;
    }

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

    QVBoxLayout *lay = static_cast<QVBoxLayout*>(m_propertiesPanel->layout());
    if (lay == 0) {
        lay = new QVBoxLayout(m_propertiesPanel);
        m_propertiesPanel->setLayout(lay);
    }
    ClipPropertiesController *panel = clip->buildProperties(m_propertiesPanel);
    connect(this, SIGNAL(refreshTimeCode()), panel, SLOT(slotRefreshTimeCode()));
    connect(this, SIGNAL(refreshPanelMarkers()), panel, SLOT(slotFillMarkers()));
    connect(panel, SIGNAL(updateClipProperties(const QString &, QMap<QString, QString>, QMap<QString, QString>)), this, SLOT(slotEditClipCommand(const QString &, QMap<QString, QString>, QMap<QString, QString>)));
    connect(panel, SIGNAL(seekToFrame(int)), m_monitor, SLOT(slotSeek(int)));
    connect(panel, SIGNAL(addMarkers(QString,QList<CommentedTime>)), this, SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));

    connect(panel, SIGNAL(editAnalysis(QString,QString,QString)), this, SLOT(slotAddClipExtraData(QString,QString,QString)));

    connect(panel, SIGNAL(loadMarkers(QString)), this, SLOT(slotLoadClipMarkers(QString)));
    connect(panel, SIGNAL(saveMarkers(QString)), this, SLOT(slotSaveClipMarkers(QString)));
    lay->addWidget(panel);
}


void Bin::slotEditClipCommand(const QString &id, QMap<QString, QString>oldProps, QMap<QString, QString>newProps)
{
    EditClipCommand *command = new EditClipCommand(this, id, oldProps, newProps, true);
    m_doc->commandStack()->push(command);
}

void Bin::reloadClip(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    QDomDocument doc;
    QDomElement xml = clip->toXml(doc);
    if (!xml.isNull()) m_doc->renderer()->getFileProperties(xml, id, 150, true);
}

void Bin::slotThumbnailReady(const QString &id, const QImage &img, bool fromFile)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setThumbnail(img);
        // Save thumbnail for later reuse
        if (!fromFile) img.save(m_doc->projectFolder().path() + "/thumbs/" + clip->hash() + ".png");
    }
}

QStringList Bin::getBinFolderClipIds(const QString &id) const
{
    QStringList ids;
    ProjectFolder *folder = m_rootFolder->folder(id);
    if (folder) {
        for (int i = 0; i < folder->count(); i++) {
            AbstractProjectItem *child = folder->at(i);
            if (child->itemType() == AbstractProjectItem::ClipItem) {
                ids << child->clipId();
            }
        }
    }
    return ids;
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

void Bin::slotRemoveInvalidClip(const QString &id, bool replace)
{
    Q_UNUSED(replace)

    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    emit requesteInvalidRemoval(id, clip->url());
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
	clip->setProducer(controller, info.replaceProducer);
        QString currentClip = m_monitor->activeClipId();
        if (currentClip.isEmpty()) {
            //No clip displayed in monitor, check if item is selected
            QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
            foreach (const QModelIndex &ix, indexes) {
		if (!ix.isValid() || ix.column() != 0) {
		    continue;
		}
                ProjectClip *currentItem = static_cast<ProjectClip *>(m_proxyModel->mapToSource(ix).internalPointer());
                if (currentItem->clipId() == info.clipId) {
                    // Item was selected, show it in monitor
                    currentItem->setCurrent(true);
                    break;
                }
            }
        }
        else if (currentClip == info.clipId) {
	    emit openClip(NULL);
            clip->setCurrent(true);
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
        //FIXME(style): constructor actually adds the new pointer to parent's children
        new ProjectClip(info.clipId, m_blankThumb, controller, parentFolder);
        if (info.clipId.toInt() >= m_clipCounter) m_clipCounter = info.clipId.toInt() + 1;
        if (m_listType == BinTreeView) {
            static_cast<QTreeView*>(m_itemView)->resizeColumnToContents(0);
        }
    }
    emit producerReady(info.clipId);
}

void Bin::slotOpenCurrent()
{
    ProjectClip *currentItem = getFirstSelectedClip();
    if (currentItem) {
        emit openClip(currentItem->controller());
    }
}

void Bin::openProducer(ClipController *controller)
{
    emit openClip(controller);
}

void Bin::openProducer(ClipController *controller, int in, int out)
{
    emit openClip(controller, in, out);
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
    /*if (!menus.contains("addMenu") && ! menus.value("addMenu") )
        return;*/

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
    if (m_duplicateAction) m_menu->addAction(m_duplicateAction);
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
    m_duplicateAction = actions.value("duplicate");
    m_proxyAction = actions.value("proxy");

    QMenu *m = new QMenu;
    m->addActions(addMenu->actions());
    m_addButton = new QToolButton;
    m_addButton->setMenu(m);
    m_addButton->setDefaultAction(defaultAction);
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(m_addButton);
    m_menu = new QMenu();
    //m_menu->addActions(addMenu->actions());
}

const QString Bin::getDocumentProperty(const QString &key)
{
    return m_doc->getDocumentProperty(key);
}

const QSize Bin::getRenderSize()
{
    return m_doc->getRenderSize();
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
        if (!xml.isNull()) m_doc->renderer()->getFileProperties(xml, id, 150, true);
    }
}

void Bin::reloadProducer(const QString &id, QDomElement xml)
{
    m_doc->renderer()->getFileProperties(xml, id, 150, true);
}

void Bin::refreshClip(const QString &id)
{
    emit clipNeedsReload(id, false);
    if (m_monitor->activeClipId() == id)
        m_monitor->refreshMonitor();
}

void Bin::refreshClipMarkers(const QString &id)
{
    if (m_monitor->activeClipId() == id)
        m_monitor->updateMarkers();
    if (m_propertiesPanel) {
        QString panelId = m_propertiesPanel->property("clipId").toString();
        if (panelId == id) emit refreshPanelMarkers();
    }
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
        m_jobManager->prepareJobs(clips, m_doc->fps(), type);
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
          ClipCreationDialog::createColorClip(m_doc, folderInfo, this);
          break;
      case SlideShow:
          ClipCreationDialog::createSlideshowClip(m_doc, folderInfo, this);
          break;
      case Text:
          ClipCreationDialog::createTitleClip(m_doc, folderInfo, QString(), this);
          break;
      case TextTemplate:
          ClipCreationDialog::createTitleTemplateClip(m_doc, folderInfo, QString(), this);
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
    QStringList folderIds;
    foreach(const QString &id, ids) {
        if (id.contains("/")) {
            // trying to move clip zone, not allowed. Ignore
            continue;
        }
        if (id.startsWith("#")) {
            // moving a folder, keep it for later
            folderIds << id;
            continue;
        }
        ProjectClip *currentItem = m_rootFolder->clip(id);
        AbstractProjectItem *currentParent = currentItem->parent();
        if (currentParent != parentItem) {
            // Item was dropped on a different folder
            new MoveBinClipCommand(this, id, currentParent->clipId(), parentItem->clipId(), moveCommand);
        }
    }
    if (!folderIds.isEmpty()) { 
        foreach(QString id, folderIds) {
            id.remove(0, 1);
            ProjectFolder *currentItem = m_rootFolder->folder(id);
            AbstractProjectItem *currentParent = currentItem->parent();
            if (currentParent != parentItem) {
                // Item was dropped on a different folder
                new MoveBinFolderCommand(this, id, currentParent->clipId(), parentItem->clipId(), moveCommand);
            }
        }
    }
    m_doc->commandStack()->push(moveCommand);
}


void Bin::slotEffectDropped(QString effect, const QModelIndex &parent)
{
    if (parent.isValid()) {
        AbstractProjectItem *parentItem;
        parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        if (parentItem->itemType() != AbstractProjectItem::ClipItem) {
            // effect only supported on clip items
            return;
        }
        QDomDocument doc;
        doc.setContent(effect);
        QDomElement e = doc.documentElement();
        AddBinEffectCommand *command = new AddBinEffectCommand(this, parentItem->clipId(), e);
        m_doc->commandStack()->push(command);
    }
}

void Bin::slotDeleteEffect(const QString &id, QDomElement effect)
{
    RemoveBinEffectCommand *command = new RemoveBinEffectCommand(this, id, effect);
    m_doc->commandStack()->push(command);
}

void Bin::removeEffect(const QString &id, const QDomElement &effect)
{
    ProjectClip *currentItem = NULL;
    if (id.isEmpty()) {
        currentItem = getFirstSelectedClip();
    }
    else currentItem = m_rootFolder->clip(id);
    if (!currentItem) return;
    currentItem->removeEffect(m_monitor->profileInfo(), effect.attribute("kdenlive_ix").toInt());
    m_monitor->refreshMonitor();
}

void Bin::addEffect(const QString &id, QDomElement &effect)
{
    ProjectClip *currentItem = NULL;
    if (id.isEmpty()) {
        currentItem = getFirstSelectedClip();
    }
    else currentItem = m_rootFolder->clip(id);
    if (!currentItem) return;
    currentItem->addEffect(m_monitor->profileInfo(), effect);
    m_monitor->refreshMonitor();
}

void Bin::editMasterEffect(ClipController *ctl)
{
    emit masterClipSelected(ctl, m_monitor);
}

void Bin::doMoveClip(const QString &id, const QString &newParentId)
{
    ProjectClip *currentItem = m_rootFolder->clip(id);
    if (!currentItem) return;
    AbstractProjectItem *currentParent = currentItem->parent();
    ProjectFolder *newParent = m_rootFolder->folder(newParentId);
    currentParent->removeChild(currentItem);
    currentItem->setParent(newParent);
    currentItem->updateParentInfo(newParentId, newParent->name());
}

void Bin::doMoveFolder(const QString &id, const QString &newParentId)
{
    ProjectFolder *currentItem = m_rootFolder->folder(id);
    AbstractProjectItem *currentParent = currentItem->parent();
    ProjectFolder *newParent = m_rootFolder->folder(newParentId);
    currentParent->removeChild(currentItem);
    currentItem->setParent(newParent);
    emit storeFolder(id, newParent->clipId(), currentParent->clipId(), currentItem->name());
}

void Bin::droppedUrls(QList <QUrl> urls)
{
    QModelIndex current = m_proxyModel->mapToSource(m_proxyModel->selectionModel()->currentIndex());
    slotItemDropped(urls, current);
}

void Bin::slotAddClipToProject(QUrl url)
{
    QList <QUrl> urls;
    urls << url;
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
            folderInfo << parentItem->clipId();
        }
    }
    //TODO: verify if urls exist
    QList <QUrl> clipsToAdd = urls;
    QMimeDatabase db;
    foreach(const QUrl & file, clipsToAdd) {
        // Check there is no folder here
        QMimeType type = db.mimeTypeForUrl(file);
        if (type.inherits("inode/directory")) {
            // user dropped a folder, import its files
            clipsToAdd.removeAll(file);
            QDir dir(file.path());
            QStringList result = dir.entryList(QDir::Files);
            QList <QUrl> folderFiles;
            foreach(const QString & path, result) {
                folderFiles.append(QUrl::fromLocalFile(dir.absoluteFilePath(path)));
            }
            if (folderFiles.count() > 0) {
		QString folderId = slotAddFolder(dir.dirName());
		QModelIndex ind = getIndexForId(folderId, true);
		QStringList newFolderInfo;
		if (ind.isValid()) {
			newFolderInfo = getFolderInfo(m_proxyModel->mapFromSource(ind));
		}
		ClipCreationDialog::createClipsCommand(m_doc, folderFiles, newFolderInfo, this);
	    }
        }
    }
        
    if (!clipsToAdd.isEmpty()) 
	ClipCreationDialog::createClipsCommand(m_doc, clipsToAdd, folderInfo, this);
}

void Bin::slotItemEdited(QModelIndex ix,QModelIndex,QVector<int>)
{
    if (ix.isValid()) {
        // User clicked in the icon, open clip properties
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(ix.internalPointer());
        ProjectClip *clip = qobject_cast<ProjectClip*>(item);  
	if (clip) emit clipNameChanged(clip->clipId());
    }
}

void Bin::renameFolderCommand(const QString &id, const QString &newName, const QString &oldName)
{
    RenameBinFolderCommand *command = new RenameBinFolderCommand(this, id, newName, oldName);
    m_doc->commandStack()->push(command);
}

void Bin::renameSubClipCommand(const QString &id, const QString &newName, const QString oldName, int in, int out)
{
    RenameBinSubClipCommand *command = new RenameBinSubClipCommand(this, id, newName, oldName, in, out);
    m_doc->commandStack()->push(command);
}

void Bin::renameSubClip(const QString &id, const QString &newName, const QString oldName, int in, int out)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    ProjectSubClip *sub = clip->getSubClip(in, out);
    if (!sub) return;
    sub->setName(newName);
    clip->setProducerProperty("kdenlive:clipzone." + oldName, QString());
    clip->setProducerProperty("kdenlive:clipzone." + newName, QString::number(in) + ";" +  QString::number(out));
    emit itemUpdated(sub);
}

void Bin::slotStartClipJob(bool enable)
{
    Q_UNUSED(enable)

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
    m_jobManager->prepareJobs(clips, m_doc->fps(), jobType, data);
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
    EditClipCommand *command = new EditClipCommand(this, id, oldProps, newProps, true);
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
    QDir thumbsFolder(projectFolder().path() + "/thumbs/");
    QList <int> missingThumbs;
    while (i.hasNext()) {
        i.next();
        if (!i.value().contains(";")) { 
            // Problem, the zone has no in/out points
            continue;
        }
        QImage img;
        int in = i.value().section(";", 0, 0).toInt();
        int out = i.value().section(";", 1, 1).toInt();
        missingThumbs << in;
        new ProjectSubClip(clip, in, out, m_doc->timecode().getDisplayTimecodeFromFrames(in, KdenliveSettings::frametimecode()), i.key());
    }
    if (!missingThumbs.isEmpty()) {
        // generate missing subclip thumbnails
        QtConcurrent::run(clip, &ProjectClip::slotExtractSubImage, missingThumbs);
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
    sub = new ProjectSubClip(clip, in, out, m_doc->timecode().getDisplayTimecodeFromFrames(in, KdenliveSettings::frametimecode()));
    QtConcurrent::run(clip, &ProjectClip::slotExtractSubImage, QList <int>() << in);
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

Timecode Bin::projectTimecode() const
{
    return m_doc->timecode();
}

void Bin::slotStartFilterJob(const ItemInfo &info, const QString&id, QMap <QString, QString> &filterParams, QMap <QString, QString> &consumerParams, QMap <QString, QString> &extraParams)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;

    QMap <QString, QString> producerParams = QMap <QString, QString> ();
    producerParams.insert("producer", clip->url().path());
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

void Bin::updateTimecodeFormat()
{
    emit refreshTimeCode();
}


void Bin::slotGotFilterJobResults(QString id, int , int , stringMap results, stringMap filterInfo)
{
    // Currently, only the first value of results is used
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;

    // Check for return value
    int markersType = -1;
    if (filterInfo.contains("addmarkers")) markersType = filterInfo.value("addmarkers").toInt();
    if (results.isEmpty()) {
        emit displayMessage(i18n("No data returned from clip analysis"), KMessageWidget::Warning);
        return;
    }
    bool dataProcessed = false;
    QString key = filterInfo.value("key");
    int offset = filterInfo.value("offset").toInt();
    QStringList value = results.value(key).split(';', QString::SkipEmptyParts);
    //qDebug()<<"// RESULT; "<<key<<" = "<<value;
    if (filterInfo.contains("resultmessage")) {
        QString mess = filterInfo.value("resultmessage");
        mess.replace("%count", QString::number(value.count()));
        emit displayMessage(mess, KMessageWidget::Information);
    }
    else emit displayMessage(i18n("Processing data analysis"), KMessageWidget::Information);
    if (filterInfo.contains("cutscenes")) {
        // Check if we want to cut scenes from returned data
        dataProcessed = true;
        int cutPos = 0;
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Auto Split Clip"));
        foreach (const QString &pos, value) {
            if (!pos.contains("=")) continue;
            int newPos = pos.section('=', 0, 0).toInt();
            // Don't use scenes shorter than 1 second
            if (newPos - cutPos < 24) continue;
            new AddBinClipCutCommand(this, id, cutPos + offset, newPos + offset, true, command);
            cutPos = newPos;
        }
        if (command->childCount() == 0)
            delete command;
        else m_doc->commandStack()->push(command);
    }
    if (markersType >= 0) {
        // Add markers from returned data
        dataProcessed = true;
        int cutPos = 0;
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Add Markers"));
        QList <CommentedTime> markersList;
        int index = 1;
        foreach (const QString &pos, value) {
            if (!pos.contains("=")) continue;
            int newPos = pos.section('=', 0, 0).toInt();
            // Don't use scenes shorter than 1 second
            if (newPos - cutPos < 24) continue;
            CommentedTime m(GenTime(newPos + offset, m_doc->fps()), QString::number(index), markersType);
            markersList << m;
            index++;
            cutPos = newPos;
        }
        slotAddClipMarker(id, markersList);
    }
    if (!dataProcessed || filterInfo.contains("storedata")) {
        // Store returned data as clip extra data
        QStringList newValue = clip->updatedAnalysisData(key, results.value(key), filterInfo.value("offset").toInt());
        slotAddClipExtraData(id, newValue.at(0), newValue.at(1));
    }
}

void Bin::slotAddClipMarker(const QString &id, QList <CommentedTime> newMarkers, QUndoCommand *groupCommand)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    if (groupCommand == NULL) {
        groupCommand = new QUndoCommand;
        groupCommand->setText(i18np("Add marker", "Add markers", newMarkers.count()));
    }
    clip->addClipMarker(newMarkers, groupCommand);
    if (groupCommand->childCount() > 0) m_doc->commandStack()->push(groupCommand);
    else delete groupCommand;
}

void Bin::slotLoadClipMarkers(const QString &id)
{
    KComboBox *cbox = new KComboBox;
    for (int i = 0; i < 5; ++i) {
        cbox->insertItem(i, i18n("Category %1", i));
        cbox->setItemData(i, CommentedTime::markerColor(i), Qt::DecorationRole);
    }
    cbox->setCurrentIndex(KdenliveSettings::default_marker_type());
    //TODO KF5 how to add custom cbox to Qfiledialog
    QPointer<QFileDialog> fd = new QFileDialog(this, i18n("Load Clip Markers"), m_doc->projectFolder().path());
    fd->setMimeTypeFilters(QStringList()<<"text/plain");
    fd->setFileMode(QFileDialog::ExistingFile);
    if (fd->exec() != QDialog::Accepted) return;
    QStringList selection = fd->selectedFiles();
    QString url;
    if (!selection.isEmpty()) url = selection.first();
    delete fd;

    //QUrl url = KFileDialog::getOpenUrl(QUrl("kfiledialog:///projectfolder"), "text/plain", this, i18n("Load marker file"));
    if (url.isEmpty()) return;
    int category = cbox->currentIndex();
    delete cbox;
    QFile file(url);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit displayMessage(i18n("Cannot open file %1", QUrl(url).fileName()), KMessageWidget::Warning);
        return;
    }
    QString data = QString::fromUtf8(file.readAll());
    file.close();
    QStringList lines = data.split('\n', QString::SkipEmptyParts);
    QStringList values;
    bool ok;
    QUndoCommand *command = new QUndoCommand();
    command->setText("Load markers");
    QString markerText;
    QList <CommentedTime> markersList;
    foreach(const QString &line, lines) {
        markerText.clear();
        values = line.split('\t', QString::SkipEmptyParts);
        double time1 = values.at(0).toDouble(&ok);
        double time2 = -1;
        if (!ok) continue;
        if (values.count() >1) {
            time2 = values.at(1).toDouble(&ok);
            if (values.count() == 2) {
                // Check if second value is a number or text
                if (!ok) {
                    time2 = -1;
                    markerText = values.at(1);
                }
                else markerText = i18n("Marker");
            }
            else {
                // We assume 3 values per line: in out name
                if (!ok) {
                    // 2nd value is not a number, drop
                }
                else {
                    markerText = values.at(2);
                }
            }
        }
        if (!markerText.isEmpty()) {
            // Marker found, add it
            //TODO: allow user to set a marker category
            CommentedTime marker1(GenTime(time1), markerText, category);
            markersList << marker1;
            if (time2 > 0 && time2 != time1) {
                CommentedTime marker2(GenTime(time2), markerText, category);
                markersList << marker2;
            }
        }
    }
    if (!markersList.isEmpty()) slotAddClipMarker(id, markersList, command);
    if (command->childCount() > 0) m_doc->commandStack()->push(command);
    else delete command;
}

void Bin::slotSaveClipMarkers(const QString &id)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    QList < CommentedTime > markers = clip->commentedSnapMarkers();
    if (!markers.isEmpty()) {
        // Set  up categories
        KComboBox *cbox = new KComboBox;
        cbox->insertItem(0, i18n("All categories"));
        for (int i = 0; i < 5; ++i) {
            cbox->insertItem(i + 1, i18n("Category %1", i));
            cbox->setItemData(i + 1, CommentedTime::markerColor(i), Qt::DecorationRole);
        }
        cbox->setCurrentIndex(0);
        //TODO KF5 how to add custom cbox to Qfiledialog
        QPointer<QFileDialog> fd = new QFileDialog(this, i18n("Save Clip Markers"), m_doc->projectFolder().path());
        fd->setMimeTypeFilters(QStringList() << "text/plain");
        fd->setFileMode(QFileDialog::AnyFile);
        fd->setAcceptMode(QFileDialog::AcceptSave);
        if (fd->exec() != QDialog::Accepted) return;
        QStringList selection = fd->selectedFiles();
        QString url;
        if (!selection.isEmpty()) url = selection.first();
        delete fd;
        //QString url = KFileDialog::getSaveFileName(QUrl("kfiledialog:///projectfolder"), "text/plain", this, i18n("Save markers"));
        if (url.isEmpty()) return;

        QString data;
        int category = cbox->currentIndex() - 1;
        for (int i = 0; i < markers.count(); ++i) {
            if (category >= 0) {
                // Save only the markers in selected category
                if (markers.at(i).markerType() != category) continue;
            }
            data.append(QString::number(markers.at(i).time().seconds()));
            data.append("\t");
            data.append(QString::number(markers.at(i).time().seconds()));
            data.append("\t");
            data.append(markers.at(i).comment());
            data.append("\n");
        }
        delete cbox;

        QFile file(url);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit displayMessage(i18n("Cannot open file %1", url), ErrorMessage);
            return;
        }
        file.write(data.toUtf8());
        file.close();
    }
}

void Bin::deleteClipMarker(const QString &comment, const QString &id, const GenTime &position)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    QUndoCommand *command = new QUndoCommand;
    command->setText(i18n("Delete marker"));
    CommentedTime marker(position, comment);
    marker.setMarkerType(-1);
    QList <CommentedTime> markers;
    markers << marker;
    clip->addClipMarker(markers, command);
    if (command->childCount() > 0) m_doc->commandStack()->push(command);
    else delete command;
}

void Bin::deleteAllClipMarkers(const QString &id)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) return;
    QUndoCommand *command = new QUndoCommand;
    command->setText(i18n("Delete clip markers"));
    if (!clip->deleteClipMarkers(command)) {
        emit displayMessage(i18n("Clip has no markers"), KMessageWidget::Warning);
    }
    if (command->childCount() > 0) m_doc->commandStack()->push(command);
    else delete command;
}

// TODO: move title editing into a better place...
void Bin::showTitleWidget(ProjectClip *clip)
{
    QString path = clip->getProducerProperty("resource");
    QString titlepath = m_doc->projectFolder().path() + QDir::separator() + "titles/";
    QPointer<TitleWidget> dia_ui = new TitleWidget(QUrl(), m_doc->timecode(), titlepath, pCore->monitorManager()->projectMonitor()->render, pCore->window());
        QDomDocument doc;
        doc.setContent(clip->getProducerProperty("xmldata"));
        dia_ui->setXml(doc);
        if (dia_ui->exec() == QDialog::Accepted) {
            QMap <QString, QString> newprops;
            newprops.insert("xmldata", dia_ui->xml().toString());
            if (dia_ui->duration() != clip->duration().frames(m_doc->fps())) {
                // duration changed, we need to update duration
                newprops.insert("out", QString::number(dia_ui->duration() - 1));
                int currentLength = clip->getProducerIntProperty("length");
                if (currentLength <= dia_ui->duration()) {
                    newprops.insert("length", QString::number(dia_ui->duration()));
                } else {
                    newprops.insert("length", clip->getProducerProperty("length"));
                }
            }
            // trigger producer reload
            newprops.insert("force_reload", "2");
            if (!path.isEmpty()) {
                // we are editing an external file, asked if we want to detach from that file or save the result to that title file.
                if (KMessageBox::questionYesNo(pCore->window(), i18n("You are editing an external title clip (%1). Do you want to save your changes to the title file or save the changes for this project only?", path), i18n("Save Title"), KGuiItem(i18n("Save to title file")), KGuiItem(i18n("Save in project only"))) == KMessageBox::Yes) {
                    // save to external file
                    dia_ui->saveTitle(QUrl::fromLocalFile(path));
                } else {
                    newprops.insert("resource", QString());
                }
            }
            slotEditClipCommand(clip->clipId(), clip->currentProperties(newprops), newprops);
        }
        delete dia_ui;
}

void Bin::slotResetInfoMessage()
{
    m_errorLog.clear();
    QList<QAction *> actions = m_infoMessage->actions();
    for (int i = 0; i < actions.count(); ++i) {
        m_infoMessage->removeAction(actions.at(i));
    }
}

void Bin::emitMessage(const QString &text, MessageType type)
{
    emit displayMessage(text, type);
}

void Bin::slotCreateAudioThumb(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    clip->createAudioThumbs();
}

void Bin::slotAbortAudioThumb(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    clip->abortAudioThumbs();
}

void Bin::slotSetSorting()
{
    QTreeView *view = static_cast<QTreeView*>(m_itemView);
    if (view) {
        int ix = view->header()->sortIndicatorSection();
        m_proxyModel->setFilterKeyColumn(ix);
    }
}

void Bin::slotShowDateColumn(bool show)
{
    QTreeView *view = static_cast<QTreeView*>(m_itemView);
    if (view) {
        view->setColumnHidden(1, !show);
    }
}

void Bin::slotShowDescColumn(bool show)
{
    QTreeView *view = static_cast<QTreeView*>(m_itemView);
    if (view) {
        view->setColumnHidden(2, !show);
    }
}

void Bin::slotQueryRemoval(const QString &id, QUrl url)
{
    if (m_invalidClipDialog) {
        if (!url.isEmpty()) m_invalidClipDialog->addClip(id, url.toLocalFile());
        return;
    }
    m_invalidClipDialog = new InvalidDialog(i18n("Invalid clip"),  i18n("Clip is invalid, will be removed from project."), true, this);
    m_invalidClipDialog->addClip(id, url.toLocalFile());
    int result = m_invalidClipDialog->exec();
    if (result == QDialog::Accepted) {
        QStringList ids = m_invalidClipDialog->getIds();
        foreach(const QString &i, ids) {
            deleteClip(i);
        }
    }
    delete m_invalidClipDialog;
    m_invalidClipDialog = NULL;
}

void Bin::slotRefreshClipThumbnail(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    clip->reloadProducer(true);
}

void Bin::slotAddClipExtraData(const QString &id, const QString &key, const QString &data, QUndoCommand *groupCommand)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    QString oldValue = clip->getProducerProperty(key);
    QMap <QString, QString> oldProps;
    oldProps.insert(key, oldValue);
    QMap <QString, QString> newProps;
    newProps.insert(key, data);
    EditClipCommand *command = new EditClipCommand(this, id, oldProps, newProps, true, groupCommand);
    if (!groupCommand) m_doc->commandStack()->push(command);
}

void Bin::slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties, bool refreshPropertiesPanel)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setProperties(properties, refreshPropertiesPanel);
    }
}

void Bin::updateTimelineProducers(const QString &id, QMap <QString, QString> passProperties)
{
    pCore->projectManager()->currentTimeline()->updateClipProperties(id, passProperties);
    m_doc->renderer()->updateSlowMotionProducers(id, passProperties);
}

void Bin::showSlideshowWidget(ProjectClip *clip)
{
    QString folder = clip->url().adjusted(QUrl::RemoveFilename).path();
    SlideshowClip *dia = new SlideshowClip(m_doc->timecode(), folder, clip, this);
    if (dia->exec() == QDialog::Accepted) {
        // edit clip properties
        QMap <QString, QString> properties;
        properties.insert("out", QString::number(m_doc->getFramePos(dia->clipDuration()) * dia->imageCount() - 1));
        properties.insert("length", QString::number(m_doc->getFramePos(dia->clipDuration()) * dia->imageCount()));
        properties.insert("kdenlive:clipname", dia->clipName());
        properties.insert("ttl", QString::number(m_doc->getFramePos(dia->clipDuration())));
        properties.insert("loop", QString::number(dia->loop()));
        properties.insert("crop", QString::number(dia->crop()));
        properties.insert("fade", QString::number(dia->fade()));
        properties.insert("luma_duration", dia->lumaDuration());
        properties.insert("luma_file", dia->lumaFile());
        properties.insert("softness", QString::number(dia->softness()));
        properties.insert("animation", dia->animation());

        QMap <QString, QString> oldProperties;
        oldProperties.insert("out", clip->getProducerProperty("out"));
        oldProperties.insert("length", clip->getProducerProperty("length"));
        oldProperties.insert("kdenlive:clipname", clip->name());
        oldProperties.insert("ttl", clip->getProducerProperty("ttl"));
        oldProperties.insert("loop", clip->getProducerProperty("loop"));
        oldProperties.insert("crop", clip->getProducerProperty("crop"));
        oldProperties.insert("fade", clip->getProducerProperty("fade"));
        oldProperties.insert("luma_duration", clip->getProducerProperty("luma_duration"));
        oldProperties.insert("luma_file", clip->getProducerProperty("luma_file"));
        oldProperties.insert("softness", clip->getProducerProperty("softness"));
        oldProperties.insert("animation", clip->getProducerProperty("animation"));
        slotEditClipCommand(clip->clipId(), oldProperties, properties);
    }
}
