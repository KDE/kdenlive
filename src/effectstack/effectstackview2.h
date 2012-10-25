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

#include "ui_effectstack2_ui.h"
#include "collapsibleeffect.h"
#include "collapsiblegroup.h"

class EffectsList;
class ClipItem;
class MltVideoProfile;
class Monitor;


class EffectStackView2 : public QWidget
{
    Q_OBJECT

public:
    explicit EffectStackView2(Monitor *monitor, QWidget *parent = 0);
    virtual ~EffectStackView2();

    /** @brief Raises @param dock if a clip is loaded. */
    void raiseWindow(QWidget* dock);

    /** @brief return the index of the track displayed in effect stack
     ** @param ok set to true if we are looking at a track's effects, otherwise false. */
    int isTrackMode(bool *ok) const;

    /** @brief Clears the list of effects and updates the buttons accordingly. */
    void clear();

    /** @brief Passes updates on @param profile and @param t on to the effect editor. */
    void updateProjectFormat(MltVideoProfile profile, Timecode t);

    /** @brief Tells the effect editor to update its timecode format. */
    void updateTimecodeFormat();
    
    /** @brief Used to trigger drag effects. */
    virtual bool eventFilter( QObject * o, QEvent * e );
    
    CollapsibleEffect *getEffectByIndex(int ix);
    
    /** @brief Delete currently selected effect. */
    void deleteCurrentEffect();
    
    /** @brief Palette was changed, update style. */
    void updatePalette();
    
    /** @brief Process dropped xml effect. */
    void processDroppedEffect(QDomElement e, QDropEvent *event);
    
    /** @brief Return the stylesheet required for effect parameters. */
    static const QString getStyleSheet();

protected:
    virtual void mouseMoveEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void resizeEvent ( QResizeEvent * event );
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dropEvent(QDropEvent *event);
  
private:
    Ui::EffectStack2_UI m_ui;
    ClipItem* m_clipref;
    QList <CollapsibleEffect*> m_effects;
    EffectsList m_currentEffectList;
    
    /** @brief Contains infos about effect like is it a track effect, which monitor displays it,... */
    EffectMetaInfo m_effectMetaInfo;
    
    /** @brief The last mouse click position, used to detect drag events. */
    QPoint m_clickPoint;

    /** @brief The track index of currently edited track. */
    int m_trackindex;

    /** If in track mode: Info of the edited track to be able to access its duration. */
    TrackInfo m_trackInfo;
    
    /** @brief The effect currently being dragged, NULL if no drag happening. */
    CollapsibleEffect *m_draggedEffect;
    
    /** @brief The effect currently being dragged, NULL if no drag happening. */
    CollapsibleGroup *m_draggedGroup;
    
    /** @brief The current number of groups. */
    int m_groupIndex;
    
    /** @brief The current effect may require an on monitor scene. */
    bool m_monitorSceneWanted;

    /** @brief Sets the list of effects according to the clip's effect list. */
    void setupListView();
    
    /** @brief Build the drag info and start it. */
    void startDrag();
    
    /** @brief Connect an effect to its signals. */
    void connectEffect(CollapsibleEffect *currentEffect);
    /** @brief Connect a group to its signals. */
    void connectGroup(CollapsibleGroup *group);

public slots:
    /** @brief Sets the clip whose effect list should be managed.
    * @param c Clip whose effect list should be managed */
    void slotClipItemSelected(ClipItem* c);

    void slotTrackItemSelected(int ix, const TrackInfo info);
   
    /** @brief Check if the mouse wheel events should be used for scrolling the widget view. */
    void slotCheckWheelEventFilter();

private slots:

    /** @brief Emits seekTimeline with position = clipstart + @param pos. */
    void slotSeekTimeline(int pos);


    /* @brief Define the region filter for current effect.
    void slotRegionChanged();*/

    /** @brief Checks whether the monitor scene has to be displayed. */
    void slotCheckMonitorPosition(int renderPos);

    void slotUpdateEffectParams(const QDomElement old, const QDomElement e, int ix);

    /** @brief Move an effect in the stack.
     * @param indexes The list of effect index in the stack
     * @param up true if we want to move effect up, false for down */
    void slotMoveEffectUp(QList <int> indexes, bool up);

    /** @brief Delete an effect in the stack. */
    void slotDeleteEffect(const QDomElement effect);
    
    /** @brief Delete all effect in a group. */
    void slotDeleteGroup(QDomDocument doc);

    /** @brief Pass position changes of the timeline cursor to the effects to keep their local timelines in sync. */
    void slotRenderPos(int pos);

    /** @brief Called whenever an effect is enabled / disabled by user. */
    void slotUpdateEffectState(bool disable, int index, bool needsMonitorEffectScene);

    void slotSetCurrentEffect(int ix);
    
    /** @brief Triggers a filter job on this clip. */
    void slotStartFilterJob(const QString&filterName, const QString&filterParams, const QString&consumer, const QString&consumerParams, const QMap <QString, QString> &extraParams);
    
    /** @brief Reset an effect to its default values. */
    void slotResetEffect(int ix);
    
    /** @brief Create a group containing effect with ix index. */
    void slotCreateGroup(int ix);
    
    /** @brief Create a region effect with ix index. */
    void slotCreateRegion(int ix, KUrl url);
    
    /** @brief Move an effect.
      ** @param currentIndexes the list of effect indexes to move in stack layout
      ** @param newIndex the position where the effects will be moved
      ** @param groupIndex the index of the group if any (-1 if none)
      ** @param groupName the name of the group to paste the effect
      */
    void slotMoveEffect(QList <int> currentIndexes, int newIndex, int groupIndex, QString groupName = QString());
    
    /** @brief Remove effects from a group */
    void slotUnGroup(CollapsibleGroup* group);
    
    /** @brief Add en effect to selected clip */
    void slotAddEffect(QDomElement effect);
    
    /** @brief Enable / disable all effects for the clip */
    void slotCheckAll(int state);
    
    /** @brief Update check all button status */
    void slotUpdateCheckAllButton();
    
    /** @brief Display additionnal effect info */
    void slotShowComments();
    
    /** @brief An effect group was renamed, update effects info */
    void slotRenameGroup(CollapsibleGroup *group);

signals:
    void removeEffect(ClipItem*, int, QDomElement);
    /**  Parameters for an effect changed, update the filter in playlist */
    void updateEffect(ClipItem*, int, QDomElement, QDomElement, int,bool);
    /** An effect in stack was moved, we need to regenerate
        all effects for this clip in the playlist */
    void refreshEffectStack(ClipItem *);
    /** Enable or disable an effect */
    void changeEffectState(ClipItem*, int, QList <int>, bool);
    /** An effect in stack was moved */
    void changeEffectPosition(ClipItem*, int, QList <int>, int);
    /** an effect was saved, reload list */
    void reloadEffects();
    /** An effect with position parameter was changed, seek */
    void seekTimeline(int);
    /** The region effect for current effect was changed */
    void updateClipRegion(ClipItem*, int, QString);
    void displayMessage(const QString&, int);
    void showComments(bool show);
    void startFilterJob(ItemInfo info, const QString &clipId, const QString &filterName, const QString &filterParams, const QString &consumer, const QString &consumerParams, const QMap<QString, QString> &extraParams);
    void addEffect(ClipItem*,QDomElement);
    void importClipKeyframes(GRAPHICSRECTITEM = AVWIDGET);
};

#endif
