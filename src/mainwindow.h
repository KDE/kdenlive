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


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDockWidget>
#include <QUndoView>
#include <QLabel>
#include <QProgressBar>
#include <QEvent>
#include <QTimer>

#include <KXmlGuiWindow>
#include <KTextEdit>
#include <KListWidget>
#include <KTabWidget>
#include <KUndoStack>
#include <KRecentFilesAction>
#include <KComboBox>
#include <kautosavefile.h>

#include "effectslist.h"
#include "gentime.h"
#include "definitions.h"
#include "statusbarmessagelabel.h"
#include "dvdwizard.h"

class KdenliveDoc;
class TrackView;
class MonitorManager;
class ProjectList;
class EffectsListView;
class EffectStackView;
class TransitionSettings;
class Monitor;
class RecMonitor;
class CustomTrackView;
class RenderWidget;
#ifndef NO_JOGSHUTTLE
class JogShuttle;
#endif /* NO_JOGSHUTTLE */
class DocClipBase;
class Render;
class Transition;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kdenlive.MainWindow")

public:
    /** Constructor
     * \param MltPath path to MLT environment
     * \param Url Url to open
     * \param parent Std. widget parent
     *
     * The constructor inits the main window. If Url is present, it will be opened.
     * If Url is not present, and openLastproject is set, last project will be set
     * If no file is open after trying this, a default "newfile" will be created. */
    explicit MainWindow(const QString &MltPath = QString(), const KUrl & Url = KUrl(), QWidget *parent = 0);
    void parseProfiles(const QString &mltPath = QString());

    static EffectsList videoEffects;
    static EffectsList audioEffects;
    static EffectsList customEffects;
    static EffectsList transitions;
protected:
    virtual bool queryClose();
    virtual void customEvent(QEvent * e);
    virtual void keyPressEvent(QKeyEvent *ke);
    /** Override hideEvent to get events when the mainwindow gets hidden */
    virtual void hideEvent(QHideEvent *e);
    bool eventFilter(QObject *obj, QEvent *ev);
    /**
     * This function is called when it is time for the app to save its
     * properties for session management purposes.
     */
    void saveProperties(KConfig *);

    /**
     * This function is called when this app is restored.  The KConfig
     * object points to the session management config file that was saved
     * with @ref saveProperties
     */
    void readProperties(KConfig *);

private:
    KTabWidget* m_timelineArea;
    QProgressBar *m_statusProgressBar;
    void setupActions();
    KdenliveDoc *m_activeDocument;
    TrackView *m_activeTimeline;
    MonitorManager *m_monitorManager;

    QDockWidget *m_projectListDock;
    ProjectList *m_projectList;

    QDockWidget *m_effectListDock;
    EffectsListView *m_effectList;
    //KListWidget *m_effectList;

    QDockWidget *m_effectStackDock;
    EffectStackView *m_effectStack;

    QDockWidget *m_transitionConfigDock;
    TransitionSettings *m_transitionConfig;

    QDockWidget *m_clipMonitorDock;
    Monitor *m_clipMonitor;

    QDockWidget *m_projectMonitorDock;
    Monitor *m_projectMonitor;

    QDockWidget *m_recMonitorDock;
    RecMonitor *m_recMonitor;

    QDockWidget *m_undoViewDock;
    QUndoView *m_undoView;
    QUndoGroup *m_commandStack;

    KComboBox *m_timecodeFormat;

    QMenu *m_customEffectsMenu;
    QMenu *m_timelineContextMenu;
    QMenu *m_timelineContextClipMenu;
    QMenu *m_timelineContextTransitionMenu;

    RenderWidget *m_renderWidget;

#ifndef NO_JOGSHUTTLE
    JogShuttle *m_jogProcess;
#endif /* NO_JOGSHUTTLE */

    KRecentFilesAction *m_fileOpenRecent;
    KAction *m_projectSearch;
    KAction *m_projectSearchNext;

    KAction *m_buttonAudioThumbs;
    KAction *m_buttonVideoThumbs;
    KAction *m_buttonShowMarkers;
    KAction *m_buttonFitZoom;
    KAction *m_buttonSelectTool;
    KAction *m_buttonRazorTool;
    KAction *m_buttonSpacerTool;
    KAction *m_buttonSnap;
    QActionGroup *m_toolGroup;
    KAction *m_saveAction;
    KAction *m_closeAction;
    QSlider *m_zoomSlider;
    KAction *m_loopZone;
    KAction *m_playZone;
    StatusBarMessageLabel *m_messageLabel;
    QActionGroup *m_clipTypeGroup;

    bool m_findActivated;
    QString m_findString;
    QTimer m_findTimer;

    void readOptions();
    void saveOptions();
#ifndef NO_JOGSHUTTLE
    void activateShuttleDevice();
    void slotShuttleAction(int code);
#endif /* NO_JOGSHUTTLE */
    void connectDocumentInfo(KdenliveDoc *doc);
    void findAhead();
    void doOpenFile(const KUrl &url, KAutoSaveFile *stale);
    void recoverFiles(QList<KAutoSaveFile *> staleFiles);
    void loadPlugins();
    void populateMenus(QObject *plugin);
    void addToMenu(QObject *plugin, const QStringList &texts,
                   QMenu *menu, const char *member,
                   QActionGroup *actionGroup);
    void aboutPlugins();
    QStringList m_pluginFileNames;
    QByteArray m_timelineState;

public slots:
    void openFile(const KUrl &url);
    void slotGotProgressInfo(const QString &message, int progress);
    Q_SCRIPTABLE void setRenderingProgress(const QString &url, int progress);
    Q_SCRIPTABLE void setRenderingFinished(const QString &url, int status, const QString &error);

private slots:
    void newFile(bool showProjectSettings = true);
    void queryQuit();
    void activateDocument();
    void connectDocument(TrackView*, KdenliveDoc*);
    void openFile();
    void openLastFile();
    bool saveFile();
    bool saveFileAs();
    bool saveFileAs(const QString &outputFileName);
    void slotPreferences(int page = -1, int option = -1);
    void updateConfiguration();
    void slotConnectMonitors();
    void slotRaiseMonitor(bool clipMonitor);
    void slotSetClipDuration(const QString &id, int duration);
    void slotUpdateMousePosition(int pos);
    void slotAddEffect(QDomElement effect, GenTime pos = GenTime(), int track = -1);
    void slotEditProfiles();
    void slotEditProjectSettings();
    void slotDisplayActionMessage(QAction *a);
    void slotSwitchVideoThumbs();
    void slotSwitchAudioThumbs();
    void slotSwitchMarkersComments();
    void slotSwitchSnap();
    void slotRenderProject();
    void slotDoRender(const QString &dest, const QString &render, const QStringList &overlay_args, const QStringList &avformat_args, bool zoneOnly, bool playAfter, double guideStart, double guideEnd, bool resizeProfile, const QString &scriptExport);
    void slotFullScreen();
    void slotUpdateDocumentState(bool modified);
    void slotZoomIn();
    void slotZoomOut();
    void slotFitZoom();
    void closeCurrentDocument();
    void slotDeleteTimelineClip();
    void slotAddClipMarker();
    void slotDeleteClipMarker();
    void slotDeleteAllClipMarkers();
    void slotEditClipMarker();
    void slotCutTimelineClip();
    void slotAddVideoEffect(QAction *result);
    void slotAddAudioEffect(QAction *result);
    void slotAddCustomEffect(QAction *result);
    void slotAddTransition(QAction *result);
    void slotAddProjectClip(KUrl url);
#ifndef NO_JOGSHUTTLE
    void slotShuttleButton(int code);
#endif /* NO_JOGSHUTTLE */
    void slotShowClipProperties(DocClipBase *clip);
    void slotActivateEffectStackView();
    void slotActivateTransitionView(Transition *);
    void slotChangeTool(QAction * action);
    void slotSetTool(PROJECTTOOL tool);
    void slotSnapForward();
    void slotSnapRewind();
    void slotClipStart();
    void slotClipEnd();
    void slotFind();
    void findTimeout();
    void slotFindNext();

    void slotInsertSpace();
    void slotRemoveSpace();
    void slotAddGuide();
    void slotEditGuide();
    void slotDeleteGuide();
    void slotDeleteAllGuides();
    void slotGuidesUpdated();

    void slotCopy();
    void slotPaste();
    void slotPasteEffects();
    void slotReloadEffects();
    void slotChangeClipSpeed();

    void slotAdjustClipMonitor();
    void slotAdjustProjectMonitor();
    void slotSaveZone(Render *render, QPoint zone);

    void slotSetInPoint();
    void slotSetOutPoint();
    void configureNotifications();
    void slotInsertTrack(int ix = 0);
    void slotDeleteTrack(int ix = 0);
    void slotChangeTrack(int ix = 0);
    void slotGetNewLumaStuff();
    void slotGetNewRenderStuff();
    void slotGetNewMltProfileStuff();
    void slotAutoTransition();
    void slotRunWizard();
    void generateClip();
    void slotZoneMoved(int start, int end);
    void slotUpdatePreviewSettings();
    void slotDvdWizard(const QString &url = QString(), const QString &profile = "dv_pal");
    void slotGroupClips();
    void slotUnGroupClips();
    void slotSplitAudio();
    void slotUpdateClipType(QAction *action);
    void slotShowTimeline(bool show);
    void slotMaximizeCurrent(bool show);

signals:
    Q_SCRIPTABLE void abortRenderJob(const QString &url);
};


#endif
