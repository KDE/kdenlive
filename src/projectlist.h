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


#ifndef PROJECTLIST_H
#define PROJECTLIST_H

#include <QDomNodeList>
#include <QHash>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QUndoStack>
#include <QTimer>
#include <QApplication>
#include <QFuture>
#include <QFutureSynchronizer>
#include <QListWidget>
#include <QTimeLine>
#include <QPushButton>

#include <KTreeWidgetSearchLine>
#include <KUrl>
#include <KIcon>
#include <kdeversion.h>

#ifdef NEPOMUK
#include <nepomuk/kratingpainter.h>
#include <nepomuk/resource.h>
#endif

#include "definitions.h"
#include "timecode.h"
#include "kdenlivesettings.h"
#include "folderprojectitem.h"
#include "subprojectitem.h"
#include "projecttree/abstractclipjob.h"
#include <kdialog.h>

#if KDE_IS_VERSION(4,7,0)
#include <KMessageWidget>
#else
// Dummy KMessageWidget to allow compilation of MyMessageWidget class since Qt's moc doesn work inside #ifdef
#include <QLabel>
class KMessageWidget: public QLabel
{
public:
    KMessageWidget(QWidget * = 0) {};
    KMessageWidget(const QString &, QWidget * = 0) {};
    virtual ~KMessageWidget(){};
};
#endif

class MyMessageWidget: public KMessageWidget
{
    Q_OBJECT
public:
    MyMessageWidget(QWidget *parent = 0);
    MyMessageWidget(const QString &text, QWidget *parent = 0);

protected:
    bool event(QEvent* ev);

signals:
    void messageClosing();
};

namespace Mlt
{
class Producer;
};

class ProjectItem;
class ProjectListView;
class Render;
class KdenliveDoc;
class DocClipBase;
class AbstractClipJob;

const int NameRole = Qt::UserRole;
const int DurationRole = NameRole + 1;
const int UsageRole = NameRole + 2;

class SmallInfoLabel: public QPushButton
{
    Q_OBJECT
public:
    SmallInfoLabel(QWidget *parent = 0);
    static const QString getStyleSheet(const QPalette &p);
private:
    QTimeLine* m_timeLine;

public slots:
    void slotSetJobCount(int jobCount);

private slots:
    void slotTimeLineChanged(qreal value);
    void slotTimeLineFinished();
};
    
class InvalidDialog: public KDialog
{
    Q_OBJECT
public:
    InvalidDialog(const QString &caption, const QString &message, bool infoOnly, QWidget *parent = 0);
    virtual ~InvalidDialog();
    void addClip(const QString &id, const QString &path);
    QStringList getIds() const;
private:
    QListWidget *m_clipList;
};


class ItemDelegate: public QStyledItemDelegate
{
public:
    ItemDelegate(QAbstractItemView* parent = 0): QStyledItemDelegate(parent) {
    }
    
    /*void drawFocus(QPainter *, const QStyleOptionViewItem &, const QRect &) const {
    }*/

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        if (index.column() == 0 && !index.data(DurationRole).isNull()) {
            QRect r1 = option.rect;
            painter->save();
            QStyleOptionViewItemV4 opt(option);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.highlightedText().color());
            }
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            QPixmap pixmap = qVariantValue<QPixmap>(index.data(Qt::DecorationRole));
            QPoint pixmapPoint(r1.left() + textMargin, r1.top() + (r1.height() - pixmap.height()) / 2);
            painter->drawPixmap(pixmapPoint, pixmap);
            int decoWidth = pixmap.width() + 2 * textMargin;

            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            int mid = (int)((r1.height() / 2));
            r1.adjust(decoWidth, 0, 0, -mid);
            QRect r2 = option.rect;
            r2.adjust(decoWidth, mid, 0, 0);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom, index.data().toString());
            font.setBold(false);
            painter->setFont(font);
            QString subText = index.data(DurationRole).toString();
            int usage = index.data(UsageRole).toInt();
            if (usage != 0) subText.append(QString(" (%1)").arg(usage));
            if (option.state & (QStyle::State_Selected)) painter->setPen(option.palette.color(QPalette::Mid));
            QRectF bounding;
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignVCenter , subText, &bounding);
            
            int jobProgress = index.data(Qt::UserRole + 5).toInt();
            if (jobProgress != 0 && jobProgress != JOBDONE && jobProgress != JOBABORTED) {
                if (jobProgress != JOBCRASHED) {
                    // Draw job progress bar
                    QColor color = option.palette.alternateBase().color();
                    painter->setPen(Qt::NoPen);
                    color.setAlpha(180);
                    painter->setBrush(QBrush(color));
                    QRect progress(pixmapPoint.x() + 1, pixmapPoint.y() + pixmap.height() - 9, pixmap.width() - 2, 8);
                    painter->drawRect(progress);
                    painter->setBrush(option.palette.text());
                    if (jobProgress > 0) {
                        progress.adjust(1, 1, 0, -1);
                        progress.setWidth((pixmap.width() - 4) * jobProgress / 100);
                        painter->drawRect(progress);
                    } else if (jobProgress == JOBWAITING) {
                        // Draw kind of a pause icon
                        progress.adjust(1, 1, 0, -1);
                        progress.setWidth(2);
                        painter->drawRect(progress);
                        progress.moveLeft(progress.right() + 2);
                        painter->drawRect(progress);
                    }
                } else if (jobProgress == JOBCRASHED) {
                    QString jobText = index.data(Qt::UserRole + 7).toString();
                    if (!jobText.isEmpty()) {
                        QRectF txtBounding = painter->boundingRect(r2, Qt::AlignRight | Qt::AlignVCenter, " " + jobText + " ");
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(option.palette.highlight());
                        painter->drawRoundedRect(txtBounding, 2, 2);
                        painter->setPen(option.palette.highlightedText().color());
                        painter->drawText(txtBounding, Qt::AlignCenter, jobText);
                    }
                }
            }
            
            painter->restore();
        } else if (index.column() == 2 && KdenliveSettings::activate_nepomuk()) {
            if (index.data().toString().isEmpty()) {
                QStyledItemDelegate::paint(painter, option, index);
                return;
            }
            QRect r1 = option.rect;
            if (option.state & (QStyle::State_Selected)) {
                painter->fillRect(r1, option.palette.highlight());
            }
#ifdef NEPOMUK
            KRatingPainter::paintRating(painter, r1, Qt::AlignCenter, index.data().toInt());
#endif
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

class ProjectList : public QWidget
{
    Q_OBJECT

public:
    ProjectList(QWidget *parent = 0);
    virtual ~ProjectList();

    QDomElement producersList();
    void setRenderer(Render *projectRender);
    void slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties);
    QByteArray headerInfo() const;
    void setHeaderInfo(const QByteArray &state);
    void updateProjectFormat(Timecode t);
    void setupMenu(QMenu *addMenu, QAction *defaultAction);
    void setupGeneratorMenu(const QHash<QString,QMenu*>& menus);
    QString currentClipUrl() const;
    KUrl::List getConditionalUrls(const QString &condition) const;
    /** @brief Get a list of selected clip Id's that match a condition. */
    QStringList getConditionalIds(const QString &condition) const;
    QDomDocument generateTemplateXml(QString data, const QString &replaceString);
    void cleanup();
    void trashUnusedClips();
    QList <DocClipBase*> documentClipList() const;
    void addClipCut(const QString &id, int in, int out, const QString desc, bool newItem);
    void removeClipCut(const QString &id, int in, int out);
    void focusTree() const;
    SubProjectItem *getSubItem(ProjectItem *clip, QPoint zone);
    void doUpdateClipCut(const QString &id, const QPoint oldzone, const QPoint zone, const QString &comment);
    bool hasMissingClips();
    void deleteProjectFolder(QMap <QString, QString> map);
    void selectItemById(const QString &clipId);

    /** @brief Returns a string list of all supported mime extensions. */
    static QString getExtensions();
    /** @brief Returns a list of urls containing original and proxy urls. */
    QMap <QString, QString> getProxies();
    /** @brief Enable / disable proxies. */
    void updateProxyConfig();
    /** @brief Get a property from the document. */
    QString getDocumentProperty(const QString &key) const;
    
    /** @brief Does this project allow proxies. */
    bool useProxy() const;
    /** @brief Should we automatically create proxy clips for newly added clips. */
    bool generateProxy() const;
    /** @brief Should we automatically create proxy clips for newly added clips. */
    bool generateImageProxy() const;
    /** @brief Returns a list of the expanded folder ids. */
    QStringList expandedFolders() const;
    /** @brief Deselect all clips in project tree. */
    void clearSelection();
    /** @brief Print required overlays over clip thumb (proxy, stabilized,...). */
    void processThumbOverlays(ProjectItem *item, QPixmap &pix);
    /** @brief Start an MLT process job. */
    void startClipFilterJob(const QString &filterName, const QString &condition);
    /** @brief Set current document for the project tree. */
    void setDocument(KdenliveDoc *doc);
    
    /** @brief Palette was changed, update style. */
    void updatePalette();

public slots:
    void updateAllClips(bool displayRatioChanged, bool fpsChanged, QStringList brokenClips);
    void slotReplyGetImage(const QString &clipId, const QImage &img);
    void slotReplyGetImage(const QString &clipId, const QString &name, int width, int height);
    void slotReplyGetFileProperties(const QString &clipId, Mlt::Producer *producer, const stringMap &properties, const stringMap &metadata, bool replace);
    void slotAddClip(DocClipBase *clip, bool getProperties);
    void slotDeleteClip(const QString &clipId);
    void slotUpdateClip(const QString &id);
    void slotRefreshClipThumbnail(const QString &clipId, bool update = true);
    void slotRefreshClipThumbnail(QTreeWidgetItem *item, bool update = true);
    void slotRemoveInvalidClip(const QString &id, bool replace);
    void slotRemoveInvalidProxy(const QString &id, bool durationError);
    void slotSelectClip(const QString &ix);

    /** @brief Prepares removing the selected items. */
    void slotRemoveClip();
    void slotAddClip(const QString url, const QString &groupName, const QString &groupId);
    void slotAddClip(const QList <QUrl> givenList = QList <QUrl> (), const QString &groupName = QString(), const QString &groupId = QString());

    /** @brief Adds, edits or deletes a folder item.
    *
    * This is triggered by AddFolderCommand and EditFolderCommand. */
    void slotAddFolder(const QString foldername, const QString &clipId, bool remove, bool edit = false);
    void slotResetProjectList();
    void slotOpenClip();
    void slotEditClip();
    void slotReloadClip(const QString &id = QString());

    /** @brief Shows dialog for setting up a color clip. */
    void slotAddColorClip();
    void regenerateTemplate(const QString &id);
    void slotUpdateClipCut(QPoint p);
    void slotAddClipCut(const QString &id, int in, int out);
    void slotForceProcessing(const QString &id);
    /** @brief Remove all instances of a proxy and delete the file. */
    void slotDeleteProxy(const QString proxyPath);
    /** @brief Start a hard cut clip job. */
    void slotCutClipJob(const QString &id, QPoint zone);
    /** @brief Start transcoding selected clips. */
    void slotTranscodeClipJob(const QString &condition, QString params, QString desc);
    /** @brief Start an MLT process job. */
    void slotStartFilterJob(ItemInfo, const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&);
    

private:
    ProjectListView *m_listView;
    Render *m_render;
    Timecode m_timecode;
    double m_fps;
    QMenu *m_menu;
    QFuture<void> m_queueRunner;
    QUndoStack *m_commandStack;
    ProjectItem *getItemById(const QString &id);
    QTreeWidgetItem *getAnyItemById(const QString &id);
    FolderProjectItem *getFolderItemById(const QString &id);
    FolderProjectItem *getFolderItemByName(const QString &name);
    QAction *m_openAction;
    QAction *m_reloadAction;
    QAction *m_discardCurrentClipJobs;
    QMenu *m_extractAudioAction;
    QMenu *m_transcodeAction;
    QMenu *m_stabilizeAction;
    KdenliveDoc *m_doc;
    ItemDelegate *m_listViewDelegate;
    /** @brief False if we have not yet finished opening the document. */
    bool m_refreshed;
    /** @brief False if we have not yet finished checking all project tree thumbs. */
    bool m_allClipsProcessed;
    QToolButton *m_addButton;
    QToolButton *m_deleteButton;
    QToolButton *m_editButton;
    //QMap <QString, QDomElement> m_infoQueue;
    QMap <QString, QDomElement> m_producerQueue;
    QList <QString> m_thumbnailQueue;
    QAction *m_proxyAction;
    QMutex m_jobMutex;
    bool m_abortAllJobs;
    /** @brief We are cleaning up the project list, so stop processing signals. */
    bool m_closing;
    QList <AbstractClipJob *> m_jobList;
    QFutureSynchronizer<void> m_jobThreads;
    InvalidDialog *m_invalidClipDialog;
    QMenu *m_jobsMenu;
    SmallInfoLabel *m_infoLabel;
    /** @brief A list of strings containing the last error logs for clip jobs. */
    QStringList m_errorLog;

#if KDE_IS_VERSION(4,7,0)
    MyMessageWidget *m_infoMessage;
    /** @brief The action that will trigger the log dialog. */
    QAction *m_logAction;
#endif
    
    void requestClipThumbnail(const QString id);

    /** @brief Creates an EditFolderCommand to change the name of an folder item. */
    void editFolder(const QString folderName, const QString oldfolderName, const QString &clipId);

    /** @brief Gets the selected folder (or the folder of the selected item). */
    QStringList getGroup() const;
    void regenerateTemplate(ProjectItem *clip);
    void editClipSelection(QList<QTreeWidgetItem *> list);

    /** @brief Enables and disables transcode actions based on the selected clip's type. */
    void adjustTranscodeActions(ProjectItem *clip) const;
    /** @brief Enables and disables stabilize actions based on the selected clip's type. */
    void adjustStabilizeActions(ProjectItem *clip) const;
    /** @brief Enables and disables proxy action based on the selected clip. */
    void adjustProxyActions(ProjectItem *clip) const;

    /** @brief Sets the buttons enabled/disabled according to selected item. */
    void updateButtons() const;

    /** @brief Set the Proxy status on a clip. 
     * @param item The clip item to set status
     * @param jobType The job type 
     * @param status The job status (see definitions.h)
     * @param progress The job progress (in percents)
     * @param statusMessage The job info message */
    void setJobStatus(ProjectItem *item, JOBTYPE jobType, CLIPJOBSTATUS status, int progress = 0, const QString &statusMessage = QString());
    void monitorItemEditing(bool enable);
    /** @brief Get cached thumbnail for a project's clip or create it if no cache. */
    void getCachedThumbnail(ProjectItem *item);
    void getCachedThumbnail(SubProjectItem *item);
    /** @brief The clip is about to be reloaded, cancel thumbnail requests. */
    void resetThumbsProducer(DocClipBase *clip);
    /** @brief Check if a clip has a running or pending job process. */
    bool hasPendingJob(ProjectItem *item, JOBTYPE type);
    /** @brief Delete pending jobs for a clip. */
    void deleteJobsForClip(const QString &clipId);
    /** @brief Discard specific job type for a clip. */
    void discardJobs(const QString &id, JOBTYPE type = NOJOBTYPE);
    /** @brief Get the list of job names for current clip. */
    QStringList getPendingJobs(const QString &id);
    /** @brief Start an MLT process job. */
    void processClipJob(QStringList ids, const QString&destination, bool autoAdd, QStringList jobParams, const QString &description, QStringList extraParams = QStringList());

private slots:
    void slotClipSelected();
    void slotAddSlideshowClip();
    void slotAddTitleClip();
    void slotAddTitleTemplateClip();

    /** @brief Shows the context menu after enabling and disabling actions based on the item's type.
    * @param pos The position where the menu should pop up
    * @param item The item for which the checks should be done */
    void slotContextMenu(const QPoint &pos, QTreeWidgetItem *item);

    /** @brief Creates an AddFolderCommand. */
    void slotAddFolder(const QString &name = QString());

    /** @brief This is triggered when a clip description has been modified. */
    void slotItemEdited(QTreeWidgetItem *item, int column);
    void slotUpdateClipProperties(ProjectItem *item, QMap <QString, QString> properties);
    void slotProcessNextThumbnail();
    void slotCheckForEmptyQueue();
    void slotPauseMonitor();
    /** A clip was modified externally, change icon so that user knows it */
    void slotModifiedClip(const QString &id);
    void slotMissingClip(const QString &id);
    void slotAvailableClip(const QString &id);
    /** @brief Try to find a matching profile for given item. */
    bool adjustProjectProfileToItem(ProjectItem *item = NULL);
    /** @brief Add a sequence from the stopmotion widget. */
    void slotAddOrUpdateSequence(const QString frameName);
    /** @brief A proxy clip was created, update display. */
    void slotGotProxy(const QString &proxyPath);
    void slotGotProxy(ProjectItem *item);
    /** @brief Enable / disable proxy for current clip. */
    void slotProxyCurrentItem(bool doProxy, ProjectItem *itemToProxy = NULL);
    /** @brief Put clip in the proxy waiting list. */
    void slotCreateProxy(const QString id);
    /** @brief Stop creation of this clip's proxy. */
    void slotAbortProxy(const QString id, const QString path);
    /** @brief Start creation of clip jobs. */
    void slotProcessJobs();
    /** @brief Discard running and pending clip jobs. */
    void slotCancelJobs();
    /** @brief Discard a running clip jobs. */
    void slotCancelRunningJob(const QString id, stringMap);
    /** @brief Update a clip's job status. */
    void slotProcessLog(const QString, int progress, int, const QString = QString());
    /** @brief A clip job crashed, inform user. */
    void slotUpdateJobStatus(const QString id, int type, int status, const QString label, const QString actionName, const QString details);
    void slotUpdateJobStatus(ProjectItem *item, int type, int status, const QString &label, const QString &actionName = QString(), const QString details = QString());
    /** @brief Display error log for last failed job. */
    void slotShowJobLog();
    /** @brief A proxy clip is ready. */
    void slotGotProxyForId(const QString);
    /** @brief Check if it is necessary to start a job thread. */
    void slotCheckJobProcess();
    /** @brief Fill the jobs menu with current clip's jobs. */
    void slotPrepareJobsMenu();
    /** @brief Discard all jobs for current clip. */
    void slotDiscardClipJobs();
    /** @brief Make sure current clip is visible in project tree. */
    void slotCheckScrolling();
    /** @brief Reset all text and log data from info message widget. */
    void slotResetInfoMessage();
    /** @brief close warning info passive popup. */
    void slotClosePopup();
    /** @brief process clip job result. */
    void slotGotFilterJobResults(QString ,int , int, QString, stringMap);

signals:
    void clipSelected(DocClipBase *, QPoint zone = QPoint(), bool forceUpdate = false);
    void receivedClipDuration(const QString &);
    void showClipProperties(DocClipBase *);
    void showClipProperties(QList <DocClipBase *>, QMap<QString, QString> commonproperties);
    void projectModified();
    void loadingIsOver();
    void displayMessage(const QString, int progress);
    void clipNameChanged(const QString, const QString);
    void clipNeedsReload(const QString&);
    /** @brief A property affecting display was changed, so we need to update monitors and thumbnails
     *  @param id: The clip's id string
     *  @param resetThumbs Should we recreate the timeline thumbnails. */
    void refreshClip(const QString &id, bool resetThumbs);
    void updateRenderStatus();
    void deleteProjectClips(QStringList ids, QMap <QString, QString> folderids);
    void findInTimeline(const QString &clipId);
    /** @brief Request a profile change for current document. */
    void updateProfile(const QString &);
    void processNextThumbnail();
    /** @brief Activate the clip monitor. */
    void raiseClipMonitor();
    /** @brief Set number of running jobs. */
    void jobCount(int);
    void cancelRunningJob(const QString, stringMap);
    void processLog(const QString, int , int, const QString = QString());
    void addClip(const QString, const QString &, const QString &);
    void updateJobStatus(const QString, int, int, const QString label = QString(), const QString actionName = QString(), const QString details = QString());
    void gotProxy(const QString);
    void checkJobProcess();
    /** @brief A Filter Job produced results, send them back to the clip. */
    void gotFilterJobResults(const QString &id, int startPos, int track, const QString &filterName, stringMap params);
    void pauseMonitor();
};

#endif


