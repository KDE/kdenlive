/***************************************************************************
                          effecstackview.h  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
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
 * @class EffectStackView
 * @brief View part of the EffectStack
 * @author Marco Gittler
 */

#ifndef EFFECTSTACKVIEW_H
#define EFFECTSTACKVIEW_H

#include "ui_effectstack_ui.h"
#include "effectstackedit.h"

class EffectsList;
class ClipItem;
class MltVideoProfile;

class EffectStackView : public QWidget
{
    Q_OBJECT

public:
    EffectStackView(QWidget *parent = 0);
    virtual ~EffectStackView();

    /** @brief Raises @param dock if a clip is loaded. */
    void raiseWindow(QWidget* dock);

    /** @brief Clears the list of effects and updates the buttons accordingly. */
    void clear();

    /** @brief Sets the add effect button's menu to @param menu. */
    void setMenu(QMenu *menu);

    /** @brief Passes updates on @param profile and @param t on to the effect editor. */
    void updateProjectFormat(MltVideoProfile profile, Timecode t);

    /** @brief Tells the effect editor to update its timecode format. */
    void updateTimecodeFormat();

private:
    Ui::EffectStack_UI m_ui;
    ClipItem* m_clipref;
    QMap<QString, EffectsList*> m_effectLists;
    EffectStackEdit* m_effectedit;

    /** @brief Sets the list of effects according to the clip's effect list.
    * @param ix Number of the effect to preselect */
    void setupListView(int ix);

public slots:
    /** @brief Sets the clip whose effect list should be managed.
    * @param c Clip whose effect list should be managed
    * @param ix Effect to preselect */
    void slotClipItemSelected(ClipItem* c, int ix);
    
    /** @brief Emits updateClipEffect.
    * @param old Old effect information
    * @param e New effect information
    *
    * Connected to a parameter change in the editor */
    void slotUpdateEffectParams(const QDomElement old, const QDomElement e);

    /** @brief Removes the selected effect. */
    void slotItemDel();

private slots:
    /** @brief Updates buttons and the editor according to selected effect.
    * @param update (optional) Set the clip's selected effect (display keyframes in timeline) */
    void slotItemSelectionChanged(bool update = true);

    /** @brief Moves the selected effect upwards. */
    void slotItemUp();

    /** @brief Moves the selected effect downwards. */
    void slotItemDown();

    /** @brief Resets the selected effect to its default values. */
    void slotResetEffect();

    /** @brief Updates effect @param item if it was enabled or disabled. */
    void slotItemChanged(QListWidgetItem *item);

    /** @brief Saves the selected effect's values to a custom effect.
    *
    * TODO: save all effects into one custom effect */
    void slotSaveEffect();

    /** @brief Emits seekTimeline with position = clipstart + @param pos. */
    void slotSeekTimeline(int pos);

    /** @brief Makes the check all checkbox represent the check state of the effects. */
    void slotUpdateCheckAllButton();

    /** @brief Sets the check state of all effects according to @param state. */
    void slotCheckAll(int state);

signals:
    void removeEffect(ClipItem*, QDomElement);
    /**  Parameters for an effect changed, update the filter in playlist */
    void updateClipEffect(ClipItem*, QDomElement, QDomElement, int);
    /** An effect in stack was moved, we need to regenerate
        all effects for this clip in the playlist */
    void refreshEffectStack(ClipItem *);
    /** Enable or disable an effect */
    void changeEffectState(ClipItem*, int, bool);
    /** An effect in stack was moved */
    void changeEffectPosition(ClipItem*, int, int);
    /** an effect was saved, reload list */
    void reloadEffects();
    /** An effect with position parameter was changed, seek */
    void seekTimeline(int);
};

#endif
