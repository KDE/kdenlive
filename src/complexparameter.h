/***************************************************************************
                          complexparameter.h  -  description
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

#ifndef COMPLEXPARAM_H
#define COMPLEXPARAM_H

#include <KIcon>

#include "ui_keyframewidget_ui.h"
#include "clipitem.h"
class EffectsList;


class ComplexParameter : public QWidget
{
	Q_OBJECT
		
	public:
		ComplexParameter(QWidget *parent=0);
	
private:
	int activeRow;
	QList<QDomElement> effects;
	Ui::KeyframeWidget_UI ui;
	ClipItem* clipref;
	void setupListView();
	void updateButtonStatus();
	QMap<QString,EffectsList*> effectLists;

public slots:
	void slotSetMoveX();
	void slotSetMoveY();
	void slotSetNew();
	void slotSetHelp();
	void slotShowInTimeline();
	void slotParameterChanged(const QString&);
	void itemSelectionChanged();
	void setupParam(const QDomElement&,int,int);
signals:
	void transferParamDesc(const QDomElement&,int ,int);
	void removeEffect(ClipItem*, QDomElement);
	void updateClipEffect(ClipItem*, QDomElement);

};

#endif
