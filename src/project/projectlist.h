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

#include "folderprojectitem.h"
#include "subprojectitem.h"
#include "jobs/abstractclipjob.h"

#include "definitions.h"
#include "timecode.h"
#include "kdenlivesettings.h"
#include "project/invaliddialog.h"

#include <QHash>
#include <QToolButton>
#include <QTreeWidget>
#include <QUndoStack>
#include <QFuture>
#include <QFutureSynchronizer>
#include <QTimeLine>
#include <QPushButton>

#include <KTreeWidgetSearchLine>
#ifdef NEPOMUK
#include <nepomuk/kratingpainter.h>
#include <nepomuk/resource.h>
#endif

#ifdef NEPOMUKCORE
#include <nepomuk2/kratingpainter.h>
#include <nepomuk2/resource.h>
#endif

#include <KMessageWidget>


class MyMessageWidget: public KMessageWidget
{
    Q_OBJECT
public:
    explicit MyMessageWidget(QWidget *parent = 0);
    explicit MyMessageWidget(const QString &text, QWidget *parent = 0);

protected:
    bool event(QEvent* ev);

signals:
    void messageClosing();
};

namespace Mlt
{
class Producer;
}

class ProjectItem;
class BinController;
class ProjectListView;
class Render;
class KdenliveDoc;
class DocClipBase;
class AbstractClipJob;
class ItemDelegate;
//class ClipPropertiesManager;

class SmallInfoLabel: public QPushButton
{
    Q_OBJECT
public:
    explicit SmallInfoLabel(QWidget *parent = 0);
    static const QString getStyleSheet(const QPalette &p);
private:
    enum ItemRole {
        NameRole = Qt::UserRole,
        DurationRole,
        UsageRole
    };

    QTimeLine* m_timeLine;

public slots:
    void slotSetJobCount(int jobCount);

private slots:
    void slotTimeLineChanged(qreal value);
    void slotTimeLineFinished();
};

class ProjectList : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectList(QWidget *parent = 0);
    virtual ~ProjectList();

    BinController *binController();
    void setRenderer(Render *projectRender);
    void slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties);
    QByteArray headerInfo() const;
    void setHeaderInfo(const QByteArray &state);
    void updateProjectFormat(Timecode t);
    /** @brief Get a list of selected clip Id's and url's that match a condition. */
    QMap <QString, QString> getConditionalIds(const QString &condition) const;
    QDomDocument generateTemplateXml(QString data, const QString &replaceString);
    void cleanup();
    void trashUnusedClips();
    void addClipCut(const QString &id, int in, int out, const QString desc, bool newItem);
    void removeClipCut(const QString &id, int in, int out);
    void focusTree() const;
    SubProjectItem *getSubItem(ProjectItem *clip, const QPoint &zone);
    void doUpdateClipCut(const QString &id, const QPoint &oldzone, const QPoint &zone, const QString &comment);
    bool hasMissingClips();
    void deleteProjectFolder(QMap <QString, QString> map);
    void selectItemById(const QString &clipId);

    /** @brief Returns a string list of all supported mime extensions. */
    static QStringList getExtensions();
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
    /** @brief Set current document for the project tree. */
    void setDocument(KdenliveDoc *doc);
    
    /** @brief Palette was changed, update style. */
    void updatePalette();

public slots:
    void updateAllClips(bool displayRatioChanged, bool fpsChanged, const QStringList &brokenClips);
    void slotReplyGetImage(const QString &clipId, const QImage &img);
    void slotReplyGetImage(const QString &clipId, const QString &name, int width, int height);
    void slotReplyGetFileProperties(requestClipInfo &clipInfo, Mlt::Producer &producer, const stringMap &properties, const stringMap &metadata);
    void slotDeleteClip(const QString &clipId);
    void slotUpdateClip(const QString &id);
    void slotRefreshClipThumbnail(const QString &clipId, bool update = true);
    void slotRefreshClipThumbnail(QTreeWidgetItem *item, bool update = true);
    void slotRemoveInvalidClip(const QString &id, bool replace);
    void slotRemoveInvalidProxy(const QString &id, bool durationError);
    void slotSelectClip(const QString &ix);

    /** @brief Prepares removing the selected items. */
    void slotRemoveClip();

    /** @brief Adds, edits or deletes a folder item.
    *
    * This is triggered by AddFolderCommand and EditFolderCommand. */
    void slotAddFolder(const QString &foldername, const QString &clipId, bool remove, bool edit = false);
    void slotResetProjectList();
    void slotOpenClip();
    void slotEditClip();
    void slotReloadClip(const QString &id = QString());
    void regenerateTemplate(const QString &id);
    void slotUpdateClipCut(QPoint p);
    void slotAddClipCut(const QString &id, int in, int out);
    void slotForceProcessing(const QString &id);
    /** @brief Remove all instances of a proxy and delete the file. */
    void slotDeleteProxy(const QString proxyPath);
    /** @brief Start a hard cut clip job. */
    void slotCutClipJob(const QString &id, QPoint zone);
    void slotSetThumbnail(const QString &id, int framePos, QImage img);
    

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
    QMenu *m_clipsActionsMenu;
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
    QMutex m_processMutex;
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
    
    //TODO
    //ClipPropertiesManager *m_clipPropertiesManager;

    MyMessageWidget *m_infoMessage;
    /** @brief The action that will trigger the log dialog. */
    QAction *m_logAction;
    
    void requestClipThumbnail(const QString &id);

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
    void setJobStatus(ProjectItem *item, AbstractClipJob::JOBTYPE jobType, ClipJobStatus status, int progress = 0, const QString &statusMessage = QString());
    void monitorItemEditing(bool enable);
    /** @brief Get cached thumbnail for a project's clip or create it if no cache. */
    void getCachedThumbnail(ProjectItem *item);
    void getCachedThumbnail(SubProjectItem *item);
    /** @brief The clip is about to be reloaded, cancel thumbnail requests. */
    void resetThumbsProducer(DocClipBase *clip);
    /** @brief Check if a clip has a running or pending job process. */
    bool hasPendingJob(ProjectItem *item, AbstractClipJob::JOBTYPE type);
    /** @brief Delete pending jobs for a clip. */
    void deleteJobsForClip(const QString &clipId);
    /** @brief Discard specific job type for a clip. */
    void discardJobs(const QString &id, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);
    /** @brief Get the list of job names for current clip. */
    QStringList getPendingJobs(const QString &id);
    /** @brief Create rounded shape pixmap for project tree thumb. */
    QPixmap roundedPixmap(const QImage &img);
    QPixmap roundedPixmap(const QPixmap &source);
    /** @brief Extract a clip's metadata with the exiftool program. */
    void extractMetadata(DocClipBase *clip);
    /** @brief Add a special FFmpeg tag if clip matches some properties (for example set full_luma for Sony NEX camcorders. */
    //void checkCamcorderFilters(DocClipBase *clip, QMap <QString, QString> meta);

private slots:
    void slotClipSelected();
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
    void slotAddOrUpdateSequence(const QString &frameName);
    /** @brief A proxy clip was created, update display. */
    void slotGotProxy(const QString &proxyPath);
    void slotGotProxy(ProjectItem *item);
    /** @brief Enable / disable proxy for current clip. */
    void slotProxyCurrentItem(bool doProxy, ProjectItem *itemToProxy = NULL);
    /** @brief Put clip in the proxy waiting list. */
    void slotCreateProxy(const QString &id);
    /** @brief Stop creation of this clip's proxy. */
    void slotAbortProxy(const QString &id, const QString& path);
    /** @brief Start creation of clip jobs. */
    void slotProcessJobs();
    /** @brief Discard running and pending clip jobs. */
    void slotCancelJobs();
    /** @brief Discard a running clip jobs. */
    void slotCancelRunningJob(const QString id, stringMap);
    /** @brief Update a clip's job status. */
    void slotProcessLog(const QString&, int progress, int, const QString & tmp= QString());
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
    /** @brief close warning info passive popup. */
    void slotClosePopup();
    /** @brief process clip job result. */
    void slotGotFilterJobResults(QString ,int , int, stringMap, stringMap);
    /** @brief Select clip in project list from its id. */
    void selectClip(const QString &id);

signals:
    void clipSelected(DocClipBase *, const QPoint &zone = QPoint(), bool forceUpdate = false);
    void receivedClipDuration(const QString &);
    void firstClip(ProjectItem *);
    void projectModified();
    void loadingIsOver();
    void displayMessage(const QString&, int progress, MessageType type = DefaultMessage);
    void clipNameChanged(const QString&, const QString&);
    void clipNeedsReload(const QString&);
    /** @brief A property affecting display was changed, so we need to update monitors and thumbnails
     *  @param id: The clip's id string
     *  @param resetThumbs Should we recreate the timeline thumbnails. */
    void refreshClip(const QString &id, bool resetThumbs);
    void updateRenderStatus();
    void deleteProjectClips(const QStringList &ids, const QMap <QString, QString> &folderids);
    void findInTimeline(const QString &clipId);
    /** @brief Request a profile change for current document. */
    void updateProfile(const QString &);
    void processNextThumbnail();
    /** @brief Activate the clip monitor. */
    void raiseClipMonitor(bool forceRefresh);
    /** @brief Set number of running jobs. */
    void jobCount(int);
    void cancelRunningJob(const QString&, const stringMap&);
    void processLog(const QString&, int , int, const QString & = QString());
    void addClip(const QString, const QString &, const QString &);
    void updateJobStatus(const QString&, int, int, const QString &label = QString(), const QString &actionName = QString(), const QString &details = QString());
    void gotProxy(const QString&);
    void checkJobProcess();
    /** @brief A Filter Job produced results, send them back to the clip. */
    void gotFilterJobResults(const QString &id, int startPos, int track, const stringMap &params, const stringMap &extra);
    void pauseMonitor();
    void updateAnalysisData(DocClipBase *);
    void addMarkers(const QString &, const QList <CommentedTime>&);
    void requestClipSelect(const QString&);
};

#endif


