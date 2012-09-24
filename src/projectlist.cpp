/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "projectlist.h"
#include "projectitem.h"
#include "commands/addfoldercommand.h"
#include "projecttree/proxyclipjob.h"
#include "projecttree/cutclipjob.h"
#include "projecttree/meltjob.h"
#include "kdenlivesettings.h"
#include "slideshowclip.h"
#include "ui_colorclip_ui.h"
#include "titlewidget.h"
#include "definitions.h"
#include "clipmanager.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "kthumb.h"
#include "projectlistview.h"
#include "timecodedisplay.h"
#include "profilesdialog.h"
#include "clipstabilize.h"
#include "commands/editclipcommand.h"
#include "commands/editclipcutcommand.h"
#include "commands/editfoldercommand.h"
#include "commands/addclipcutcommand.h"

#include "ui_templateclip_ui.h"
#include "ui_cutjobdialog_ui.h"

#include <KDebug>
#include <KAction>
#include <KLocale>
#include <KFileDialog>
#include <KInputDialog>
#include <KMessageBox>
#include <KIO/NetAccess>
#include <KFileItem>
#include <KApplication>
#include <KStandardDirs>
#include <KColorScheme>
#include <KActionCollection>
#include <KUrlRequester>
#include <KVBox>
#include <KHBox>

#ifdef USE_NEPOMUK
#include <nepomuk/global.h>
#include <nepomuk/resourcemanager.h>
#include <Nepomuk/Resource>
//#include <nepomuk/tag.h>
#endif

#include <QMouseEvent>
#include <QStylePainter>
#include <QPixmap>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QScrollBar>
#include <QHeaderView>
#include <QInputDialog>
#include <QtConcurrentRun>
#include <QVBoxLayout>
#include <KPassivePopup>


MyMessageWidget::MyMessageWidget(QWidget *parent) : KMessageWidget(parent) {}
MyMessageWidget::MyMessageWidget(const QString &text, QWidget *parent) : KMessageWidget(text, parent) {}


bool MyMessageWidget::event(QEvent* ev) {
    if (ev->type() == QEvent::Hide || ev->type() == QEvent::Close) emit messageClosing();
    return KMessageWidget::event(ev);
}

SmallInfoLabel::SmallInfoLabel(QWidget *parent) : QPushButton(parent)
{
    setFixedWidth(0);
    setFlat(true);
    
    /*QString style = "QToolButton {background-color: %1;border-style: outset;border-width: 2px;
     border-radius: 5px;border-color: beige;}";*/
    m_timeLine = new QTimeLine(500, this);
    QObject::connect(m_timeLine, SIGNAL(valueChanged(qreal)), this, SLOT(slotTimeLineChanged(qreal)));
    QObject::connect(m_timeLine, SIGNAL(finished()), this, SLOT(slotTimeLineFinished()));
    hide();
}

const QString SmallInfoLabel::getStyleSheet(const QPalette &p)
{
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor bg = scheme.background(KColorScheme::LinkBackground).color();
    QColor fg = scheme.foreground(KColorScheme::LinkText).color();
    QString style = QString("QPushButton {padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}").arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(fg.red()).arg(fg.green()).arg(fg.blue());
    
    bg = scheme.background(KColorScheme::ActiveBackground).color();
    fg = scheme.foreground(KColorScheme::ActiveText).color();
    style.append(QString("\nQPushButton:hover {padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}").arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(fg.red()).arg(fg.green()).arg(fg.blue()));
    
    return style;
}

void SmallInfoLabel::slotTimeLineChanged(qreal value)
{
    setFixedWidth(qMin(value * 2, qreal(1.0)) * sizeHint().width());
    update();
}

void SmallInfoLabel::slotTimeLineFinished()
{
    if (m_timeLine->direction() == QTimeLine::Forward) {
        // Show
        show();
    } else {
        // Hide
        hide();
        setText(QString());
    }
}

void SmallInfoLabel::slotSetJobCount(int jobCount)
{
    if (jobCount > 0) {
        // prepare animation
        setText(i18np("%1 job", "%1 jobs", jobCount));
        setToolTip(i18np("%1 pending job", "%1 pending jobs", jobCount));
        
        if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
            setFixedWidth(sizeHint().width());
            show();
            return;
        }
        
        if (isVisible()) {
            setFixedWidth(sizeHint().width());
            update();
            return;
        }
        
        setFixedWidth(0);
        show();
        int wantedWidth = sizeHint().width();
        setGeometry(-wantedWidth, 0, wantedWidth, height());
        m_timeLine->setDirection(QTimeLine::Forward);
        if (m_timeLine->state() == QTimeLine::NotRunning) {
            m_timeLine->start();
        }
    }
    else {
        if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
            setFixedWidth(0);
            hide();
            return;
        }
        // hide
        m_timeLine->setDirection(QTimeLine::Backward);
        if (m_timeLine->state() == QTimeLine::NotRunning) {
            m_timeLine->start();
        }
    }
    
}


InvalidDialog::InvalidDialog(const QString &caption, const QString &message, bool infoOnly, QWidget *parent) : KDialog(parent)
{
    setCaption(caption);
    if (infoOnly) setButtons(KDialog::Ok);
    else setButtons(KDialog::Yes | KDialog::No);
    QWidget *w = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout;
    l->addWidget(new QLabel(message));
    m_clipList = new QListWidget;
    l->addWidget(m_clipList);
    w->setLayout(l);
    setMainWidget(w);
}

InvalidDialog::~InvalidDialog()
{
    delete m_clipList;
}


void InvalidDialog::addClip(const QString &id, const QString &path)
{
    QListWidgetItem *item = new QListWidgetItem(path);
    item->setData(Qt::UserRole, id);
    m_clipList->addItem(item);
}

QStringList InvalidDialog::getIds() const
{
    QStringList ids;
    for (int i = 0; i < m_clipList->count(); i++) {
        ids << m_clipList->item(i)->data(Qt::UserRole).toString();
    }
    return ids;
}


ProjectList::ProjectList(QWidget *parent) :
    QWidget(parent),
    m_render(NULL),
    m_fps(-1),
    m_commandStack(NULL),
    m_openAction(NULL),
    m_reloadAction(NULL),
    m_extractAudioAction(NULL),
    m_transcodeAction(NULL),
    m_stabilizeAction(NULL),
    m_doc(NULL),
    m_refreshed(false),
    m_allClipsProcessed(false),
    m_thumbnailQueue(),
    m_abortAllJobs(false),
    m_closing(false),
    m_invalidClipDialog(NULL)
{
    qRegisterMetaType<stringMap> ("stringMap");
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    qRegisterMetaType<QDomElement>("QDomElement");
    // setup toolbar
    QFrame *frame = new QFrame;
    frame->setFrameStyle(QFrame::NoFrame);
    QHBoxLayout *box = new QHBoxLayout;
    box->setContentsMargins(0, 0, 0, 0);
    
    KTreeWidgetSearchLine *searchView = new KTreeWidgetSearchLine;
    box->addWidget(searchView);
    
    // small info button for pending jobs
    m_infoLabel = new SmallInfoLabel(this);
    m_infoLabel->setStyleSheet(SmallInfoLabel::getStyleSheet(palette()));
    connect(this, SIGNAL(jobCount(int)), m_infoLabel, SLOT(slotSetJobCount(int)));
    m_jobsMenu = new QMenu(this);
    connect(m_jobsMenu, SIGNAL(aboutToShow()), this, SLOT(slotPrepareJobsMenu()));
    QAction *cancelJobs = new QAction(i18n("Cancel All Jobs"), this);
    cancelJobs->setCheckable(false);
    connect(cancelJobs, SIGNAL(triggered()), this, SLOT(slotCancelJobs()));
    connect(this, SIGNAL(checkJobProcess()), this, SLOT(slotCheckJobProcess()));
    m_discardCurrentClipJobs = new QAction(i18n("Cancel Current Clip Jobs"), this);
    m_discardCurrentClipJobs->setCheckable(false);
    connect(m_discardCurrentClipJobs, SIGNAL(triggered()), this, SLOT(slotDiscardClipJobs()));
    m_jobsMenu->addAction(cancelJobs);
    m_jobsMenu->addAction(m_discardCurrentClipJobs);
    m_infoLabel->setMenu(m_jobsMenu);
    box->addWidget(m_infoLabel);
       
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

    m_addButton = new QToolButton;
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_addButton->setAutoRaise(true);
    m_addButton->setIconSize(iconSize);
    box->addWidget(m_addButton);

    m_editButton = new QToolButton;
    m_editButton->setAutoRaise(true);
    m_editButton->setIconSize(iconSize);
    box->addWidget(m_editButton);

    m_deleteButton = new QToolButton;
    m_deleteButton->setAutoRaise(true);
    m_deleteButton->setIconSize(iconSize);
    box->addWidget(m_deleteButton);
    frame->setLayout(box);
    layout->addWidget(frame);

    m_listView = new ProjectListView(this);
    layout->addWidget(m_listView);
    
#if KDE_IS_VERSION(4,7,0)
    m_infoMessage = new MyMessageWidget;
    layout->addWidget(m_infoMessage);
    m_infoMessage->setCloseButtonVisible(true);
    connect(m_infoMessage, SIGNAL(messageClosing()), this, SLOT(slotResetInfoMessage()));
    //m_infoMessage->setWordWrap(true);
    m_infoMessage->hide();
    m_logAction = new QAction(i18n("Show Log"), this);
    m_logAction->setCheckable(false);
    connect(m_logAction, SIGNAL(triggered()), this, SLOT(slotShowJobLog()));
#endif

    setLayout(layout);
    searchView->setTreeWidget(m_listView);

    connect(this, SIGNAL(processNextThumbnail()), this, SLOT(slotProcessNextThumbnail()));
    connect(m_listView, SIGNAL(projectModified()), this, SIGNAL(projectModified()));
    connect(m_listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
    connect(m_listView, SIGNAL(focusMonitor()), this, SIGNAL(raiseClipMonitor()));
    connect(m_listView, SIGNAL(pauseMonitor()), this, SLOT(slotPauseMonitor()));
    connect(m_listView, SIGNAL(requestMenu(const QPoint &, QTreeWidgetItem *)), this, SLOT(slotContextMenu(const QPoint &, QTreeWidgetItem *)));
    connect(m_listView, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    connect(m_listView, SIGNAL(addClip(const QList <QUrl>, const QString &, const QString &)), this, SLOT(slotAddClip(const QList <QUrl>, const QString &, const QString &)));
    connect(this, SIGNAL(addClip(const QString, const QString &, const QString &)), this, SLOT(slotAddClip(const QString, const QString &, const QString &)));
    connect(m_listView, SIGNAL(addClipCut(const QString &, int, int)), this, SLOT(slotAddClipCut(const QString &, int, int)));
    connect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));
    connect(m_listView, SIGNAL(showProperties(DocClipBase *)), this, SIGNAL(showClipProperties(DocClipBase *)));
    
    connect(this, SIGNAL(cancelRunningJob(const QString, stringMap )), this, SLOT(slotCancelRunningJob(const QString, stringMap)));
    connect(this, SIGNAL(processLog(const QString, int , int, const QString)), this, SLOT(slotProcessLog(const QString, int , int, const QString)));
    
    connect(this, SIGNAL(updateJobStatus(const QString, int, int, const QString, const QString, const QString)), this, SLOT(slotUpdateJobStatus(const QString, int, int, const QString, const QString, const QString)));
    
    connect(this, SIGNAL(gotProxy(const QString)), this, SLOT(slotGotProxyForId(const QString)));
    
    m_listViewDelegate = new ItemDelegate(m_listView);
    m_listView->setItemDelegate(m_listViewDelegate);
#ifdef USE_NEPOMUK
    if (KdenliveSettings::activate_nepomuk()) {
        Nepomuk::ResourceManager::instance()->init();
        if (!Nepomuk::ResourceManager::instance()->initialized()) {
            kDebug() << "Cannot communicate with Nepomuk, DISABLING it";
            KdenliveSettings::setActivate_nepomuk(false);
        }
    }
#endif
}

ProjectList::~ProjectList()
{
    m_abortAllJobs = true;
    for (int i = 0; i < m_jobList.count(); i++) {
        m_jobList.at(i)->setStatus(JOBABORTED);
    }
    m_closing = true;
    m_thumbnailQueue.clear();
    m_jobThreads.waitForFinished();
    m_jobThreads.clearFutures();
    if (!m_jobList.isEmpty()) qDeleteAll(m_jobList);
    m_jobList.clear();
    delete m_menu;
    m_listView->blockSignals(true);
    m_listView->clear();
    delete m_listViewDelegate;
#if KDE_IS_VERSION(4,7,0)
    delete m_infoMessage;
#endif
}

void ProjectList::focusTree() const
{
    m_listView->setFocus();
}

void ProjectList::setupMenu(QMenu *addMenu, QAction *defaultAction)
{
    QList <QAction *> actions = addMenu->actions();
    for (int i = 0; i < actions.count(); i++) {
        if (actions.at(i)->data().toString() == "clip_properties") {
            m_editButton->setDefaultAction(actions.at(i));
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "delete_clip") {
            m_deleteButton->setDefaultAction(actions.at(i));
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "edit_clip") {
            m_openAction = actions.at(i);
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "reload_clip") {
            m_reloadAction = actions.at(i);
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "proxy_clip") {
            m_proxyAction = actions.at(i);
            actions.removeAt(i);
            i--;
        }
    }

    QMenu *m = new QMenu();
    m->addActions(actions);
    m_addButton->setMenu(m);
    m_addButton->setDefaultAction(defaultAction);
    m_menu = new QMenu();
    m_menu->addActions(addMenu->actions());
}

void ProjectList::setupGeneratorMenu(const QHash<QString,QMenu*>& menus)
{
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
	if (menus.contains("stabilizeMenu") && menus.value("stabilizeMenu") ){
		QMenu* stabilizeMenu=menus.value("stabilizeMenu");
		m_menu->addMenu(stabilizeMenu);
		if (stabilizeMenu->isEmpty())
			stabilizeMenu->setEnabled(false);
		m_stabilizeAction=stabilizeMenu;

	}
    m_menu->addAction(m_reloadAction);
    m_menu->addAction(m_proxyAction);
	if (menus.contains("inTimelineMenu") && menus.value("inTimelineMenu")){
		QMenu* inTimelineMenu=menus.value("inTimelineMenu");
		m_menu->addMenu(inTimelineMenu);
		inTimelineMenu->setEnabled(false);
	}
    m_menu->addAction(m_editButton->defaultAction());
    m_menu->addAction(m_openAction);
    m_menu->addAction(m_deleteButton->defaultAction());
    m_menu->insertSeparator(m_deleteButton->defaultAction());
}

void ProjectList::clearSelection()
{
    m_listView->clearSelection();
}

QByteArray ProjectList::headerInfo() const
{
    return m_listView->header()->saveState();
}

void ProjectList::setHeaderInfo(const QByteArray &state)
{
    m_listView->header()->restoreState(state);
}

void ProjectList::updateProjectFormat(Timecode t)
{
    m_timecode = t;
}

void ProjectList::slotEditClip()
{
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    if (list.isEmpty()) return;
    if (list.count() > 1 || list.at(0)->type() == PROJECTFOLDERTYPE) {
        editClipSelection(list);
        return;
    }
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE)
        return;
    if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE)
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    else
        item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item && (item->flags() & Qt::ItemIsDragEnabled)) {
        emit clipSelected(item->referencedClip());
        emit showClipProperties(item->referencedClip());
    }
}

void ProjectList::editClipSelection(QList<QTreeWidgetItem *> list)
{
    // Gather all common properties
    QMap <QString, QString> commonproperties;
    QList <DocClipBase *> clipList;
    commonproperties.insert("force_aspect_num", "-");
    commonproperties.insert("force_aspect_den", "-");
    commonproperties.insert("force_fps", "-");
    commonproperties.insert("force_progressive", "-");
    commonproperties.insert("force_tff", "-");
    commonproperties.insert("threads", "-");
    commonproperties.insert("video_index", "-");
    commonproperties.insert("audio_index", "-");
    commonproperties.insert("force_colorspace", "-");
    commonproperties.insert("full_luma", "-");
    QString transparency = "-";

    bool allowDurationChange = true;
    int commonDuration = -1;
    bool hasImages = false;;
    ProjectItem *item;
    for (int i = 0; i < list.count(); i++) {
        item = NULL;
        if (list.at(i)->type() == PROJECTFOLDERTYPE) {
            // Add folder items to the list
            int ct = list.at(i)->childCount();
            for (int j = 0; j < ct; j++) {
                list.append(list.at(i)->child(j));
            }
            continue;
        }
        else if (list.at(i)->type() == PROJECTSUBCLIPTYPE)
            item = static_cast <ProjectItem*>(list.at(i)->parent());
        else
            item = static_cast <ProjectItem*>(list.at(i));
        if (!(item->flags() & Qt::ItemIsDragEnabled))
            continue;
        if (item) {
            // check properties
            DocClipBase *clip = item->referencedClip();
            if (clipList.contains(clip)) continue;
            if (clip->clipType() == IMAGE) {
                hasImages = true;
                if (clip->getProperty("transparency").isEmpty() || clip->getProperty("transparency").toInt() == 0) {
                    if (transparency == "-") {
                        // first non transparent image
                        transparency = '0';
                    }
                    else if (transparency == "1") {
                        // we have transparent and non transparent clips
                        transparency = "-1";
                    }
                }
                else {
                    if (transparency == "-") {
                        // first transparent image
                        transparency = '1';
                    }
                    else if (transparency == "0") {
                        // we have transparent and non transparent clips
                        transparency = "-1";
                    }
                }
            }
            if (clip->clipType() != COLOR && clip->clipType() != IMAGE && clip->clipType() != TEXT)
                allowDurationChange = false;
            if (allowDurationChange && commonDuration != 0) {
                if (commonDuration == -1)
                    commonDuration = clip->duration().frames(m_fps);
                else if (commonDuration != clip->duration().frames(m_fps))
                    commonDuration = 0;
            }
            clipList.append(clip);
            QMap <QString, QString> clipprops = clip->properties();
            QMapIterator<QString, QString> p(commonproperties);
            while (p.hasNext()) {
                p.next();
                if (p.value().isEmpty()) continue;
                if (clipprops.contains(p.key())) {
                    if (p.value() == "-")
                        commonproperties.insert(p.key(), clipprops.value(p.key()));
                    else if (p.value() != clipprops.value(p.key()))
                        commonproperties.insert(p.key(), QString());
                } else {
                    commonproperties.insert(p.key(), QString());
                }
            }
        }
    }
    if (allowDurationChange)
        commonproperties.insert("out", QString::number(commonDuration));
    if (hasImages)
        commonproperties.insert("transparency", transparency);
    /*QMapIterator<QString, QString> p(commonproperties);
    while (p.hasNext()) {
        p.next();
        kDebug() << "Result: " << p.key() << " = " << p.value();
    }*/
    if (clipList.isEmpty()) {
        emit displayMessage(i18n("No available clip selected"), -2);        
    }
    else emit showClipProperties(clipList, commonproperties);
}

void ProjectList::slotOpenClip()
{
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE)
        return;
    if (m_listView->currentItem()->type() == QTreeWidgetItem::UserType + 1)
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    else
        item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item) {
        if (item->clipType() == IMAGE) {
            if (KdenliveSettings::defaultimageapp().isEmpty())
                KMessageBox::sorry(kapp->activeWindow(), i18n("Please set a default application to open images in the Settings dialog"));
            else
                QProcess::startDetached(KdenliveSettings::defaultimageapp(), QStringList() << item->clipUrl().path());
        }
        if (item->clipType() == AUDIO) {
            if (KdenliveSettings::defaultaudioapp().isEmpty())
                KMessageBox::sorry(kapp->activeWindow(), i18n("Please set a default application to open audio files in the Settings dialog"));
            else
                QProcess::startDetached(KdenliveSettings::defaultaudioapp(), QStringList() << item->clipUrl().path());
        }
    }
}

void ProjectList::cleanup()
{
    m_listView->clearSelection();
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            it++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it);
        if (item->numReferences() == 0)
            item->setSelected(true);
        it++;
    }
    slotRemoveClip();
}

void ProjectList::trashUnusedClips()
{
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;
    QStringList ids;
    QStringList urls;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            it++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it);
        if (item->numReferences() == 0) {
            ids << item->clipId();
            KUrl url = item->clipUrl();
            if (!url.isEmpty() && !urls.contains(url.path()))
                urls << url.path();
        }
        it++;
    }

    // Check that we don't use the URL in another clip
    QTreeWidgetItemIterator it2(m_listView);
    while (*it2) {
        if ((*it2)->type() != PROJECTCLIPTYPE) {
            it2++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it2);
        if (item->numReferences() > 0) {
            KUrl url = item->clipUrl();
            if (!url.isEmpty() && urls.contains(url.path())) urls.removeAll(url.path());
        }
        it2++;
    }

    emit deleteProjectClips(ids, QMap <QString, QString>());
    for (int i = 0; i < urls.count(); i++)
        KIO::NetAccess::del(KUrl(urls.at(i)), this);
}

void ProjectList::slotReloadClip(const QString &id)
{
    QList<QTreeWidgetItem *> selected;
    if (id.isEmpty())
        selected = m_listView->selectedItems();
    else {
        ProjectItem *itemToReLoad = getItemById(id);
        if (itemToReLoad) selected.append(itemToReLoad);
    }
    ProjectItem *item;
    for (int i = 0; i < selected.count(); i++) {
        if (selected.at(i)->type() != PROJECTCLIPTYPE) {
            if (selected.at(i)->type() == PROJECTFOLDERTYPE) {
                for (int j = 0; j < selected.at(i)->childCount(); j++)
                    selected.append(selected.at(i)->child(j));
            }
            continue;
        }
        item = static_cast <ProjectItem *>(selected.at(i));
        if (item && !hasPendingJob(item, PROXYJOB)) {
            DocClipBase *clip = item->referencedClip();
            if (!clip || !clip->isClean() || m_render->isProcessing(item->clipId())) {
                kDebug()<<"//// TRYING TO RELOAD: "<<item->clipId()<<", but it is busy";
                continue;
            }
            CLIPTYPE t = item->clipType();
            if (t == TEXT) {
                if (clip && !clip->getProperty("xmltemplate").isEmpty())
                    regenerateTemplate(item);
            } else if (t != COLOR && t != SLIDESHOW && clip && clip->checkHash() == false) {
                item->referencedClip()->setPlaceHolder(true);
                item->setProperty("file_hash", QString());
            } else if (t == IMAGE) {
                //clip->getProducer() clip->getProducer()->set("force_reload", 1);
            }

            QDomElement e = item->toXml();
            // Make sure we get the correct producer length if it was adjusted in timeline
            if (t == COLOR || t == IMAGE || t == SLIDESHOW || t == TEXT) {
                int length = QString(clip->producerProperty("length")).toInt();
                if (length > 0 && !e.hasAttribute("length")) {
                    e.setAttribute("length", length);
                }
            }
            resetThumbsProducer(clip);
            m_render->getFileProperties(e, item->clipId(), m_listView->iconSize().height(), true);
        }
    }
}

void ProjectList::slotModifiedClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        QPixmap pixmap = qVariantValue<QPixmap>(item->data(0, Qt::DecorationRole));
        if (!pixmap.isNull()) {
            QPainter p(&pixmap);
            p.fillRect(0, 0, pixmap.width(), pixmap.height(), QColor(255, 255, 255, 200));
            p.drawPixmap(0, 0, KIcon("view-refresh").pixmap(m_listView->iconSize()));
            p.end();
        } else {
            pixmap = KIcon("view-refresh").pixmap(m_listView->iconSize());
        }
        item->setData(0, Qt::DecorationRole, pixmap);
    }
}

void ProjectList::slotMissingClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
        int height = m_listView->iconSize().height();
        if (m_render == NULL) {
            kDebug() << "*********  ERROR, NULL RENDR";
            return;
        }
        int width = (int)(height  * m_render->dar());
        QPixmap pixmap = qVariantValue<QPixmap>(item->data(0, Qt::DecorationRole));
        if (pixmap.isNull()) {
            pixmap = QPixmap(width, height);
            pixmap.fill(Qt::transparent);
        }
        KIcon icon("dialog-close");
        QPainter p(&pixmap);
        p.drawPixmap(3, 3, icon.pixmap(width - 6, height - 6));
        p.end();
        item->setData(0, Qt::DecorationRole, pixmap);
        if (item->referencedClip()) {
            item->referencedClip()->setPlaceHolder(true);
            Mlt::Producer *newProd = m_render->invalidProducer(id);
            if (item->referencedClip()->getProducer()) {
                Mlt::Properties props(newProd->get_properties());
                Mlt::Properties src_props(item->referencedClip()->getProducer()->get_properties());
                props.inherit(src_props);
            }
            item->referencedClip()->setProducer(newProd, true);
            item->slotSetToolTip();
            emit clipNeedsReload(id);
        }
    }
    update();
    emit displayMessage(i18n("Check missing clips"), -2);
    emit updateRenderStatus();
}

void ProjectList::slotAvailableClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item == NULL)
        return;
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
    if (item->referencedClip()) { // && item->referencedClip()->checkHash() == false) {
        item->setProperty("file_hash", QString());
        slotReloadClip(id);
    }
    /*else {
    item->referencedClip()->setValid();
    item->slotSetToolTip();
    }
    update();*/
    emit updateRenderStatus();
}

bool ProjectList::hasMissingClips()
{
    bool missing = false;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() == PROJECTCLIPTYPE && !((*it)->flags() & Qt::ItemIsDragEnabled)) {
            missing = true;
            break;
        }
        it++;
    }
    return missing;
}

void ProjectList::setRenderer(Render *projectRender)
{
    m_render = projectRender;
    m_listView->setIconSize(QSize((ProjectItem::itemDefaultHeight() - 2) * m_render->dar(), ProjectItem::itemDefaultHeight() - 2));
    connect(m_render, SIGNAL(requestProxy(QString)), this, SLOT(slotCreateProxy(QString)));
}

void ProjectList::slotClipSelected()
{
    QTreeWidgetItem *item = m_listView->currentItem();
    ProjectItem *clip = NULL;
    if (item) {
        if (item->type() == PROJECTFOLDERTYPE) {
            emit clipSelected(NULL);
            m_editButton->defaultAction()->setEnabled(item->childCount() > 0);
            m_deleteButton->defaultAction()->setEnabled(true);
            m_openAction->setEnabled(false);
            m_reloadAction->setEnabled(false);
            m_extractAudioAction->setEnabled(false);
            m_transcodeAction->setEnabled(false);
            m_stabilizeAction->setEnabled(false);
        } else {
            if (item->type() == PROJECTSUBCLIPTYPE) {
                // this is a sub item, use base clip
                m_deleteButton->defaultAction()->setEnabled(true);
                clip = static_cast <ProjectItem*>(item->parent());
                if (clip == NULL) kDebug() << "-----------ERROR";
                SubProjectItem *sub = static_cast <SubProjectItem*>(item);
                emit clipSelected(clip->referencedClip(), sub->zone());
                m_extractAudioAction->setEnabled(false);
                m_transcodeAction->setEnabled(false);
                m_stabilizeAction->setEnabled(false);
                m_reloadAction->setEnabled(false);
                adjustProxyActions(clip);
                return;
            }
            clip = static_cast <ProjectItem*>(item);
            if (clip && clip->referencedClip())
                emit clipSelected(clip->referencedClip());
            m_editButton->defaultAction()->setEnabled(true);
            m_deleteButton->defaultAction()->setEnabled(true);
            m_reloadAction->setEnabled(true);
            m_extractAudioAction->setEnabled(true);
            m_transcodeAction->setEnabled(true);
            m_stabilizeAction->setEnabled(true);
            if (clip && clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
                m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
                m_openAction->setEnabled(true);
            } else if (clip && clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
                m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
                m_openAction->setEnabled(true);
            } else {
                m_openAction->setEnabled(false);
            }
            // Display relevant transcoding actions only
            adjustTranscodeActions(clip);
            adjustStabilizeActions(clip);
            // Display uses in timeline
            emit findInTimeline(clip->clipId());
        }
    } else {
        emit clipSelected(NULL);
        m_editButton->defaultAction()->setEnabled(false);
        m_deleteButton->defaultAction()->setEnabled(false);
        m_openAction->setEnabled(false);
        m_reloadAction->setEnabled(false);
        m_extractAudioAction->setEnabled(true);
        m_transcodeAction->setEnabled(false);
        m_stabilizeAction->setEnabled(false);
    }
    adjustProxyActions(clip);
}

void ProjectList::adjustProxyActions(ProjectItem *clip) const
{
    if (clip == NULL || clip->type() != PROJECTCLIPTYPE || clip->clipType() == COLOR || clip->clipType() == TEXT || clip->clipType() == SLIDESHOW || clip->clipType() == AUDIO) {
        m_proxyAction->setEnabled(false);
        return;
    }
    bool enabled = useProxy();
    if (clip->referencedClip() && !clip->referencedClip()->getProperty("_missingsource").isEmpty()) enabled = false;
    m_proxyAction->setEnabled(enabled);
    m_proxyAction->blockSignals(true);
    m_proxyAction->setChecked(clip->hasProxy());
    m_proxyAction->blockSignals(false);
}

void ProjectList::adjustStabilizeActions(ProjectItem *clip) const
{

    if (clip == NULL || clip->type() != PROJECTCLIPTYPE || clip->clipType() == COLOR || clip->clipType() == TEXT || clip->clipType() == PLAYLIST || clip->clipType() == SLIDESHOW) {
        m_stabilizeAction->setEnabled(false);
        return;
    }
	m_stabilizeAction->setEnabled(true);

}

void ProjectList::adjustTranscodeActions(ProjectItem *clip) const
{
    if (clip == NULL || clip->type() != PROJECTCLIPTYPE || clip->clipType() == COLOR || clip->clipType() == TEXT || clip->clipType() == PLAYLIST || clip->clipType() == SLIDESHOW) {
        m_transcodeAction->setEnabled(false);
        m_extractAudioAction->setEnabled(false);
        return;
    }
    m_transcodeAction->setEnabled(true);
    m_extractAudioAction->setEnabled(true);
    QList<QAction *> transcodeActions = m_transcodeAction->actions();
    QStringList data;
    QString condition;
    for (int i = 0; i < transcodeActions.count(); i++) {
        data = transcodeActions.at(i)->data().toStringList();
        if (data.count() > 2) {
            condition = data.at(2);
            if (condition.startsWith("vcodec"))
                transcodeActions.at(i)->setEnabled(clip->referencedClip()->hasVideoCodec(condition.section('=', 1, 1)));
            else if (condition.startsWith("acodec"))
                transcodeActions.at(i)->setEnabled(clip->referencedClip()->hasVideoCodec(condition.section('=', 1, 1)));
        }
    }

}

void ProjectList::slotPauseMonitor()
{
    if (m_render)
        m_render->pause();
}

void ProjectList::slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        slotUpdateClipProperties(item, properties);
        if (properties.contains("out") || properties.contains("force_fps") || properties.contains("resource") || properties.contains("video_index") || properties.contains("audio_index")) {
            slotReloadClip(id);
        } else if (properties.contains("colour") ||
                   properties.contains("xmldata") ||
                   properties.contains("force_aspect_num") ||
                   properties.contains("force_aspect_den") ||
                   properties.contains("templatetext")) {
            slotRefreshClipThumbnail(item);
            emit refreshClip(id, true);
        } else if (properties.contains("full_luma") || properties.contains("force_colorspace") || properties.contains("loop")) {
            emit refreshClip(id, false);
        }
    }
}

void ProjectList::slotUpdateClipProperties(ProjectItem *clip, QMap <QString, QString> properties)
{
    if (!clip)
        return;
    clip->setProperties(properties);
    if (properties.contains("proxy")) {
        if (properties.value("proxy") == "-" || properties.value("proxy").isEmpty())
            // this should only apply to proxy jobs
            clip->setConditionalJobStatus(NOJOB, PROXYJOB);
    }
    if (properties.contains("name")) {
        monitorItemEditing(false);
        clip->setText(0, properties.value("name"));
        monitorItemEditing(true);
        emit clipNameChanged(clip->clipId(), properties.value("name"));
    }
    if (properties.contains("description")) {
        CLIPTYPE type = clip->clipType();
        monitorItemEditing(false);
        clip->setText(1, properties.value("description"));
        monitorItemEditing(true);
#ifdef USE_NEPOMUK
        if (KdenliveSettings::activate_nepomuk() && (type == AUDIO || type == VIDEO || type == AV || type == IMAGE || type == PLAYLIST)) {
            // Use Nepomuk system to store clip description
            Nepomuk::Resource f(clip->clipUrl().path());
            f.setDescription(properties.value("description"));
        }
#endif
        emit projectModified();
    }
}

void ProjectList::slotItemEdited(QTreeWidgetItem *item, int column)
{
    if (item->type() == PROJECTSUBCLIPTYPE) {
        // this is a sub-item
        if (column == 1) {
            // user edited description
            SubProjectItem *sub = static_cast <SubProjectItem*>(item);
            ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
            EditClipCutCommand *command = new EditClipCutCommand(this, item->clipId(), sub->zone(), sub->zone(), sub->description(), sub->text(1), true);
            m_commandStack->push(command);
            //slotUpdateCutClipProperties(sub->clipId(), sub->zone(), sub->text(1), sub->text(1));
        }
        return;
    }
    if (item->type() == PROJECTFOLDERTYPE) {
        if (column == 0) {
            FolderProjectItem *folder = static_cast <FolderProjectItem*>(item);
            editFolder(item->text(0), folder->groupName(), folder->clipId());
            folder->setGroupName(item->text(0));
            m_doc->clipManager()->addFolder(folder->clipId(), item->text(0));
            const int children = item->childCount();
            for (int i = 0; i < children; i++) {
                ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                child->setProperty("groupname", item->text(0));
            }
        }
        return;
    }

    ProjectItem *clip = static_cast <ProjectItem*>(item);
    if (column == 1) {
        if (clip->referencedClip()) {
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            oldprops["description"] = clip->referencedClip()->getProperty("description");
            newprops["description"] = item->text(1);

            if (clip->clipType() == TEXT) {
                // This is a text template clip, update the image
                /*oldprops.insert("xmldata", clip->referencedClip()->getProperty("xmldata"));
                newprops.insert("xmldata", generateTemplateXml(clip->referencedClip()->getProperty("xmltemplate"), item->text(2)).toString());*/
                oldprops.insert("templatetext", clip->referencedClip()->getProperty("templatetext"));
                newprops.insert("templatetext", item->text(1));
            }
            slotUpdateClipProperties(clip->clipId(), newprops);
            EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
            m_commandStack->push(command);
        }
    } else if (column == 0) {
        if (clip->referencedClip()) {
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            oldprops["name"] = clip->referencedClip()->getProperty("name");
            if (oldprops.value("name") != item->text(0)) {
                newprops["name"] = item->text(0);
                slotUpdateClipProperties(clip, newprops);
                emit projectModified();
                EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
                m_commandStack->push(command);
		QTimer::singleShot(100, this, SLOT(slotCheckScrolling()));
            }
        }
    }
}

void ProjectList::slotCheckScrolling()
{
    m_listView->scrollToItem(m_listView->currentItem());
}

void ProjectList::slotContextMenu(const QPoint &pos, QTreeWidgetItem *item)
{
    bool enable = item ? true : false;
    m_editButton->defaultAction()->setEnabled(enable);
    m_deleteButton->defaultAction()->setEnabled(enable);
    m_reloadAction->setEnabled(enable);
    m_extractAudioAction->setEnabled(enable);
    m_transcodeAction->setEnabled(enable);
    m_stabilizeAction->setEnabled(enable);
    if (enable) {
        ProjectItem *clip = NULL;
        if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
            clip = static_cast <ProjectItem*>(item->parent());
            m_extractAudioAction->setEnabled(false);
            m_transcodeAction->setEnabled(false);
            m_stabilizeAction->setEnabled(false);
            adjustProxyActions(clip);
        } else if (m_listView->currentItem()->type() == PROJECTCLIPTYPE) {
            clip = static_cast <ProjectItem*>(item);
            // Display relevant transcoding actions only
            adjustTranscodeActions(clip);
            adjustStabilizeActions(clip);
            adjustProxyActions(clip);
            // Display uses in timeline
            emit findInTimeline(clip->clipId());
        } else {
            m_extractAudioAction->setEnabled(false);
            m_transcodeAction->setEnabled(false);
            m_stabilizeAction->setEnabled(false);
        }
        if (clip && clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip && clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else {
            m_openAction->setEnabled(false);
        }

    } else {
        m_openAction->setEnabled(false);
    }
    m_menu->popup(pos);
}

void ProjectList::slotRemoveClip()
{
    if (!m_listView->currentItem())
        return;
    QStringList ids;
    QMap <QString, QString> folderids;
    QList<QTreeWidgetItem *> selected = m_listView->selectedItems();

    QUndoCommand *delCommand = new QUndoCommand();
    delCommand->setText(i18n("Delete Clip Zone"));
    for (int i = 0; i < selected.count(); i++) {
        if (selected.at(i)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            SubProjectItem *sub = static_cast <SubProjectItem *>(selected.at(i));
            ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
            new AddClipCutCommand(this, item->clipId(), sub->zone().x(), sub->zone().y(), sub->description(), false, true, delCommand);
        } else if (selected.at(i)->type() == PROJECTFOLDERTYPE) {
            // folder
            FolderProjectItem *folder = static_cast <FolderProjectItem *>(selected.at(i));
            folderids[folder->groupName()] = folder->clipId();
            int children = folder->childCount();

            if (children > 0 && KMessageBox::questionYesNo(kapp->activeWindow(), i18np("Delete folder <b>%2</b>?<br />This will also remove the clip in that folder", "Delete folder <b>%2</b>?<br />This will also remove the %1 clips in that folder",  children, folder->text(1)), i18n("Delete Folder")) != KMessageBox::Yes)
                return;
            for (int i = 0; i < children; ++i) {
                ProjectItem *child = static_cast <ProjectItem *>(folder->child(i));
                ids << child->clipId();
            }
        } else {
            ProjectItem *item = static_cast <ProjectItem *>(selected.at(i));
            ids << item->clipId();
            if (item->numReferences() > 0 && KMessageBox::questionYesNo(kapp->activeWindow(), i18np("Delete clip <b>%2</b>?<br />This will also remove the clip in timeline", "Delete clip <b>%2</b>?<br />This will also remove its %1 clips in timeline", item->numReferences(), item->text(1)), i18n("Delete Clip"), KStandardGuiItem::yes(), KStandardGuiItem::no(), "DeleteAll") == KMessageBox::No) {
                KMessageBox::enableMessage("DeleteAll");
                return;
            }
        }
    }
    KMessageBox::enableMessage("DeleteAll");
    if (delCommand->childCount() == 0)
        delete delCommand;
    else
        m_commandStack->push(delCommand);
    emit deleteProjectClips(ids, folderids);
}

void ProjectList::updateButtons() const
{
    if (m_listView->topLevelItemCount() == 0) {
        m_deleteButton->defaultAction()->setEnabled(false);
        m_editButton->defaultAction()->setEnabled(false);
    } else {
        m_deleteButton->defaultAction()->setEnabled(true);
        if (!m_listView->currentItem())
            m_listView->setCurrentItem(m_listView->topLevelItem(0));
        QTreeWidgetItem *item = m_listView->currentItem();
        if (item && item->type() == PROJECTCLIPTYPE) {
            m_editButton->defaultAction()->setEnabled(true);
            m_openAction->setEnabled(true);
            m_reloadAction->setEnabled(true);
            m_transcodeAction->setEnabled(true);
            m_stabilizeAction->setEnabled(true);
            return;
        }
        else if (item && item->type() == PROJECTFOLDERTYPE && item->childCount() > 0) {
            m_editButton->defaultAction()->setEnabled(true);
        }
        else m_editButton->defaultAction()->setEnabled(false);
    }
    m_openAction->setEnabled(false);
    m_reloadAction->setEnabled(false);
    m_transcodeAction->setEnabled(false);
    m_stabilizeAction->setEnabled(false);
    m_proxyAction->setEnabled(false);
}

void ProjectList::selectItemById(const QString &clipId)
{
    ProjectItem *item = getItemById(clipId);
    if (item)
        m_listView->setCurrentItem(item);
}


void ProjectList::slotDeleteClip(const QString &clipId)
{
    ProjectItem *item = getItemById(clipId);
    if (!item) {
        kDebug() << "/// Cannot find clip to delete";
        return;
    }
    deleteJobsForClip(clipId);
    m_listView->blockSignals(true);
    QTreeWidgetItem *newSelectedItem = m_listView->itemAbove(item);
    if (!newSelectedItem)
        newSelectedItem = m_listView->itemBelow(item);
    delete item;
    // Pause playing to prevent crash while deleting clip
    slotPauseMonitor();
    m_doc->clipManager()->deleteClip(clipId);
    m_listView->blockSignals(false);
    if (newSelectedItem) {
        m_listView->setCurrentItem(newSelectedItem);
    } else {
        updateButtons();
        emit clipSelected(NULL);
    }
}


void ProjectList::editFolder(const QString folderName, const QString oldfolderName, const QString &clipId)
{
    EditFolderCommand *command = new EditFolderCommand(this, folderName, oldfolderName, clipId, false);
    m_commandStack->push(command);
    m_doc->setModified(true);
}

void ProjectList::slotAddFolder(const QString &name)
{
    AddFolderCommand *command = new AddFolderCommand(this, name.isEmpty() ? i18n("Folder") : name, QString::number(m_doc->clipManager()->getFreeFolderId()), true);
    m_commandStack->push(command);
}

void ProjectList::slotAddFolder(const QString foldername, const QString &clipId, bool remove, bool edit)
{
    if (remove) {
        FolderProjectItem *item = getFolderItemById(clipId);
        if (item) {
            m_doc->clipManager()->deleteFolder(clipId);
            QTreeWidgetItem *newSelectedItem = m_listView->itemAbove(item);
            if (!newSelectedItem)
                newSelectedItem = m_listView->itemBelow(item);
            delete item;
            if (newSelectedItem)
                m_listView->setCurrentItem(newSelectedItem);
            else
                updateButtons();
        }
    } else {
        if (edit) {
            FolderProjectItem *item = getFolderItemById(clipId);
            if (item) {
                m_listView->blockSignals(true);
                item->setGroupName(foldername);
                m_listView->blockSignals(false);
                m_doc->clipManager()->addFolder(clipId, foldername);
                const int children = item->childCount();
                for (int i = 0; i < children; i++) {
                    ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                    child->setProperty("groupname", foldername);
                }
            }
        } else {
            m_listView->blockSignals(true);
            m_listView->setCurrentItem(new FolderProjectItem(m_listView, QStringList() << foldername, clipId));
            m_doc->clipManager()->addFolder(clipId, foldername);
            m_listView->blockSignals(false);
            m_listView->editItem(m_listView->currentItem(), 0);
        }
        updateButtons();
    }
    m_doc->setModified(true);
}



void ProjectList::deleteProjectFolder(QMap <QString, QString> map)
{
    QMapIterator<QString, QString> i(map);
    QUndoCommand *delCommand = new QUndoCommand();
    delCommand->setText(i18n("Delete Folder"));
    while (i.hasNext()) {
        i.next();
        new AddFolderCommand(this, i.key(), i.value(), false, delCommand);
    }
    if (delCommand->childCount() > 0) m_commandStack->push(delCommand);
    else delete delCommand;
}

void ProjectList::slotAddClip(DocClipBase *clip, bool getProperties)
{
    //m_listView->setEnabled(false);
    const QString parent = clip->getProperty("groupid");
    QString groupName = clip->getProperty("groupname");
    ProjectItem *item = NULL;
    monitorItemEditing(false);
    if (!parent.isEmpty()) {
        FolderProjectItem *parentitem = getFolderItemById(parent);
        if (!parentitem) {
            QStringList text;
            //kDebug() << "Adding clip to new group: " << groupName;
            if (groupName.isEmpty()) groupName = i18n("Folder");
            text << groupName;
            parentitem = new FolderProjectItem(m_listView, text, parent);
        }

        if (parentitem)
            item = new ProjectItem(parentitem, clip);
    }
    if (item == NULL) {
        item = new ProjectItem(m_listView, clip);
    }
    if (item->data(0, DurationRole).isNull()) item->setData(0, DurationRole, i18n("Loading"));
    connect(clip, SIGNAL(createProxy(const QString &)), this, SLOT(slotCreateProxy(const QString &)));
    connect(clip, SIGNAL(abortProxy(const QString &, const QString &)), this, SLOT(slotAbortProxy(const QString, const QString)));
    if (getProperties) {
        int height = m_listView->iconSize().height();
        int width = (int)(height  * m_render->dar());
        QPixmap pix =  KIcon("video-x-generic").pixmap(QSize(width, height));
        item->setData(0, Qt::DecorationRole, pix);
        //item->setFlags(Qt::ItemIsSelectable);
        m_listView->processLayout();
        QDomElement e = clip->toXML().cloneNode().toElement();
	if (!groupName.isEmpty()) {
	    e.setAttribute("groupId", parent);
	    e.setAttribute("group", groupName);
	}
        e.removeAttribute("file_hash");
        resetThumbsProducer(clip);
        m_render->getFileProperties(e, clip->getId(), m_listView->iconSize().height(), true);
    }
    // WARNING: code below triggers unnecessary reload of all proxy clips on document loading... is it useful in some cases?
    /*else if (item->hasProxy() && !item->isJobRunning()) {
        slotCreateProxy(clip->getId());
    }*/
    
    KUrl url = clip->fileURL();
#ifdef USE_NEPOMUK
    if (!url.isEmpty() && KdenliveSettings::activate_nepomuk() && clip->getProperty("description").isEmpty()) {
        // if file has Nepomuk comment, use it
        Nepomuk::Resource f(url.path());
        QString annotation = f.description();
        if (!annotation.isEmpty()) {
            item->setText(1, annotation);
            clip->setProperty("description", annotation);
        }
        item->setText(2, QString::number(f.rating()));
    }
#endif

    // Add info to date column
    QFileInfo fileInfo(url.path());
    if (fileInfo.exists()) {
    	item->setText(3, fileInfo.lastModified().toString(QString("yyyy/MM/dd hh:mm:ss")));
    }

    // Add cut zones
    QList <CutZoneInfo> cuts = clip->cutZones();
    if (!cuts.isEmpty()) {
        for (int i = 0; i < cuts.count(); i++) {
            SubProjectItem *sub = new SubProjectItem(item, cuts.at(i).zone.x(), cuts.at(i).zone.y(), cuts.at(i).description);
            if (!clip->getClipHash().isEmpty()) {
                QString cachedPixmap = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "thumbs/" + clip->getClipHash() + '#' + QString::number(cuts.at(i).zone.x()) + ".png";
                if (QFile::exists(cachedPixmap)) {
                    QPixmap pix(cachedPixmap);
                    if (pix.isNull())
                        KIO::NetAccess::del(KUrl(cachedPixmap), this);
                    sub->setData(0, Qt::DecorationRole, pix);
                }
            }
        }
    }
    monitorItemEditing(true);
    updateButtons();
}

void ProjectList::slotGotProxy(const QString &proxyPath)
{
    if (proxyPath.isEmpty() || m_abortAllJobs) return;
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;

    while (*it && !m_closing) {
        if ((*it)->type() == PROJECTCLIPTYPE) {
            item = static_cast <ProjectItem *>(*it);
            if (item->referencedClip()->getProperty("proxy") == proxyPath)
                slotGotProxy(item);
        }
        ++it;
    }
}

void ProjectList::slotGotProxyForId(const QString id)
{
    if (m_closing) return;
    ProjectItem *item = getItemById(id);
    slotGotProxy(item);
}

void ProjectList::slotGotProxy(ProjectItem *item)
{
    if (item == NULL) return;
    DocClipBase *clip = item->referencedClip();
    if (!clip || !clip->isClean() || m_render->isProcessing(item->clipId())) {
        // Clip is being reprocessed, abort
        kDebug()<<"//// TRYING TO PROXY: "<<item->clipId()<<", but it is busy";
        return;
    }
    
    // Proxy clip successfully created
    QDomElement e = clip->toXML().cloneNode().toElement();

    // Make sure we get the correct producer length if it was adjusted in timeline
    CLIPTYPE t = item->clipType();
    if (t == COLOR || t == IMAGE || t == SLIDESHOW || t == TEXT) {
        int length = QString(clip->producerProperty("length")).toInt();
        if (length > 0 && !e.hasAttribute("length")) {
            e.setAttribute("length", length);
        }
    }
    resetThumbsProducer(clip);
    m_render->getFileProperties(e, clip->getId(), m_listView->iconSize().height(), true);
}

void ProjectList::slotResetProjectList()
{
    m_listView->blockSignals(true);
    m_abortAllJobs = true;
    for (int i = 0; i < m_jobList.count(); i++) {
        m_jobList.at(i)->setStatus(JOBABORTED);
    }
    m_closing = true;
    m_jobThreads.waitForFinished();
    m_jobThreads.clearFutures();
    m_thumbnailQueue.clear();
    if (!m_jobList.isEmpty()) qDeleteAll(m_jobList);
    m_jobList.clear();
    m_listView->clear();
    m_listView->setEnabled(true);
    emit clipSelected(NULL);
    m_refreshed = false;
    m_allClipsProcessed = false;
    m_abortAllJobs = false;
    m_closing = false;
    m_listView->blockSignals(false);
}

void ProjectList::slotUpdateClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    monitorItemEditing(false);
    if (item) item->setData(0, UsageRole, QString::number(item->numReferences()));
    monitorItemEditing(true);
}

void ProjectList::getCachedThumbnail(ProjectItem *item)
{
    if (!item) return;
    DocClipBase *clip = item->referencedClip();
    if (!clip) return;
    QString cachedPixmap = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "thumbs/" + clip->getClipHash() + ".png";
    if (QFile::exists(cachedPixmap)) {
        QPixmap pix(cachedPixmap);
        if (pix.isNull()) {
            KIO::NetAccess::del(KUrl(cachedPixmap), this);
            requestClipThumbnail(item->clipId());
        }
        else {
            processThumbOverlays(item, pix);
            item->setData(0, Qt::DecorationRole, pix);
        }
    }
    else {
        requestClipThumbnail(item->clipId());
    }
}

void ProjectList::getCachedThumbnail(SubProjectItem *item)
{
    if (!item) return;
    ProjectItem *parentItem = static_cast <ProjectItem *>(item->parent());
    if (!parentItem) return;
    DocClipBase *clip = parentItem->referencedClip();
    if (!clip) return;
    int pos = item->zone().x();
    QString cachedPixmap = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "thumbs/" + clip->getClipHash() + '#' + QString::number(pos) + ".png";
    if (QFile::exists(cachedPixmap)) {
        QPixmap pix(cachedPixmap);
        if (pix.isNull()) {
            KIO::NetAccess::del(KUrl(cachedPixmap), this);
            requestClipThumbnail(parentItem->clipId() + '#' + QString::number(pos));
        }
        else item->setData(0, Qt::DecorationRole, pix);
    }
    else requestClipThumbnail(parentItem->clipId() + '#' + QString::number(pos));
}

void ProjectList::updateAllClips(bool displayRatioChanged, bool fpsChanged, QStringList brokenClips)
{
    if (!m_allClipsProcessed) m_listView->setEnabled(false);
    m_listView->setSortingEnabled(false);
    QTreeWidgetItemIterator it(m_listView);
    DocClipBase *clip;
    ProjectItem *item;
    monitorItemEditing(false);
    int height = m_listView->iconSize().height();
    int width = (int)(height  * m_render->dar());
    QPixmap missingPixmap = QPixmap(width, height);
    missingPixmap.fill(Qt::transparent);
    KIcon icon("dialog-close");
    QPainter p(&missingPixmap);
    p.drawPixmap(3, 3, icon.pixmap(width - 6, height - 6));
    p.end();
    
    int max = m_doc->clipManager()->clipsCount();
    max = qMax(1, max);
    int ct = 0;

    while (*it) {
        emit displayMessage(i18n("Loading thumbnails"), (int)(100 *(max - ct++) / max));
        if ((*it)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            SubProjectItem *sub = static_cast <SubProjectItem *>(*it);
            if (displayRatioChanged) {
                item = static_cast <ProjectItem *>((*it)->parent());
                requestClipThumbnail(item->clipId() + '#' + QString::number(sub->zone().x()));
            }
            else if (sub->data(0, Qt::DecorationRole).isNull()) {
                getCachedThumbnail(sub);
            }
            ++it;
            continue;
        } else if ((*it)->type() == PROJECTFOLDERTYPE) {
            // folder
            ++it;
            continue;
        } else {
            item = static_cast <ProjectItem *>(*it);
            clip = item->referencedClip();
            if (clip->getProducer() == NULL) {
                bool replace = false;
                if (brokenClips.contains(item->clipId())) {
                    // if this is a proxy clip, disable proxy
                    item->setConditionalJobStatus(NOJOB, PROXYJOB);
                    discardJobs(item->clipId(), PROXYJOB);
                    clip->setProperty("proxy", "-");
                    replace = true;
                }
                if (clip->isPlaceHolder() == false && !hasPendingJob(item, PROXYJOB)) {
                    QDomElement xml = clip->toXML();
                    if (fpsChanged) {
                        xml.removeAttribute("out");
                        xml.removeAttribute("file_hash");
                        xml.removeAttribute("proxy_out");
                    }
                    if (!replace) replace = xml.attribute("_replaceproxy") == "1";
                    xml.removeAttribute("_replaceproxy");
                    if (replace) {
                        resetThumbsProducer(clip);
                    }
                    m_render->getFileProperties(xml, clip->getId(), m_listView->iconSize().height(), replace);
                }
                else if (clip->isPlaceHolder()) {
                    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
                    if (item->data(0, Qt::DecorationRole).isNull()) {
                        item->setData(0, Qt::DecorationRole, missingPixmap);
                    }
                    else {
                        QPixmap pixmap = qVariantValue<QPixmap>(item->data(0, Qt::DecorationRole));
                        QPainter p(&pixmap);
                        p.drawPixmap(3, 3, KIcon("dialog-close").pixmap(pixmap.width() - 6, pixmap.height() - 6));
                        p.end();
                        item->setData(0, Qt::DecorationRole, pixmap);
                    }
                }
            } else {              
                if (displayRatioChanged) {
                    requestClipThumbnail(clip->getId());
                }
                else if (item->data(0, Qt::DecorationRole).isNull()) {
                    getCachedThumbnail(item);
                }
                if (item->data(0, DurationRole).toString().isEmpty()) {
                    item->changeDuration(clip->getProducer()->get_playtime());
                }
                if (clip->isPlaceHolder()) {
                    QPixmap pixmap = qVariantValue<QPixmap>(item->data(0, Qt::DecorationRole));
                    if (pixmap.isNull()) {
                        pixmap = QPixmap(width, height);
                        pixmap.fill(Qt::transparent);
                    }
                    QPainter p(&pixmap);
                    p.drawPixmap(3, 3, KIcon("dialog-close").pixmap(pixmap.width() - 6, pixmap.height() - 6));
                    p.end();
                    item->setData(0, Qt::DecorationRole, pixmap);
                }
                else if (clip->getProperty("_replaceproxy") == "1") {
                    clip->setProperty("_replaceproxy", QString());
                    slotCreateProxy(clip->getId());
                }
            }
            item->setData(0, UsageRole, QString::number(item->numReferences()));
        }
        ++it;
    }

    m_listView->setSortingEnabled(true);
    m_allClipsProcessed = true;
    if (m_render->processingItems() == 0) {
       monitorItemEditing(true);
       slotProcessNextThumbnail();
    }
}

// static
QString ProjectList::getExtensions()
{
    // Build list of mime types
    QStringList mimeTypes = QStringList() << "application/x-kdenlive" << "application/x-kdenlivetitle" << "video/mlt-playlist" << "text/plain";

    // Video mimes
    mimeTypes <<  "video/x-flv" << "application/vnd.rn-realmedia" << "video/x-dv" << "video/dv" << "video/x-msvideo" << "video/x-matroska" << "video/mpeg" << "video/ogg" << "video/x-ms-wmv" << "video/mp4" << "video/quicktime" << "video/webm" << "video/3gpp";

    // Audio mimes
    mimeTypes << "audio/x-flac" << "audio/x-matroska" << "audio/mp4" << "audio/mpeg" << "audio/x-mp3" << "audio/ogg" << "audio/x-wav" << "audio/x-aiff" << "audio/aiff" << "application/ogg" << "application/mxf" << "application/x-shockwave-flash" << "audio/ac3";

    // Image mimes
    mimeTypes << "image/gif" << "image/jpeg" << "image/png" << "image/x-tga" << "image/x-bmp" << "image/svg+xml" << "image/tiff" << "image/x-xcf" << "image/x-xcf-gimp" << "image/x-vnd.adobe.photoshop" << "image/x-pcx" << "image/x-exr";

    QString allExtensions;
    foreach(const QString & mimeType, mimeTypes) {
        KMimeType::Ptr mime(KMimeType::mimeType(mimeType));
        if (mime) {
            allExtensions.append(mime->patterns().join(" "));
            allExtensions.append(' ');
        }
    }
    return allExtensions.simplified();
}

void ProjectList::slotAddClip(const QString url, const QString &groupName, const QString &groupId)
{
    kDebug()<<"// Adding clip: "<<url;
    QList <QUrl> list;
    list.append(url);
    slotAddClip(list, groupName, groupId);
}

void ProjectList::slotAddClip(const QList <QUrl> givenList, const QString &groupName, const QString &groupId)
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    KUrl::List list;
    if (givenList.isEmpty()) {
        QString allExtensions = getExtensions();
        const QString dialogFilter = allExtensions + ' ' + QLatin1Char('|') + i18n("All Supported Files") + "\n* " + QLatin1Char('|') + i18n("All Files");
        QCheckBox *b = new QCheckBox(i18n("Import image sequence"));
        b->setChecked(KdenliveSettings::autoimagesequence());
        QCheckBox *c = new QCheckBox(i18n("Transparent background for images"));
        c->setChecked(KdenliveSettings::autoimagetransparency());
        QFrame *f = new QFrame;
        f->setFrameShape(QFrame::NoFrame);
        QHBoxLayout *l = new QHBoxLayout;
        l->addWidget(b);
        l->addWidget(c);
        l->addStretch(5);
        f->setLayout(l);
        QPointer<KFileDialog> d = new KFileDialog(KUrl("kfiledialog:///clipfolder"), dialogFilter, kapp->activeWindow(), f);
        d->setOperationMode(KFileDialog::Opening);
        d->setMode(KFile::Files);
        if (d->exec() == QDialog::Accepted) {
            KdenliveSettings::setAutoimagetransparency(c->isChecked());
        }
        list = d->selectedUrls();
        if (b->isChecked() && list.count() == 1) {
            // Check for image sequence
            KUrl url = list.at(0);
            QString fileName = url.fileName().section('.', 0, -2);
            if (fileName.at(fileName.size() - 1).isDigit()) {
                KFileItem item(KFileItem::Unknown, KFileItem::Unknown, url);
                if (item.mimetype().startsWith("image")) {
                    // import as sequence if we found more than one image in the sequence
                    QStringList list;
                    QString pattern = SlideshowClip::selectedPath(url.path(), false, QString(), &list);
                    int count = list.count();
                    if (count > 1) {
                        delete d;
                        QStringList groupInfo = getGroup();

                        // get image sequence base name
                        while (fileName.at(fileName.size() - 1).isDigit()) {
                            fileName.chop(1);
                        }
                        QMap <QString, QString> properties;
                        properties.insert("name", fileName);
                        properties.insert("resource", pattern);
                        properties.insert("in", "0");
                        QString duration = m_timecode.reformatSeparators(KdenliveSettings::sequence_duration());
                        properties.insert("out", QString::number(m_doc->getFramePos(duration) * count));
                        properties.insert("ttl", QString::number(m_doc->getFramePos(duration)));
                        properties.insert("loop", QString::number(false));
                        properties.insert("crop", QString::number(false));
                        properties.insert("fade", QString::number(false));
                        properties.insert("luma_duration", QString::number(m_doc->getFramePos(m_timecode.getTimecodeFromFrames(int(ceil(m_timecode.fps()))))));
                        m_doc->slotCreateSlideshowClipFile(properties, groupInfo.at(0), groupInfo.at(1));
                        return;
                    }
                }
            }
        }
        delete d;
    } else {
        for (int i = 0; i < givenList.count(); i++)
            list << givenList.at(i);
    }
    QList <KUrl::List> foldersList;

    foreach(const KUrl & file, list) {
        // Check there is no folder here
        KMimeType::Ptr type = KMimeType::findByUrl(file);
        if (type->is("inode/directory")) {
            // user dropped a folder, import its files
            list.removeAll(file);
            QDir dir(file.path());
            QStringList result = dir.entryList(QDir::Files);
            KUrl::List folderFiles;
            folderFiles << file;
            foreach(const QString & path, result) {
                KUrl newFile = file;
                newFile.addPath(path);
                folderFiles.append(newFile);
            }
            if (folderFiles.count() > 1) foldersList.append(folderFiles);
        }
    }

    if (givenList.isEmpty() && !list.isEmpty()) {
        QStringList groupInfo = getGroup();
	QMap <QString, QString> data;
	data.insert("group", groupInfo.at(0));
	data.insert("groupId", groupInfo.at(1));
        m_doc->slotAddClipList(list, data);
    } else if (!list.isEmpty()) {
	QMap <QString, QString> data;
	data.insert("group", groupName);
	data.insert("groupId", groupId);
        m_doc->slotAddClipList(list, data);
    }
    
    if (!foldersList.isEmpty()) {
        // create folders 
        for (int i = 0; i < foldersList.count(); i++) {
            KUrl::List urls = foldersList.at(i);
            KUrl folderUrl = urls.takeFirst();
            QString folderName = folderUrl.fileName();
            FolderProjectItem *folder = NULL;
            if (!folderName.isEmpty()) {
                folder = getFolderItemByName(folderName);
                if (folder == NULL) {
                    slotAddFolder(folderName);
                    folder = getFolderItemByName(folderName);
                }
            }
            if (folder) {
		QMap <QString, QString> data;
		data.insert("group", folder->groupName());
		data.insert("groupId", folder->clipId());
                m_doc->slotAddClipList(urls, data);
	    }
            else m_doc->slotAddClipList(urls);
        }
    }
}

void ProjectList::slotRemoveInvalidClip(const QString &id, bool replace)
{
    ProjectItem *item = getItemById(id);
    m_thumbnailQueue.removeAll(id);
    if (item) {
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
        const QString path = item->referencedClip()->fileURL().path();
        if (item->referencedClip()->isPlaceHolder()) replace = false;
        if (!path.isEmpty()) {
            if (m_invalidClipDialog) {
                m_invalidClipDialog->addClip(id, path);
                return;
            }
            else {
                if (replace)
                    m_invalidClipDialog = new InvalidDialog(i18n("Invalid clip"),  i18n("Clip <b>%1</b><br />is invalid, will be removed from project.", QString()), replace, kapp->activeWindow());
                else {
                    m_invalidClipDialog = new InvalidDialog(i18n("Invalid clip"),  i18n("Clip <b>%1</b><br />is missing or invalid. Remove it from project?", QString()), replace, kapp->activeWindow());
                }
                m_invalidClipDialog->addClip(id, path);
                int result = m_invalidClipDialog->exec();
                if (result == KDialog::Yes) replace = true;
            }
        }
        if (m_invalidClipDialog) {
            if (replace)
                emit deleteProjectClips(m_invalidClipDialog->getIds(), QMap <QString, QString>());
            delete m_invalidClipDialog;
            m_invalidClipDialog = NULL;
        }
        
    }
}

void ProjectList::slotRemoveInvalidProxy(const QString &id, bool durationError)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        kDebug()<<"// Proxy for clip "<<id<<" is invalid, delete";
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
        if (durationError) {
            kDebug() << "Proxy duration is wrong, try changing transcoding parameters.";
            emit displayMessage(i18n("Proxy clip unusable (duration is different from original)."), -2);
        }
        slotUpdateJobStatus(item, PROXYJOB, JOBCRASHED, i18n("Failed to create proxy for %1. check parameters", item->text(0)), "project_settings");
        QString path = item->referencedClip()->getProperty("proxy");
        KUrl proxyFolder(m_doc->projectFolder().path( KUrl::AddTrailingSlash) + "proxy/");

        //Security check: make sure the invalid proxy file is in the proxy folder
        if (proxyFolder.isParentOf(KUrl(path))) {
            QFile::remove(path);
        }
        if (item->referencedClip()->getProducer() == NULL) {
            // Clip has no valid producer, request it
            slotProxyCurrentItem(false, item);
        }
        else {
            // refresh thumbs producer
            item->referencedClip()->reloadThumbProducer();
        }
    }
    m_thumbnailQueue.removeAll(id);
}

void ProjectList::slotAddColorClip()
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    QPointer<QDialog> dia = new QDialog(this);
    Ui::ColorClip_UI dia_ui;
    dia_ui.setupUi(dia);
    dia->setWindowTitle(i18n("Color Clip"));
    dia_ui.clip_name->setText(i18n("Color Clip"));

    TimecodeDisplay *t = new TimecodeDisplay(m_timecode);
    t->setValue(KdenliveSettings::color_duration());
    dia_ui.clip_durationBox->addWidget(t);
    dia_ui.clip_color->setColor(KdenliveSettings::colorclipcolor());

    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui.clip_color->color().name();
        KdenliveSettings::setColorclipcolor(color);
        color = color.replace(0, 1, "0x") + "ff";
        QStringList groupInfo = getGroup();
        m_doc->slotCreateColorClip(dia_ui.clip_name->text(), color, m_timecode.getTimecode(t->gentime()), groupInfo.at(0), groupInfo.at(1));
    }
    delete t;
    delete dia;
}


void ProjectList::slotAddSlideshowClip()
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    SlideshowClip *dia = new SlideshowClip(m_timecode, this);

    if (dia->exec() == QDialog::Accepted) {
        QStringList groupInfo = getGroup();
        
        QMap <QString, QString> properties;
        properties.insert("name", dia->clipName());
        properties.insert("resource", dia->selectedPath());
        properties.insert("in", "0");
        properties.insert("out", QString::number(m_doc->getFramePos(dia->clipDuration()) * dia->imageCount()));
        properties.insert("ttl", QString::number(m_doc->getFramePos(dia->clipDuration())));
        properties.insert("loop", QString::number(dia->loop()));
        properties.insert("crop", QString::number(dia->crop()));
        properties.insert("fade", QString::number(dia->fade()));
        properties.insert("luma_duration", dia->lumaDuration());
        properties.insert("luma_file", dia->lumaFile());
        properties.insert("softness", QString::number(dia->softness()));
        properties.insert("animation", dia->animation());
        
        m_doc->slotCreateSlideshowClipFile(properties, groupInfo.at(0), groupInfo.at(1));
    }
    delete dia;
}

void ProjectList::slotAddTitleClip()
{
    QStringList groupInfo = getGroup();
    m_doc->slotCreateTextClip(groupInfo.at(0), groupInfo.at(1));
}

void ProjectList::slotAddTitleTemplateClip()
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    QStringList groupInfo = getGroup();

    // Get the list of existing templates
    QStringList filter;
    filter << "*.kdenlivetitle";
    const QString path = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
    QStringList templateFiles = QDir(path).entryList(filter, QDir::Files);

    QPointer<QDialog> dia = new QDialog(this);
    Ui::TemplateClip_UI dia_ui;
    dia_ui.setupUi(dia);
    for (int i = 0; i < templateFiles.size(); ++i)
        dia_ui.template_list->comboBox()->addItem(templateFiles.at(i), path + templateFiles.at(i));

    if (!templateFiles.isEmpty())
        dia_ui.buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    dia_ui.template_list->fileDialog()->setFilter("application/x-kdenlivetitle");
    //warning: setting base directory doesn't work??
    KUrl startDir(path);
    dia_ui.template_list->fileDialog()->setUrl(startDir);
    dia_ui.text_box->setHidden(true);
    if (dia->exec() == QDialog::Accepted) {
        QString textTemplate = dia_ui.template_list->comboBox()->itemData(dia_ui.template_list->comboBox()->currentIndex()).toString();
        if (textTemplate.isEmpty()) textTemplate = dia_ui.template_list->comboBox()->currentText();
        // Create a cloned template clip
        m_doc->slotCreateTextTemplateClip(groupInfo.at(0), groupInfo.at(1), KUrl(textTemplate));
    }
    delete dia;
}

QStringList ProjectList::getGroup() const
{
    QStringList result;
    QTreeWidgetItem *item = m_listView->currentItem();
    while (item && item->type() != PROJECTFOLDERTYPE)
        item = item->parent();

    if (item) {
        FolderProjectItem *folder = static_cast <FolderProjectItem *>(item);
        result << folder->groupName() << folder->clipId();
    } else {
        result << QString() << QString();
    }
    return result;
}

void ProjectList::setDocument(KdenliveDoc *doc)
{
    m_listView->blockSignals(true);
    m_abortAllJobs = true;
    for (int i = 0; i < m_jobList.count(); i++) {
        m_jobList.at(i)->setStatus(JOBABORTED);
    }
    m_closing = true;
    m_jobThreads.waitForFinished();
    m_jobThreads.clearFutures();
    m_thumbnailQueue.clear();
    m_listView->clear();
    
    m_listView->setSortingEnabled(false);
    emit clipSelected(NULL);
    m_refreshed = false;
    m_allClipsProcessed = false;
    m_fps = doc->fps();
    m_timecode = doc->timecode();
    m_commandStack = doc->commandStack();
    m_doc = doc;
    m_abortAllJobs = false;
    m_closing = false;

    QMap <QString, QString> flist = doc->clipManager()->documentFolderList();
    QStringList openedFolders = doc->getExpandedFolders();
    QMapIterator<QString, QString> f(flist);
    while (f.hasNext()) {
        f.next();
        FolderProjectItem *folder = new FolderProjectItem(m_listView, QStringList() << f.value(), f.key());
        folder->setExpanded(openedFolders.contains(f.key()));
    }

    QList <DocClipBase*> list = doc->clipManager()->documentClipList();
    if (list.isEmpty()) {
        // blank document
        m_refreshed = true;
        m_allClipsProcessed = true;
    }
    for (int i = 0; i < list.count(); i++)
        slotAddClip(list.at(i), false);

    m_listView->blockSignals(false);
    connect(m_doc->clipManager(), SIGNAL(reloadClip(const QString &)), this, SLOT(slotReloadClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(modifiedClip(const QString &)), this, SLOT(slotModifiedClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(missingClip(const QString &)), this, SLOT(slotMissingClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(availableClip(const QString &)), this, SLOT(slotAvailableClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(checkAllClips(bool, bool, QStringList)), this, SLOT(updateAllClips(bool, bool, QStringList)));
}

QList <DocClipBase*> ProjectList::documentClipList() const
{
    if (m_doc == NULL)
        return QList <DocClipBase*> ();

    return m_doc->clipManager()->documentClipList();
}

QDomElement ProjectList::producersList()
{
    QDomDocument doc;
    QDomElement prods = doc.createElement("producerlist");
    doc.appendChild(prods);
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        prods.appendChild(doc.importNode(((ProjectItem *)(*it))->toXml(), true));
        ++it;
    }
    return prods;
}

void ProjectList::slotCheckForEmptyQueue()
{
    if (m_render->processingItems() == 0 && m_thumbnailQueue.isEmpty()) {
        if (!m_refreshed && m_allClipsProcessed) {
            m_refreshed = true;
            m_listView->setEnabled(true);
            slotClipSelected();
            QTimer::singleShot(500, this, SIGNAL(loadingIsOver()));
            emit displayMessage(QString(), -1);
        }
        updateButtons();
    } else if (!m_refreshed) {
        QTimer::singleShot(300, this, SLOT(slotCheckForEmptyQueue()));
    }
}


void ProjectList::requestClipThumbnail(const QString id)
{
    if (!m_thumbnailQueue.contains(id)) m_thumbnailQueue.append(id);
    slotProcessNextThumbnail();
}

void ProjectList::resetThumbsProducer(DocClipBase *clip)
{
    if (!clip) return;
    clip->clearThumbProducer();
    QString id = clip->getId();
    m_thumbnailQueue.removeAll(id);
}

void ProjectList::slotProcessNextThumbnail()
{
    if (m_render->processingItems() > 0) {
        return;
    }
    if (m_thumbnailQueue.isEmpty()) {
        slotCheckForEmptyQueue();
        return;
    }
    int max = m_doc->clipManager()->clipsCount();
    emit displayMessage(i18n("Loading thumbnails"), (int)(100 *(max - m_thumbnailQueue.count()) / max));
    slotRefreshClipThumbnail(m_thumbnailQueue.takeFirst(), false);
}

void ProjectList::slotRefreshClipThumbnail(const QString &clipId, bool update)
{
    QTreeWidgetItem *item = getAnyItemById(clipId);
    if (item)
        slotRefreshClipThumbnail(item, update);
    else {
        slotProcessNextThumbnail();
    }
}

void ProjectList::slotRefreshClipThumbnail(QTreeWidgetItem *it, bool update)
{
    if (it == NULL) return;
    ProjectItem *item = NULL;
    bool isSubItem = false;
    int frame;
    if (it->type() == PROJECTFOLDERTYPE) return;
    if (it->type() == PROJECTSUBCLIPTYPE) {
        item = static_cast <ProjectItem *>(it->parent());
        frame = static_cast <SubProjectItem *>(it)->zone().x();
        isSubItem = true;
    } else {
        item = static_cast <ProjectItem *>(it);
        frame = item->referencedClip()->getClipThumbFrame();
    }

    if (item) {
        DocClipBase *clip = item->referencedClip();
        if (!clip) {
            slotProcessNextThumbnail();
            return;
        }
        QPixmap pix;
        QImage img;
        int height = m_listView->iconSize().height();
        int swidth = (int)(height  * m_render->frameRenderWidth() / m_render->renderHeight()+ 0.5);
        int dwidth = (int)(height  * m_render->dar() + 0.5);
        if (clip->clipType() == AUDIO)
            pix = KIcon("audio-x-generic").pixmap(QSize(dwidth, height));
        else if (clip->clipType() == IMAGE) {
            img = KThumb::getFrame(item->referencedClip()->getProducer(), 0, swidth, dwidth, height);
	}
        else {
            img = item->referencedClip()->extractImage(frame, dwidth, height);
        }
        if (!pix.isNull() || !img.isNull()) {
            monitorItemEditing(false);
            if (!img.isNull()) {
                pix = QPixmap::fromImage(img);
                processThumbOverlays(item, pix);
            }
            it->setData(0, Qt::DecorationRole, pix);
            monitorItemEditing(true);
            
            QString hash = item->getClipHash();
            if (!hash.isEmpty() && !img.isNull()) {
                if (!isSubItem)
                    m_doc->cacheImage(hash, img);
                else
                    m_doc->cacheImage(hash + '#' + QString::number(frame), img);
            }
        }
        if (update)
            emit projectModified();
        slotProcessNextThumbnail();
    }
}


void ProjectList::slotReplyGetFileProperties(const QString &clipId, Mlt::Producer *producer, const stringMap &properties, const stringMap &metadata, bool replace)
{
    QString toReload;
    ProjectItem *item = getItemById(clipId);
    int queue = m_render->processingItems();
    if (item && producer) {
        monitorItemEditing(false);
        DocClipBase *clip = item->referencedClip();
        if (producer->is_valid()) {
            if (clip->isPlaceHolder()) {
                clip->setValid();
                toReload = clipId;
            }
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
        }
        item->setProperties(properties, metadata);
        clip->setProducer(producer, replace);

        // Proxy stuff
        QString size = properties.value("frame_size");
        if (!useProxy() && clip->getProperty("proxy").isEmpty()) {
            item->setConditionalJobStatus(NOJOB, PROXYJOB);
            discardJobs(clipId, PROXYJOB);
        }
        if (useProxy() && generateProxy() && clip->getProperty("proxy") == "-") {
            item->setConditionalJobStatus(NOJOB, PROXYJOB);
            discardJobs(clipId, PROXYJOB);
        }
        else if (useProxy() && !item->hasProxy() && !hasPendingJob(item, PROXYJOB)) {
            // proxy video and image clips
            int maxSize;
            CLIPTYPE t = item->clipType();
            if (t == IMAGE) maxSize = m_doc->getDocumentProperty("proxyimageminsize").toInt();
            else maxSize = m_doc->getDocumentProperty("proxyminsize").toInt();
            if ((((t == AV || t == VIDEO || t == PLAYLIST) && generateProxy()) || (t == IMAGE && generateImageProxy())) && (size.section('x', 0, 0).toInt() > maxSize || size.section('x', 1, 1).toInt() > maxSize)) {
                if (clip->getProperty("proxy").isEmpty()) {
                    KUrl proxyPath = m_doc->projectFolder();
                    proxyPath.addPath("proxy/");
                    proxyPath.addPath(clip->getClipHash() + '.' + (t == IMAGE ? "png" : m_doc->getDocumentProperty("proxyextension")));
                    QMap <QString, QString> newProps;
                    // insert required duration for proxy
                    if (t != IMAGE) newProps.insert("proxy_out", clip->producerProperty("out"));
                    newProps.insert("proxy", proxyPath.path());
                    QMap <QString, QString> oldProps = clip->properties();
                    oldProps.insert("proxy", QString());
                    EditClipCommand *command = new EditClipCommand(this, clipId, oldProps, newProps, true);
                    m_doc->commandStack()->push(command);
                }
            }
        }

        if (!replace && m_allClipsProcessed && item->data(0, Qt::DecorationRole).isNull()) {
            getCachedThumbnail(item);
        }
        if (!toReload.isEmpty())
            item->slotSetToolTip();
    } else kDebug() << "////////  COULD NOT FIND CLIP TO UPDATE PRPS...";
    if (queue == 0) {
        monitorItemEditing(true);
        if (item && m_thumbnailQueue.isEmpty()) {
            if (!item->hasProxy() || m_render->activeClipId() == item->clipId())
                m_listView->setCurrentItem(item);
            bool updatedProfile = false;
            if (item->parent()) {
                if (item->parent()->type() == PROJECTFOLDERTYPE)
                    static_cast <FolderProjectItem *>(item->parent())->switchIcon();
            } else if (KdenliveSettings::checkfirstprojectclip() &&  m_listView->topLevelItemCount() == 1 && m_refreshed && m_allClipsProcessed) {
                // this is the first clip loaded in project, check if we want to adjust project settings to the clip
                updatedProfile = adjustProjectProfileToItem(item);
            }
            if (updatedProfile == false) {
                //emit clipSelected(item->referencedClip());
            }
        } else {
            int max = m_doc->clipManager()->clipsCount();
            if (max > 0) emit displayMessage(i18n("Loading clips"), (int)(100 *(max - queue) / max));
        }
        if (m_allClipsProcessed) emit processNextThumbnail();
    }
    if (!item) {
        // no item for producer, delete it
        delete producer;
        return;
    }
    if (replace) toReload = clipId;
    if (!toReload.isEmpty())
        emit clipNeedsReload(toReload);
}

bool ProjectList::adjustProjectProfileToItem(ProjectItem *item)
{
    if (item == NULL) {
        if (m_listView->currentItem() && m_listView->currentItem()->type() != PROJECTFOLDERTYPE)
            item = static_cast <ProjectItem*>(m_listView->currentItem());
    }
    if (item == NULL || item->referencedClip() == NULL) {
        KMessageBox::information(kapp->activeWindow(), i18n("Cannot find profile from current clip"));
        return false;
    }
    bool profileUpdated = false;
    QString size = item->referencedClip()->getProperty("frame_size");
    int width = size.section('x', 0, 0).toInt();
    int height = size.section('x', -1).toInt();
    // Fix some avchd clips tht report a wrong size (1920x1088)
    if (height == 1088) height = 1080;
    double fps = item->referencedClip()->getProperty("fps").toDouble();
    double par = item->referencedClip()->getProperty("aspect_ratio").toDouble();
    if (item->clipType() == IMAGE || item->clipType() == AV || item->clipType() == VIDEO) {
        if (ProfilesDialog::matchProfile(width, height, fps, par, item->clipType() == IMAGE, m_doc->mltProfile()) == false) {
            // get a list of compatible profiles
            QMap <QString, QString> suggestedProfiles = ProfilesDialog::getProfilesFromProperties(width, height, fps, par, item->clipType() == IMAGE);
            if (!suggestedProfiles.isEmpty()) {
                KDialog *dialog = new KDialog(this);
                dialog->setCaption(i18n("Change project profile"));
                dialog->setButtons(KDialog::Ok | KDialog::Cancel);

                QWidget container;
                QVBoxLayout *l = new QVBoxLayout;
                QLabel *label = new QLabel(i18n("Your clip does not match current project's profile.\nDo you want to change the project profile?\n\nThe following profiles match the clip (size: %1, fps: %2)", size, fps));
                l->addWidget(label);
                QListWidget *list = new QListWidget;
                list->setAlternatingRowColors(true);
                QMapIterator<QString, QString> i(suggestedProfiles);
                while (i.hasNext()) {
                    i.next();
                    QListWidgetItem *item = new QListWidgetItem(i.value(), list);
                    item->setData(Qt::UserRole, i.key());
                    item->setToolTip(i.key());
                }
                list->setCurrentRow(0);
                l->addWidget(list);
                container.setLayout(l);
                dialog->setButtonText(KDialog::Ok, i18n("Update profile"));
                dialog->setMainWidget(&container);
                if (dialog->exec() == QDialog::Accepted) {
                    //Change project profile
                    profileUpdated = true;
                    if (list->currentItem())
                        emit updateProfile(list->currentItem()->data(Qt::UserRole).toString());
                }
                delete list;
                delete label;
            } else if (fps > 0) {
                KMessageBox::information(kapp->activeWindow(), i18n("Your clip does not match current project's profile.\nNo existing profile found to match the clip's properties.\nClip size: %1\nFps: %2\n", size, fps));
            }
        }
    }
    return profileUpdated;
}

QString ProjectList::getDocumentProperty(const QString &key) const
{
    return m_doc->getDocumentProperty(key);
}

bool ProjectList::useProxy() const
{
    return m_doc->getDocumentProperty("enableproxy").toInt();
}

bool ProjectList::generateProxy() const
{
    return m_doc->getDocumentProperty("generateproxy").toInt();
}

bool ProjectList::generateImageProxy() const
{
    return m_doc->getDocumentProperty("generateimageproxy").toInt();
}

void ProjectList::slotReplyGetImage(const QString &clipId, const QImage &img)
{
    ProjectItem *item = getItemById(clipId);
    if (item && !img.isNull()) {
        QPixmap pix = QPixmap::fromImage(img);
        processThumbOverlays(item, pix);
        monitorItemEditing(false);
        item->setData(0, Qt::DecorationRole, pix);
        monitorItemEditing(true);
        QString hash = item->getClipHash();
        if (!hash.isEmpty()) m_doc->cacheImage(hash, img);
    }
}

void ProjectList::slotReplyGetImage(const QString &clipId, const QString &name, int width, int height)
{
    // For clips that have a generic icon (like audio clips...)
    ProjectItem *item = getItemById(clipId);
    QPixmap pix =  KIcon(name).pixmap(QSize(width, height));
    if (item && !pix.isNull()) {
        monitorItemEditing(false);
        item->setData(0, Qt::DecorationRole, pix);
        monitorItemEditing(true);
    }
}

QTreeWidgetItem *ProjectList::getAnyItemById(const QString &id)
{
    QTreeWidgetItemIterator it(m_listView);
    QString lookId = id;
    if (id.contains('#'))
        lookId = id.section('#', 0, 0);

    ProjectItem *result = NULL;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        ProjectItem *item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == lookId) {
            result = item;
            break;
        }
        ++it;
    }
    if (result == NULL || !id.contains('#')) {
        return result;
    } else {
        for (int i = 0; i < result->childCount(); i++) {
            SubProjectItem *sub = static_cast <SubProjectItem *>(result->child(i));
            if (sub && sub->zone().x() == id.section('#', 1, 1).toInt())
                return sub;
        }
    }

    return NULL;
}


ProjectItem *ProjectList::getItemById(const QString &id)
{
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem or folder
            ++it;
            continue;
        }
        item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == id)
            return item;
        ++it;
    }
    return NULL;
}

FolderProjectItem *ProjectList::getFolderItemByName(const QString &name)
{
    FolderProjectItem *item = NULL;
    QList <QTreeWidgetItem *> hits = m_listView->findItems(name, Qt::MatchExactly, 0);
    for (int i = 0; i < hits.count(); i++) {
        if (hits.at(i)->type() == PROJECTFOLDERTYPE) {
            item = static_cast<FolderProjectItem *>(hits.at(i));
            break;
        }
    }
    return item;
}

FolderProjectItem *ProjectList::getFolderItemById(const QString &id)
{
    FolderProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() == PROJECTFOLDERTYPE) {
            item = static_cast<FolderProjectItem *>(*it);
            if (item->clipId() == id)
                return item;
        }
        ++it;
    }
    return NULL;
}

void ProjectList::slotSelectClip(const QString &ix)
{
    ProjectItem *clip = getItemById(ix);
    if (clip) {
        m_listView->setCurrentItem(clip);
        m_listView->scrollToItem(clip);
        m_editButton->defaultAction()->setEnabled(true);
        m_deleteButton->defaultAction()->setEnabled(true);
        m_reloadAction->setEnabled(true);
        m_extractAudioAction->setEnabled(true);
        m_transcodeAction->setEnabled(true);
        m_stabilizeAction->setEnabled(true);
        if (clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else {
            m_openAction->setEnabled(false);
        }
    }
}

QString ProjectList::currentClipUrl() const
{
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE) return QString();
    if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
        // subitem
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    } else {
        item = static_cast <ProjectItem*>(m_listView->currentItem());
    }
    if (item == NULL)
        return QString();
    return item->clipUrl().path();
}

KUrl::List ProjectList::getConditionalUrls(const QString &condition) const
{
    KUrl::List result;
    ProjectItem *item;
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == PROJECTFOLDERTYPE)
            continue;
        if (list.at(i)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            item = static_cast <ProjectItem*>(list.at(i)->parent());
        } else {
            item = static_cast <ProjectItem*>(list.at(i));
        }
        if (item == NULL || item->type() == COLOR || item->type() == SLIDESHOW || item->type() == TEXT)
            continue;
        DocClipBase *clip = item->referencedClip();
        if (!condition.isEmpty()) {
            if (condition.startsWith("vcodec") && !clip->hasVideoCodec(condition.section('=', 1, 1)))
                continue;
            else if (condition.startsWith("acodec") && !clip->hasAudioCodec(condition.section('=', 1, 1)))
                continue;
        }
        result.append(item->clipUrl());
    }
    return result;
}

QStringList ProjectList::getConditionalIds(const QString &condition) const
{
    QStringList result;
    ProjectItem *item;
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == PROJECTFOLDERTYPE)
            continue;
        if (list.at(i)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            item = static_cast <ProjectItem*>(list.at(i)->parent());
        } else {
            item = static_cast <ProjectItem*>(list.at(i));
        }
        if (item == NULL || item->type() == COLOR || item->type() == SLIDESHOW || item->type() == TEXT)
            continue;
        DocClipBase *clip = item->referencedClip();
        if (!condition.isEmpty()) {
            if (condition.startsWith("vcodec") && !clip->hasVideoCodec(condition.section('=', 1, 1)))
                continue;
            else if (condition.startsWith("acodec") && !clip->hasAudioCodec(condition.section('=', 1, 1)))
                continue;
        }
        result.append(item->clipId());
    }
    return result;
}

void ProjectList::regenerateTemplate(const QString &id)
{
    ProjectItem *clip = getItemById(id);
    if (clip)
        regenerateTemplate(clip);
}

void ProjectList::regenerateTemplate(ProjectItem *clip)
{
    //TODO: remove this unused method, only force_reload is necessary
    clip->referencedClip()->getProducer()->set("force_reload", 1);
}

QDomDocument ProjectList::generateTemplateXml(QString path, const QString &replaceString)
{
    QDomDocument doc;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
        return doc;
    }
    if (!doc.setContent(&file)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
        file.close();
        return doc;
    }
    file.close();
    QDomNodeList texts = doc.elementsByTagName("content");
    for (int i = 0; i < texts.count(); i++) {
        QString data = texts.item(i).firstChild().nodeValue();
        data.replace("%s", replaceString);
        texts.item(i).firstChild().setNodeValue(data);
    }
    return doc;
}


void ProjectList::slotAddClipCut(const QString &id, int in, int out)
{
    ProjectItem *clip = getItemById(id);
    if (clip == NULL || clip->referencedClip()->hasCutZone(QPoint(in, out)))
        return;
    AddClipCutCommand *command = new AddClipCutCommand(this, id, in, out, QString(), true, false);
    m_commandStack->push(command);
}

void ProjectList::addClipCut(const QString &id, int in, int out, const QString desc, bool newItem)
{
    ProjectItem *clip = getItemById(id);
    if (clip) {
        DocClipBase *base = clip->referencedClip();
        base->addCutZone(in, out);
        monitorItemEditing(false);
        SubProjectItem *sub = new SubProjectItem(clip, in, out, desc);
        if (newItem && desc.isEmpty() && !m_listView->isColumnHidden(1)) {
            if (!clip->isExpanded())
                clip->setExpanded(true);
            m_listView->scrollToItem(sub);
            m_listView->editItem(sub, 1);
        }
        QImage img = clip->referencedClip()->extractImage(in, (int)(sub->sizeHint(0).height()  * m_render->dar()), sub->sizeHint(0).height() - 2);
        sub->setData(0, Qt::DecorationRole, QPixmap::fromImage(img));
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) m_doc->cacheImage(hash + '#' + QString::number(in), img);
        monitorItemEditing(true);
    }
    emit projectModified();
}

void ProjectList::removeClipCut(const QString &id, int in, int out)
{
    ProjectItem *clip = getItemById(id);
    if (clip) {
        DocClipBase *base = clip->referencedClip();
        base->removeCutZone(in, out);
        SubProjectItem *sub = getSubItem(clip, QPoint(in, out));
        if (sub) {
            monitorItemEditing(false);
            delete sub;
            monitorItemEditing(true);
        }
    }
    emit projectModified();
}

SubProjectItem *ProjectList::getSubItem(ProjectItem *clip, QPoint zone)
{
    SubProjectItem *sub = NULL;
    if (clip) {
        for (int i = 0; i < clip->childCount(); i++) {
            QTreeWidgetItem *it = clip->child(i);
            if (it->type() == PROJECTSUBCLIPTYPE) {
                sub = static_cast <SubProjectItem*>(it);
                if (sub->zone() == zone)
                    break;
                else
                    sub = NULL;
            }
        }
    }
    return sub;
}

void ProjectList::slotUpdateClipCut(QPoint p)
{
    if (!m_listView->currentItem() || m_listView->currentItem()->type() != PROJECTSUBCLIPTYPE)
        return;
    SubProjectItem *sub = static_cast <SubProjectItem*>(m_listView->currentItem());
    ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
    EditClipCutCommand *command = new EditClipCutCommand(this, item->clipId(), sub->zone(), p, sub->text(1), sub->text(1), true);
    m_commandStack->push(command);
}

void ProjectList::doUpdateClipCut(const QString &id, const QPoint oldzone, const QPoint zone, const QString &comment)
{
    ProjectItem *clip = getItemById(id);
    SubProjectItem *sub = getSubItem(clip, oldzone);
    if (sub == NULL || clip == NULL)
        return;
    DocClipBase *base = clip->referencedClip();
    base->updateCutZone(oldzone.x(), oldzone.y(), zone.x(), zone.y(), comment);
    monitorItemEditing(false);
    sub->setZone(zone);
    sub->setDescription(comment);
    monitorItemEditing(true);
    emit projectModified();
}

void ProjectList::slotForceProcessing(const QString &id)
{
    m_render->forceProcessing(id);
}

void ProjectList::slotAddOrUpdateSequence(const QString frameName)
{
    QString fileName = KUrl(frameName).fileName().section('_', 0, -2);
    QStringList list;
    QString pattern = SlideshowClip::selectedPath(frameName, false, QString(), &list);
    int count = list.count();
    if (count > 1) {
        const QList <DocClipBase *> existing = m_doc->clipManager()->getClipByResource(pattern);
        if (!existing.isEmpty()) {
            // Sequence already exists, update
            QString id = existing.at(0)->getId();
            //ProjectItem *item = getItemById(id);
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            int ttl = existing.at(0)->getProperty("ttl").toInt();
            oldprops["out"] = existing.at(0)->getProperty("out");
            newprops["out"] = QString::number(ttl * count - 1);
            slotUpdateClipProperties(id, newprops);
            EditClipCommand *command = new EditClipCommand(this, id, oldprops, newprops, false);
            m_commandStack->push(command);
        } else {
            // Create sequence
            QStringList groupInfo = getGroup();
            QMap <QString, QString> properties;
            properties.insert("name", fileName);
            properties.insert("resource", pattern);
            properties.insert("in", "0");
            QString duration = m_timecode.reformatSeparators(KdenliveSettings::sequence_duration());
            properties.insert("out", QString::number(m_doc->getFramePos(duration) * count));
            properties.insert("ttl", QString::number(m_doc->getFramePos(duration)));
            properties.insert("loop", QString::number(false));
            properties.insert("crop", QString::number(false));
            properties.insert("fade", QString::number(false));
            properties.insert("luma_duration", m_timecode.getTimecodeFromFrames(int(ceil(m_timecode.fps()))));
                        
            m_doc->slotCreateSlideshowClipFile(properties, groupInfo.at(0), groupInfo.at(1));
        }
    } else emit displayMessage(i18n("Sequence not found"), -2);
}

QMap <QString, QString> ProjectList::getProxies()
{
    QMap <QString, QString> list;
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            ++it;
            continue;
        }
        item = static_cast<ProjectItem *>(*it);
        if (item && item->referencedClip() != NULL) {
            if (item->hasProxy()) {
                QString proxy = item->referencedClip()->getProperty("proxy");
                list.insert(proxy, item->clipUrl().path());
            }
        }
        ++it;
    }
    return list;
}

void ProjectList::slotCreateProxy(const QString id)
{
    ProjectItem *item = getItemById(id);
    if (!item || hasPendingJob(item, PROXYJOB) || item->referencedClip()->isPlaceHolder()) return;
    QString path = item->referencedClip()->getProperty("proxy");
    if (path.isEmpty()) {
        slotUpdateJobStatus(item, PROXYJOB, JOBCRASHED, i18n("Failed to create proxy, empty path."));
        return;
    }
    
    if (QFileInfo(path).size() > 0) {
        // Proxy already created
        setJobStatus(item, PROXYJOB, JOBDONE);
        slotGotProxy(path);
        return;
    }

    ProxyJob *job = new ProxyJob(item->clipType(), id, QStringList() << path << item->clipUrl().path() << item->referencedClip()->producerProperty("_exif_orientation") << m_doc->getDocumentProperty("proxyparams").simplified() << QString::number(m_render->frameRenderWidth()) << QString::number(m_render->renderHeight()));
    if (job->isExclusive() && hasPendingJob(item, job->jobType)) {
        delete job;
        return;
    }

    m_jobList.append(job);
    setJobStatus(item, job->jobType, JOBWAITING, 0, job->statusMessage());
    slotCheckJobProcess();
}

void ProjectList::slotCutClipJob(const QString &id, QPoint zone)
{
    ProjectItem *item = getItemById(id);
    if (!item|| item->referencedClip()->isPlaceHolder()) return;
    QString source = item->clipUrl().path();
    QString ext = source.section('.', -1);
    QString dest = source.section('.', 0, -2) + '_' + QString::number(zone.x()) + '.' + ext;
    
    double clipFps = item->referencedClip()->getProperty("fps").toDouble();
    if (clipFps == 0) clipFps = m_fps;
    // if clip and project have different frame rate, adjust in and out
    int in = zone.x();
    int out = zone.y();
    in = GenTime(in, m_timecode.fps()).frames(clipFps);
    out = GenTime(out, m_timecode.fps()).frames(clipFps);
    int max = GenTime(item->clipMaxDuration(), m_timecode.fps()).frames(clipFps);
    int duration = out - in + 1;
    QString timeIn = Timecode::getStringTimecode(in, clipFps, true);
    QString timeOut = Timecode::getStringTimecode(duration, clipFps, true);
    
    QPointer<QDialog> d = new QDialog(this);
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    ui.extra_params->setVisible(false);
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.file_url->fileDialog()->setOperationMode(KFileDialog::Saving);
    ui.extra_params->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    ui.file_url->setUrl(KUrl(dest));
    ui.button_more->setIcon(KIcon("configure"));
    ui.extra_params->setPlainText("-acodec copy -vcodec copy");
    QString mess = i18n("Extracting %1 out of %2", timeOut, Timecode::getStringTimecode(max, clipFps, true));
    ui.info_label->setText(mess);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return;
    }
    dest = ui.file_url->url().path();
    bool acceptPath = dest != source;
    if (acceptPath && QFileInfo(dest).size() > 0) {
        // destination file olready exists, overwrite?
        acceptPath = false;
    }
    while (!acceptPath) {
        // Do not allow to save over original clip
        if (dest == source) ui.info_label->setText("<b>" + i18n("You cannot overwrite original clip.") + "</b><br>" + mess);
        else if (KMessageBox::questionYesNo(this, i18n("Overwrite file %1", dest)) == KMessageBox::Yes) break;
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return;
        }
        dest = ui.file_url->url().path();
        acceptPath = dest != source;
        if (acceptPath && QFileInfo(dest).size() > 0) {
            acceptPath = false;
        }
    }
    QString extraParams = ui.extra_params->toPlainText().simplified();
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    delete d;

    QStringList jobParams;
    jobParams << dest << item->clipUrl().path() << timeIn << timeOut << QString::number(duration) << QString::number(KdenliveSettings::add_new_clip());
    if (!extraParams.isEmpty()) jobParams << extraParams;
    CutClipJob *job = new CutClipJob(item->clipType(), id, jobParams);
    if (job->isExclusive() && hasPendingJob(item, job->jobType)) {
        delete job;
        return;
    }
    m_jobList.append(job);
    setJobStatus(item, job->jobType, JOBWAITING, 0, job->statusMessage());

    slotCheckJobProcess();
}

void ProjectList::slotTranscodeClipJob(const QString &condition, QString params, QString desc)
{
    QStringList existingFiles;
    QStringList ids = getConditionalIds(condition);
    QStringList destinations;
    foreach(const QString &id, ids) {
        ProjectItem *item = getItemById(id);
        if (!item) continue;
        QString newFile = params.section(' ', -1).replace("%1", item->clipUrl().path());
        destinations << newFile;
        if (QFile::exists(newFile)) existingFiles << newFile;
    }
    if (!existingFiles.isEmpty()) {
        if (KMessageBox::warningContinueCancelList(this, i18n("The transcoding job will overwrite the following files:"), existingFiles) ==  KMessageBox::Cancel) return;
    }
    
    QDialog *d = new QDialog(this);
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    d->setWindowTitle(i18n("Transcoding"));
    ui.extra_params->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    if (ids.count() == 1) {
        ui.file_url->setUrl(KUrl(destinations.first()));
    }
    else {
        ui.destination_label->setVisible(false);
        ui.file_url->setVisible(false);
    }
    ui.extra_params->setVisible(false);
    d->adjustSize();
    ui.button_more->setIcon(KIcon("configure"));
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.extra_params->setPlainText(params.simplified().section(' ', 0, -2));
    QString mess = desc;
    mess.append(' ' + i18np("(%1 clip)", "(%1 clips)", ids.count()));
    ui.info_label->setText(mess);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return;
    }
    params = ui.extra_params->toPlainText().simplified();
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    int index = 0;
    foreach(const QString &id, ids) {
        ProjectItem *item = getItemById(id);
        if (!item || !item->referencedClip()) continue;
        QString src = item->clipUrl().path();
        QString dest;
        if (ids.count() > 1) {
	    dest = destinations.at(index);
	    index++;
	}
        else dest = ui.file_url->url().path();
        QStringList jobParams;
        jobParams << dest << src << QString() << QString();
        double clipFps = item->referencedClip()->getProperty("fps").toDouble();
        if (clipFps == 0) clipFps = m_fps;
        int max = item->clipMaxDuration();
        QString duration = QString::number(max);
        jobParams << duration;
        jobParams << QString::number(KdenliveSettings::add_new_clip());
        jobParams << params;
        CutClipJob *job = new CutClipJob(item->clipType(), id, jobParams);
        if (job->isExclusive() && hasPendingJob(item, job->jobType)) {
            delete job;
            continue;
        }
        m_jobList.append(job);
        setJobStatus(item, job->jobType, JOBWAITING, 0, job->statusMessage());
    }
    delete d;
    slotCheckJobProcess();
    
}

void ProjectList::slotCheckJobProcess()
{        
    if (!m_jobThreads.futures().isEmpty()) {
        // Remove inactive threads
        QList <QFuture<void> > futures = m_jobThreads.futures();
        m_jobThreads.clearFutures();
        for (int i = 0; i < futures.count(); i++)
            if (!futures.at(i).isFinished()) {
                m_jobThreads.addFuture(futures.at(i));
            }
    }
    if (m_jobList.isEmpty()) return;
    int count = 0;
    m_jobMutex.lock();
    for (int i = 0; i < m_jobList.count(); i++) {
        if (m_jobList.at(i)->jobStatus == JOBWORKING || m_jobList.at(i)->jobStatus == JOBWAITING)
            count ++;
        else {
            // remove finished jobs
            AbstractClipJob *job = m_jobList.takeAt(i);
            delete job;
            i--;
        }
    }

    emit jobCount(count);
    m_jobMutex.unlock();
    
    if (m_jobThreads.futures().isEmpty() || m_jobThreads.futures().count() < KdenliveSettings::proxythreads()) m_jobThreads.addFuture(QtConcurrent::run(this, &ProjectList::slotProcessJobs));
}

void ProjectList::slotAbortProxy(const QString id, const QString path)
{
    Q_UNUSED(path)

    ProjectItem *item = getItemById(id);
    if (!item) return;
    if (!item->isProxyRunning()) slotGotProxy(item);
    item->setConditionalJobStatus(NOJOB, PROXYJOB);
    discardJobs(id, PROXYJOB);
}

void ProjectList::slotProcessJobs()
{
    while (!m_jobList.isEmpty() && !m_abortAllJobs) {
        emit projectModified();
        AbstractClipJob *job = NULL;
        int count = 0;
        m_jobMutex.lock();
        for (int i = 0; i < m_jobList.count(); i++) {
            if (m_jobList.at(i)->jobStatus == JOBWAITING) {
                if (job == NULL) {
                    m_jobList.at(i)->jobStatus = JOBWORKING;
                    job = m_jobList.at(i);
                }
                count++;
            }
            else if (m_jobList.at(i)->jobStatus == JOBWORKING)
                count ++;
        }
        // Set jobs count
        emit jobCount(count);
        m_jobMutex.unlock();

        if (job == NULL) {
            break;
        }
        QString destination = job->destination();
        // Check if the clip is still here
        DocClipBase *currentClip = m_doc->clipManager()->getClipById(job->clipId());
        //ProjectItem *processingItem = getItemById(job->clipId());
        if (currentClip == NULL) {
            job->setStatus(JOBDONE);
            continue;
        }
        // Set clip status to started
        emit processLog(job->clipId(), 0, job->jobType, job->statusMessage()); 

        // Make sure destination path is writable
        if (!destination.isEmpty()) {
            QFile file(destination);
            if (!file.open(QIODevice::WriteOnly)) {
                emit updateJobStatus(job->clipId(), job->jobType, JOBCRASHED, i18n("Cannot write to path: %1", destination));
                job->setStatus(JOBCRASHED);
                continue;
            }
            file.close();
            QFile::remove(destination);
        }
        connect(job, SIGNAL(jobProgress(QString, int, int)), this, SIGNAL(processLog(QString, int, int)));
        connect(job, SIGNAL(cancelRunningJob(const QString, stringMap)), this, SIGNAL(cancelRunningJob(const QString, stringMap)));
        connect(job, SIGNAL(gotFilterJobResults(QString,int, int, QString,stringMap)), this, SIGNAL(gotFilterJobResults(QString,int, int, QString,stringMap)));

        if (job->jobType == MLTJOB) {
            MeltJob *jb = static_cast<MeltJob *> (job);
            jb->setProducer(currentClip->getProducer(), currentClip->fileURL());
        }
        job->startJob();
        if (job->jobStatus == JOBDONE) {
            emit updateJobStatus(job->clipId(), job->jobType, JOBDONE);
            //TODO: replace with more generic clip replacement framework
            if (job->jobType == PROXYJOB) emit gotProxy(job->clipId());
            if (job->addClipToProject()) {
                emit addClip(destination, QString(), QString());
            }
        } else if (job->jobStatus == JOBCRASHED || job->jobStatus == JOBABORTED) {
            emit updateJobStatus(job->clipId(), job->jobType, job->jobStatus, job->errorMessage(), QString(), job->logDetails());
        }
    }
    // Thread finished, cleanup & update count
    QTimer::singleShot(200, this, SIGNAL(checkJobProcess()));
}


void ProjectList::updateProxyConfig()
{
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    QUndoCommand *command = new QUndoCommand();
    command->setText(i18n("Update proxy settings"));
    QString proxydir = m_doc->projectFolder().path( KUrl::AddTrailingSlash) + "proxy/";
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            ++it;
            continue;
        }
        item = static_cast<ProjectItem *>(*it);
        if (item == NULL) {
            ++it;
            continue;
        }
        CLIPTYPE t = item->clipType();
        if ((t == VIDEO || t == AV || t == UNKNOWN) && item->referencedClip() != NULL) {
            if  (generateProxy() && useProxy() && !hasPendingJob(item, PROXYJOB)) {
                DocClipBase *clip = item->referencedClip();
                if (clip->getProperty("frame_size").section('x', 0, 0).toInt() > m_doc->getDocumentProperty("proxyminsize").toInt()) {
                    if (clip->getProperty("proxy").isEmpty()) {
                        // We need to insert empty proxy in old properties so that undo will work
                        QMap <QString, QString> oldProps;// = clip->properties();
                        oldProps.insert("proxy", QString());
                        QMap <QString, QString> newProps;
                        newProps.insert("proxy", proxydir + item->referencedClip()->getClipHash() + '.' + m_doc->getDocumentProperty("proxyextension"));
                        new EditClipCommand(this, clip->getId(), oldProps, newProps, true, command);
                    }
                }
            }
            else if (item->hasProxy()) {
                // remove proxy
                QMap <QString, QString> newProps;
                newProps.insert("proxy", QString());
                // insert required duration for proxy
                newProps.insert("proxy_out", item->referencedClip()->producerProperty("out"));
                new EditClipCommand(this, item->clipId(), item->referencedClip()->currentProperties(newProps), newProps, true, command);
            }
        }
        else if (t == IMAGE && item->referencedClip() != NULL) {
            if  (generateImageProxy() && useProxy()) {
                DocClipBase *clip = item->referencedClip();
                int maxImageSize = m_doc->getDocumentProperty("proxyimageminsize").toInt();
                if (clip->getProperty("frame_size").section('x', 0, 0).toInt() > maxImageSize || clip->getProperty("frame_size").section('x', 1, 1).toInt() > maxImageSize) {
                    if (clip->getProperty("proxy").isEmpty()) {
                        // We need to insert empty proxy in old properties so that undo will work
                        QMap <QString, QString> oldProps = clip->properties();
                        oldProps.insert("proxy", QString());
                        QMap <QString, QString> newProps;
                        newProps.insert("proxy", proxydir + item->referencedClip()->getClipHash() + ".png");
                        new EditClipCommand(this, clip->getId(), oldProps, newProps, true, command);
                    }
                }
            }
            else if (item->hasProxy()) {
                // remove proxy
                QMap <QString, QString> newProps;
                newProps.insert("proxy", QString());
                new EditClipCommand(this, item->clipId(), item->referencedClip()->properties(), newProps, true, command);
            }
        }
        ++it;
    }
    if (command->childCount() > 0) m_doc->commandStack()->push(command);
    else delete command;
}

void ProjectList::slotProcessLog(const QString id, int progress, int type, const QString message)
{
    ProjectItem *item = getItemById(id);
    setJobStatus(item, (JOBTYPE) type, JOBWORKING, progress, message);
}

void ProjectList::slotProxyCurrentItem(bool doProxy, ProjectItem *itemToProxy)
{
    QList<QTreeWidgetItem *> list;
    if (itemToProxy == NULL) list = m_listView->selectedItems();
    else list << itemToProxy;

    // expand list (folders, subclips) to get real clips
    QTreeWidgetItem *listItem;
    QList<ProjectItem *> clipList;
    for (int i = 0; i < list.count(); i++) {
        listItem = list.at(i);
        if (listItem->type() == PROJECTFOLDERTYPE) {
            for (int j = 0; j < listItem->childCount(); j++) {
                QTreeWidgetItem *sub = listItem->child(j);
                if (sub->type() == PROJECTCLIPTYPE) {
                    ProjectItem *item = static_cast <ProjectItem*>(sub);
                    if (!clipList.contains(item)) clipList.append(item);
                }
            }
        }
        else if (listItem->type() == PROJECTSUBCLIPTYPE) {
            QTreeWidgetItem *sub = listItem->parent();
            ProjectItem *item = static_cast <ProjectItem*>(sub);
            if (!clipList.contains(item)) clipList.append(item);
        }
        else if (listItem->type() == PROJECTCLIPTYPE) {
            ProjectItem *item = static_cast <ProjectItem*>(listItem);
            if (!clipList.contains(item)) clipList.append(item);
        }
    }
    
    QUndoCommand *command = new QUndoCommand();
    if (doProxy) command->setText(i18np("Add proxy clip", "Add proxy clips", clipList.count()));
    else command->setText(i18np("Remove proxy clip", "Remove proxy clips", clipList.count()));
    
    // Make sure the proxy folder exists
    QString proxydir = m_doc->projectFolder().path( KUrl::AddTrailingSlash) + "proxy/";
    KStandardDirs::makeDir(proxydir);
                
    QMap <QString, QString> newProps;
    QMap <QString, QString> oldProps;
    if (!doProxy) newProps.insert("proxy", "-");
    for (int i = 0; i < clipList.count(); i++) {
        ProjectItem *item = clipList.at(i);
        CLIPTYPE t = item->clipType();
        if ((t == VIDEO || t == AV || t == UNKNOWN || t == IMAGE || t == PLAYLIST) && item->referencedClip()) {
            if ((doProxy && item->hasProxy()) || (!doProxy && !item->hasProxy() && item->referencedClip()->getProducer() != NULL)) continue;
            DocClipBase *clip = item->referencedClip();
            if (!clip || !clip->isClean() || m_render->isProcessing(item->clipId())) {
                kDebug()<<"//// TRYING TO PROXY: "<<item->clipId()<<", but it is busy";
                continue;
            }
                
            //oldProps = clip->properties();
            if (doProxy) {
                newProps.clear();
                QString path = proxydir + clip->getClipHash() + '.' + (t == IMAGE ? "png" : m_doc->getDocumentProperty("proxyextension"));
                // insert required duration for proxy
                newProps.insert("proxy_out", clip->producerProperty("out"));
                newProps.insert("proxy", path);
                // We need to insert empty proxy so that undo will work
                //oldProps.insert("proxy", QString());
            }
            else if (item->referencedClip()->getProducer() == NULL) {
                // Force clip reload
                kDebug()<<"// CLIP HAD NULL PROD------------";
                newProps.insert("resource", item->referencedClip()->getProperty("resource"));
            }
            // We need to insert empty proxy so that undo will work
            oldProps = clip->currentProperties(newProps);
            if (doProxy) oldProps.insert("proxy", "-");
            new EditClipCommand(this, item->clipId(), oldProps, newProps, true, command);
        }
    }
    if (command->childCount() > 0) {
        m_doc->commandStack()->push(command);
    }
    else delete command;
}


void ProjectList::slotDeleteProxy(const QString proxyPath)
{
    if (proxyPath.isEmpty()) return;
    QUndoCommand *proxyCommand = new QUndoCommand();
    proxyCommand->setText(i18n("Remove Proxy"));
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;
    while (*it) {
        if ((*it)->type() == PROJECTCLIPTYPE) {
            item = static_cast <ProjectItem *>(*it);
            if (item->referencedClip()->getProperty("proxy") == proxyPath) {
                QMap <QString, QString> props;
                props.insert("proxy", QString());
                new EditClipCommand(this, item->clipId(), item->referencedClip()->currentProperties(props), props, true, proxyCommand);
            
            }
        }
        ++it;
    }
    if (proxyCommand->childCount() == 0)
        delete proxyCommand;
    else
        m_commandStack->push(proxyCommand);
    QFile::remove(proxyPath);
}

void ProjectList::setJobStatus(ProjectItem *item, JOBTYPE jobType, CLIPJOBSTATUS status, int progress, const QString &statusMessage)
{
    if (item == NULL || (m_abortAllJobs && m_closing)) return;
    monitorItemEditing(false);
    item->setJobStatus(jobType, status, progress, statusMessage);
    if (status == JOBCRASHED) {
        DocClipBase *clip = item->referencedClip();
        if (!clip) {
            kDebug()<<"// PROXY CRASHED";
        }
        else if (clip->getProducer() == NULL && !clip->isPlaceHolder()) {
            // disable proxy and fetch real clip
            clip->setProperty("proxy", "-");
            QDomElement xml = clip->toXML();
            m_render->getFileProperties(xml, clip->getId(), m_listView->iconSize().height(), true);
        }
        else {
            // Disable proxy for this clip
            clip->setProperty("proxy", "-");
        }
    }
    monitorItemEditing(true);
}

void ProjectList::monitorItemEditing(bool enable)
{
    if (enable) connect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));     
    else disconnect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));     
}

QStringList ProjectList::expandedFolders() const
{
    QStringList result;
    FolderProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTFOLDERTYPE) {
            ++it;
            continue;
        }
        if ((*it)->isExpanded()) {
            item = static_cast<FolderProjectItem *>(*it);
            result.append(item->clipId());
        }
        ++it;
    }
    return result;
}

void ProjectList::processThumbOverlays(ProjectItem *item, QPixmap &pix)
{
    if (item->hasProxy()) {
        QPainter p(&pix);
        QColor c(220, 220, 10, 200);
        QRect r(0, 0, 12, 12);
        p.fillRect(r, c);
        QFont font = p.font();
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(r, Qt::AlignCenter, i18nc("The first letter of Proxy, used as abbreviation", "P"));
    }
}

void ProjectList::slotCancelJobs()
{
    m_abortAllJobs = true;
    for (int i = 0; i < m_jobList.count(); i++) {
        m_jobList.at(i)->setStatus(JOBABORTED);
    }
    m_jobThreads.waitForFinished();
    m_jobThreads.clearFutures();
    QUndoCommand *command = new QUndoCommand();
    command->setText(i18np("Cancel job", "Cancel jobs", m_jobList.count()));
    m_jobMutex.lock();
    for (int i = 0; i < m_jobList.count(); i++) {
        DocClipBase *currentClip = m_doc->clipManager()->getClipById(m_jobList.at(i)->clipId());
        if (!currentClip) continue;
        QMap <QString, QString> newProps = m_jobList.at(i)->cancelProperties();
        if (newProps.isEmpty()) continue;
        QMap <QString, QString> oldProps = currentClip->currentProperties(newProps);
        new EditClipCommand(this, m_jobList.at(i)->clipId(), oldProps, newProps, true, command);
    }
    m_jobMutex.unlock();
    if (command->childCount() > 0) {
        m_doc->commandStack()->push(command);
    }
    else delete command;
    if (!m_jobList.isEmpty()) qDeleteAll(m_jobList);
    m_jobList.clear();
    m_abortAllJobs = false;
    m_infoLabel->slotSetJobCount(0);    
}

void ProjectList::slotCancelRunningJob(const QString id, stringMap newProps)
{
    if (newProps.isEmpty() || m_closing) return;
    DocClipBase *currentClip = m_doc->clipManager()->getClipById(id);
    if (!currentClip) return;
    QMap <QString, QString> oldProps = currentClip->currentProperties(newProps);
    if (newProps == oldProps) return;
    QMapIterator<QString, QString> i(oldProps);
    EditClipCommand *command = new EditClipCommand(this, id, oldProps, newProps, true);
    m_commandStack->push(command);    
}

bool ProjectList::hasPendingJob(ProjectItem *item, JOBTYPE type)
{
    if (!item || !item->referencedClip() || m_abortAllJobs) return false;
    AbstractClipJob *job;
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); i++) {
        if (m_abortAllJobs) break;
        job = m_jobList.at(i);
        if (job->clipId() == item->clipId() && job->jobType == type && (job->jobStatus == JOBWAITING || job->jobStatus == JOBWORKING)) return true;
    }
    
    return false;
}

void ProjectList::deleteJobsForClip(const QString &clipId)
{
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); i++) {
        if (m_jobList.at(i)->clipId() == clipId) {
            m_jobList.at(i)->setStatus(JOBABORTED);
        }
    }
}

void ProjectList::slotUpdateJobStatus(const QString id, int type, int status, const QString label, const QString actionName, const QString details)
{
    ProjectItem *item = getItemById(id);
    if (!item) return;
    slotUpdateJobStatus(item, type, status, label, actionName, details);
    
}

void ProjectList::slotUpdateJobStatus(ProjectItem *item, int type, int status, const QString &label, const QString &actionName, const QString details)
{
    item->setJobStatus((JOBTYPE) type, (CLIPJOBSTATUS) status);
    if (status != JOBCRASHED) return;
#if KDE_IS_VERSION(4,7,0)
    QList<QAction *> actions = m_infoMessage->actions();
    if (m_infoMessage->isHidden()) {
	m_infoMessage->setText(label);
	m_infoMessage->setWordWrap(m_infoMessage->text().length() > 35);
	m_infoMessage->setMessageType(KMessageWidget::Warning);
    }
    
    if (!actionName.isEmpty()) {
        QAction *action = NULL;
        QList< KActionCollection * > collections = KActionCollection::allCollections();
        for (int i = 0; i < collections.count(); i++) {
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
#else
    // warning for KDE < 4.7
    KPassivePopup *passivePop = new KPassivePopup( this );
    passivePop->setAutoDelete(true);
    connect(passivePop, SIGNAL(clicked()), this, SLOT(slotClosePopup()));
    m_errorLog.append(details);
    KVBox *vb = new KVBox( passivePop );
    KHBox *vh1 = new KHBox( vb );
    KIcon icon("dialog-warning");
    QLabel *iconLabel = new QLabel(vh1);
    iconLabel->setPixmap(icon.pixmap(m_listView->iconSize()));
    (void) new QLabel( label, vh1);
    KHBox *box = new KHBox( vb );
    QPushButton *but = new QPushButton( "Show log", box );
    connect(but, SIGNAL(clicked(bool)), this, SLOT(slotShowJobLog()));

    passivePop->setView( vb );
    passivePop->show();
    
#endif
}

void ProjectList::slotShowJobLog()
{
    KDialog d(this);
    d.setButtons(KDialog::Close);
    QTextEdit t(&d);
    for (int i = 0; i < m_errorLog.count(); i++) {
	if (i > 0) t.insertHtml("<br><hr /><br>");
	t.insertPlainText(m_errorLog.at(i));
    }
    t.setReadOnly(true);
    d.setMainWidget(&t);
    d.exec();
}

QStringList ProjectList::getPendingJobs(const QString &id)
{
    QStringList result;
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); i++) {
        if (m_jobList.at(i)->clipId() == id && (m_jobList.at(i)->jobStatus == JOBWAITING || m_jobList.at(i)->jobStatus == JOBWORKING)) {
            // discard this job
            result << m_jobList.at(i)->description;
        }
    }   
    return result;
}

void ProjectList::discardJobs(const QString &id, JOBTYPE type) {
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); i++) {
        if (m_jobList.at(i)->clipId() == id && (m_jobList.at(i)->jobType == type || type == NOJOBTYPE)) {
            // discard this job
            m_jobList.at(i)->setStatus(JOBABORTED);
        }
    }
}

void ProjectList::slotStartFilterJob(ItemInfo info, const QString&id, const QString&filterName, const QString&filterParams, const QString&finalFilterName, const QString&consumer, const QString&consumerParams, const QString&properties)
{
    ProjectItem *item = getItemById(id);
    if (!item) return;
    QStringList jobParams;
    jobParams << QString::number(info.cropStart.frames(m_fps)) << QString::number((info.cropStart + info.cropDuration).frames(m_fps));
    jobParams << QString() << filterName << filterParams << consumer << consumerParams << properties << QString::number(info.startPos.frames(m_fps)) << QString::number(info.track) << finalFilterName;
    MeltJob *job = new MeltJob(item->clipType(), id, jobParams);
    if (job->isExclusive() && hasPendingJob(item, job->jobType)) {
        delete job;
        return;
    }
    job->description = i18n("Filter %1", finalFilterName);
    m_jobList.append(job);
    setJobStatus(item, job->jobType, JOBWAITING, 0, job->statusMessage());
    slotCheckJobProcess();
}

void ProjectList::startClipFilterJob(const QString &filterName, const QString &condition)
{
    QStringList ids = getConditionalIds(condition);
    QString destination;
    ProjectItem *item = getItemById(ids.at(0));
    if (!item) {
        emit displayMessage(i18n("Cannot find clip to process filter %1", filterName), -2);
        return;
    }
    if (ids.count() == 1) {
        destination = item->clipUrl().path();
    }
    else {
        destination = item->clipUrl().directory();
    }
    QPointer<ClipStabilize> d = new ClipStabilize(destination, ids.count(), filterName);
    if (d->exec() == QDialog::Accepted) {
        processClipJob(ids, d->destination(), d->autoAddClip(), d->params(), d->desc());
    }
    delete d;
}

void ProjectList::processClipJob(QStringList ids, const QString&destination, bool autoAdd, QStringList jobParams, const QString &description)
{
    QStringList preParams;
    // in and out
    preParams << QString::number(0) << QString::number(-1);
    // producer params
    preParams << jobParams.takeFirst();
    // filter name
    preParams << jobParams.takeFirst();
    // filter params
    preParams << jobParams.takeFirst();
    // consumer
    QString consumer = jobParams.takeFirst();
    
    foreach(const QString&id, ids) {
        ProjectItem *item = getItemById(id);
        if (!item) continue;
        if (ids.count() == 1) {
            consumer += ':' + destination;
        }
        else {
            consumer += ':' + destination + item->clipUrl().fileName() + ".mlt";
        }
        preParams << consumer << jobParams;
        
        MeltJob *job = new MeltJob(item->clipType(), id, preParams);
        if (autoAdd) {
            job->setAddClipToProject(true);
            kDebug()<<"// ADDING TRUE";
        }
        else kDebug()<<"// ADDING FALSE!!!";
        if (job->isExclusive() && hasPendingJob(item, job->jobType)) {
            delete job;
            return;
        }
        job->description = description;
        m_jobList.append(job);
        setJobStatus(item, job->jobType, JOBWAITING, 0, job->statusMessage());
    }
    slotCheckJobProcess();
}

void ProjectList::slotPrepareJobsMenu()
{
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE)
        return;
    if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE)
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    else
        item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item && (item->flags() & Qt::ItemIsDragEnabled)) {
        QString id = item->clipId();
        m_discardCurrentClipJobs->setData(id);
        QStringList jobs = getPendingJobs(id);
        m_discardCurrentClipJobs->setEnabled(!jobs.isEmpty());
    } else {
        m_discardCurrentClipJobs->setData(QString());
        m_discardCurrentClipJobs->setEnabled(false);
    }
}

void ProjectList::slotDiscardClipJobs()
{
    QString id = m_discardCurrentClipJobs->data().toString();
    if (id.isEmpty()) return;
    discardJobs(id);
}

void ProjectList::updatePalette()
{
    m_infoLabel->setStyleSheet(SmallInfoLabel::getStyleSheet(QApplication::palette()));
    m_listView->updateStyleSheet();
}

void ProjectList::slotResetInfoMessage()
{
#if KDE_IS_VERSION(4,7,0)
    m_errorLog.clear();
    QList<QAction *> actions = m_infoMessage->actions();
    for (int i = 0; i < actions.count(); i++) {
	m_infoMessage->removeAction(actions.at(i));
    }
#endif
}

void ProjectList::slotClosePopup()
{
    m_errorLog.clear();
}

#include "projectlist.moc"
