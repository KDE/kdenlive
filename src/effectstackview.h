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

#include "clipitem.h"
#include "ui_effectstack_ui.h"
#include "effectstackedit.h"
class EffectsList;


class EffectStackView : public QWidget
{
	Q_OBJECT
		
	public:
		EffectStackView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent=0);
	
private:
	Ui::EffectStack_UI ui;
	ClipItem* clipref;
	QMap<QString,EffectsList*> effectLists;
	EffectStackEdit* effectedit;
	void setupListView();
	void updateButtonStatus();

public slots:
	void slotClipItemSelected(ClipItem*);
	void slotItemSelectionChanged();
	void slotItemUp();
	void slotItemDown();
	void slotItemDel();
	void slotNewEffect();
	void itemSelectionChanged();
	void slotUpdateEffectParams(const QDomElement&);

signals:
	void transferParamDesc(const QDomElement&,int ,int);
	void removeEffect(ClipItem*, QDomElement);
	/**  Parameters for an effect changed, update the filter in playlist */
	void updateClipEffect(ClipItem*, QDomElement);
	/** An effect in stack was moved, we need to regenerate 
	    all effects for this clip in the playlist */
	void refreshEffectStack(ClipItem *);

};

#endif
