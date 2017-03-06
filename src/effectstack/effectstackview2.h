/***************************************************************************
                          effecstackview2.h  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler (g.marco@freenet.de)
    copyright            : (C) 2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @class EffectStackView2
 * @brief View part of the EffectStack
 * @author Marco Gittler
 */

#ifndef EFFECTSTACKVIEW2_H
#define EFFECTSTACKVIEW2_H

#include "collapsibleeffect.h"
#include "collapsiblegroup.h"

#include <QTimer>

class EffectsList;
class ClipItem;
class Transition;
class EffectSettings;
class TransitionSettings;
class ClipController;
class Monitor;

class EffectStackView2 : public QWidget
{
    Q_OBJECT

public:
    explicit EffectStackView2(Monitor *projectMonitor, QWidget *parent = nullptr);
    virtual ~EffectStackView2();

    /** @brief Raises @param dock if a clip is loaded. */
    void raiseWindow(QWidget *dock);

    /** @brief return the current status of effect stack (timeline clip, track or master clip). */
    EFFECTMODE effectStatus() const;
    /** @brief return the index of the track displayed in effect stack */
    int trackIndex() const;

    /** @brief Clears the list of effects and updates the buttons accordingly. */
    void clear();

    /** @brief Tells the effect editor to update its timecode format. */
    void updateTimecodeFormat();

    /** @brief Used to trigger drag effects. */
    bool eventFilter(QObject *o, QEvent *e) Q_DECL_OVERRIDE;

    CollapsibleEffect *getEffectByIndex(int ix);

    /** @brief Delete currently selected effect. */
    void deleteCurrentEffect();

    /** @brief Palette was changed, update style. */
    void updatePalette();

    /** @brief Color theme was changed, update icons. */
    void refreshIcons();

    /** @brief Process dropped xml effect. */
    void processDroppedEffect(QDomElement e, QDropEvent *event);

    /** @brief Return the stylesheet required for effect parameters. */
    static const QString getStyleSheet();

    /** @brief Import keyframes from the clip metadata */
    void setKeyframes(const QString &tag, const QString &keyframes);

    /** @brief Returns the transition setting widget for signal/slot connections */
    TransitionSettings *transitionConfig();

    /** @brief Dis/Enable the effect stack */
    void disableBinEffects(bool disable);
    void disableTimelineEffects(bool disable);

    enum STACKSTATUS {
        NORMALSTATUS = 0,
        DISABLEBIN = 1,
        DISABLETIMELINE = 2,
        DISABLEALL = 3
    };

protected:
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

private:
    ClipItem *m_clipref;
    ClipController *m_masterclipref;
    /** @brief Current status of the effect stack (if it contains a timeline clip, track or master clip effect. */
    EFFECTMODE m_status;
    STACKSTATUS m_stateStatus;

    /** @brief The track index of currently edited track. */
    int m_trackindex;

    /** @brief The effect currently being dragged, nullptr if no drag happening. */
    CollapsibleEffect *m_draggedEffect;

    /** @brief The effect currently being dragged, nullptr if no drag happening. */
    CollapsibleGroup *m_draggedGroup;

    /** @brief The current number of groups. */
    int m_groupIndex;

    /** @brief The current effect may require an on monitor scene. */
    MonitorSceneType m_monitorSceneWanted;
    QMutex m_mutex;
    QTimer m_scrollTimer;

    /** If in track mode: Info of the edited track to be able to access its duration. */
    TrackInfo m_trackInfo;

    QList<CollapsibleEffect *> m_effects;
    EffectsList m_currentEffectList;

    QVBoxLayout m_layout;
    EffectSettings *m_effect;
    TransitionSettings *m_transition;

    /** @brief Contains info about effect like is it a track effect, which monitor displays it,... */
    EffectMetaInfo m_effectMetaInfo;

    /** @brief The last mouse click position, used to detect drag events. */
    QPoint m_clickPoint;

    /** @brief Sets the list of effects according to the clip's effect list. */
    void setupListView();

    /** @brief Build the drag info and start it. */
    void startDrag();

    /** @brief Connect an effect to its signals. */
    void connectEffect(CollapsibleEffect *currentEffect);
    /** @brief Connect a group to its signals. */
    void connectGroup(CollapsibleGroup *group);
    /** @brief Returns index of currently selected effect in stack. */
    int activeEffectIndex() const;
    /** @brief Returns index of previous effect in stack. */
    int getPreviousIndex(int current);
    /** @brief Returns index of next effect in stack. */
    int getNextIndex(int ix);

public slots:
    /** @brief Sets the clip whose effect list should be managed.
    * @param c Clip whose effect list should be managed */
    void slotClipItemSelected(ClipItem *c, Monitor *m = nullptr, bool reloadStack = true);
    /** @brief An effect parameter was changed, refresh effect stack if it was displaying it.
    * @param c Clip controller whose effect list should be managed */
    void slotRefreshMasterClipEffects(ClipController *c, Monitor *m);
    /** @brief Display effects for the selected Bin clip.
    * @param c Clip controller whose effect list should be managed */
    void slotMasterClipItemSelected(ClipController *c, Monitor *m = nullptr);

    /** @brief Update the clip range (in-out points)
    * @param c Clip whose effect list should be managed */
    void slotClipItemUpdate();

    void slotTrackItemSelected(int ix, const TrackInfo &info, Monitor *m = nullptr);

    /** @brief Check if the mouse wheel events should be used for scrolling the widget view. */
    void slotCheckWheelEventFilter();

    void slotTransitionItemSelected(Transition *t, int nextTrack, const QPoint &p, bool update);

    /** @brief Select active keyframe in an animated effect. */
    void setActiveKeyframe(int frame);

private slots:

    /** @brief Emits seekTimeline with position = clipstart + @param pos. */
    void slotSeekTimeline(int pos);

    /* @brief Define the region filter for current effect.
    void slotRegionChanged();*/

    /** @brief Checks whether the monitor scene has to be displayed. */
    void slotCheckMonitorPosition(int renderPos);

    void slotUpdateEffectParams(const QDomElement &old, const QDomElement &e, int ix);

    /** @brief Move an effect in the stack.
     * @param indexes The list of effect index in the stack
     * @param up true if we want to move effect up, false for down */
    void slotMoveEffectUp(const QList<int> &indexes, bool up);

    /** @brief Delete an effect in the stack. */
    void slotDeleteEffect(const QDomElement &effect);

    /** @brief Delete all effect in a group. */
    void slotDeleteGroup(const QDomDocument &doc);

    /** @brief Pass position changes of the timeline cursor to the effects to keep their local timelines in sync. */
    void slotRenderPos(int pos);

    /** @brief Called whenever an effect is enabled / disabled by user. */
    void slotUpdateEffectState(bool disable, int index, MonitorSceneType needsMonitorEffectScene);

    void slotSetCurrentEffect(int ix);

    /** @brief Triggers a filter job on this clip. */
    void slotStartFilterJob(QMap<QString, QString> &, QMap<QString, QString> &, QMap<QString, QString> &);

    /** @brief Reset an effect to its default values. */
    void slotResetEffect(int ix);

    /** @brief Create a group containing effect with ix index. */
    void slotCreateGroup(int ix);

    /** @brief Create a region effect with ix index. */
    void slotCreateRegion(int ix, const QUrl &url);

    /** @brief Move an effect.
      ** @param currentIndexes the list of effect indexes to move in stack layout
      ** @param newIndex the position where the effects will be moved
      ** @param groupIndex the index of the group if any (-1 if none)
      ** @param groupName the name of the group to paste the effect
      */
    void slotMoveEffect(const QList<int> &currentIndexes, int newIndex, int groupIndex, const QString &groupName = QString());

    /** @brief Remove effects from a group */
    void slotUnGroup(CollapsibleGroup *group);

    /** @brief Add en effect to selected clip */
    void slotAddEffect(const QDomElement &effect);

    /** @brief Enable / disable all effects for the clip */
    void slotCheckAll(int state);

    /** @brief Update check all button status */
    void slotUpdateCheckAllButton();

    /** @brief An effect group was renamed, update effects info */
    void slotRenameGroup(CollapsibleGroup *group);

    /** @brief Dis/Enable monitor effect compare */
    void slotSwitchCompare(bool enable);

signals:
    void removeEffectGroup(ClipItem *, int, const QDomDocument &);
    void removeEffect(ClipItem *, int, const QDomElement &);
    void removeMasterEffect(const QString &id, const QDomElement &);
    void addMasterEffect(const QString &id, const QDomElement &);
    /**  Parameters for an effect changed, update the filter in timeline */
    void updateEffect(ClipItem *, int, const QDomElement &, const QDomElement &, int, bool);
    /**  Parameters for an effect changed, update the filter in timeline */
    void updateMasterEffect(QString, const QDomElement &, const QDomElement &, int ix,bool refreshStack = false);
    /** An effect in stack was moved, we need to regenerate
        all effects for this clip in the playlist */
    void refreshEffectStack(ClipItem *);
    /** Enable or disable an effect */
    void changeEffectState(ClipItem *, int, const QList<int> &, bool);
    void changeMasterEffectState(QString id, const QList<int> &, bool);
    /** An effect in stack was moved */
    void changeEffectPosition(ClipItem *, int, const QList<int> &, int);
    /** An effect in stack was moved for a Bin clip */
    void changeEffectPosition(const QString &, const QList<int> &, int);
    /** an effect was saved, reload list */
    void reloadEffects();
    /** An effect with position parameter was changed, seek */
    void seekTimeline(int);
    /** The region effect for current effect was changed */
    void updateClipRegion(ClipItem *, int, const QString &);
    void displayMessage(const QString &, int);
    void showComments(bool show);
    void startFilterJob(const ItemInfo &info, const QString &clipId, QMap<QString, QString> &, QMap<QString, QString> &, QMap<QString, QString> &);
    void addEffect(ClipItem *, const QDomElement &, int);
    void importClipKeyframes(GraphicsRectItem, ItemInfo, const QDomElement &, const QMap<QString, QString> &keyframes = QMap<QString, QString>());
};

#endif
