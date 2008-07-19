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

#ifndef EFFECTSTACKVIEW_H
#define EFFECTSTACKVIEW_H

#include "ui_effectstack_ui.h"
#include "effectstackedit.h"
class EffectsList;
class ClipItem;

class EffectStackView : public QWidget {
    Q_OBJECT

public:
    EffectStackView(QWidget *parent = 0);
    void raiseWindow(QWidget*);
private:
    Ui::EffectStack_UI ui;
    ClipItem* clipref;
    QMap<QString, EffectsList*> effectLists;
    EffectStackEdit* effectedit;
    void setupListView(int ix);
    //void updateButtonStatus();

public slots:
    void slotClipItemSelected(ClipItem*);
    void slotUpdateEffectParams(const QDomElement&, const QDomElement&);

private slots:
    void slotItemSelectionChanged();
    void slotItemUp();
    void slotItemDown();
    void slotItemDel();
    void slotNewEffect();
    void slotResetEffect();
    void slotItemChanged(QListWidgetItem *item);
    void slotSaveEffect();

signals:
    void transferParamDesc(const QDomElement&, int , int);
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

};

#endif
