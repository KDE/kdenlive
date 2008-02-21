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

#include "ui_effectstack_ui.h"
#include "clipitem.h"
class EffectsList;


class EffectStackView : public QWidget
{
	Q_OBJECT
		
	public:
		EffectStackView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent=0);
	
private:
	int activeRow;
	QList<QDomElement> effects;
	Ui::EffectStack_UI ui;
	ClipItem* clipref;
	void setupListView();
	void updateButtonStatus();
	QMap<QString,EffectsList*> effectLists;

public slots:
	void slotClipItemSelected(ClipItem*);
	void slotItemSelectionChanged();
	void slotItemUp();
	void slotItemDown();
	void slotItemDel();
	void slotSetMoveX();
	void slotSetMoveY();
	void slotSetNew();
	void slotNewEffect();
	void slotSetHelp();
	void slotShowInTimeline();
	void slotParameterChanged(const QString&);
	void itemSelectionChanged();
	void slotUpdateEffectParams(QDomElement);
signals:
	void transferParamDesc(const QDomElement&,int ,int);
	void removeEffect(ClipItem*, QDomElement);
	void updateClipEffect(ClipItem*, QDomElement);

};

#endif
