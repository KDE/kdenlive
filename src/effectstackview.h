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

#include <KIcon>
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
	int activeRow;
	QMap<int,QDomElement> effects;
	Ui::EffectStack_UI ui;
	ClipItem* clipref;
	void setupListView();
	void updateButtonStatus();
	QMap<QString,EffectsList*> effectLists;
	EffectStackEdit* effectedit;
	void renumberEffects();
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
	void updateClipEffect(ClipItem*, QDomElement);

};

#endif
