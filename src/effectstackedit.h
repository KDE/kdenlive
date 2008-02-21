/***************************************************************************
                          effecstackedit.h  -  description
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

#ifndef EFFECTSTACKEDIT_H
#define EFFECTSTACKEDIT_H

#include <QWidget>
#include <QDomElement>
#include <QVBoxLayout>
#include <QList>
#include <QGroupBox>
#include <QMap>


class EffectStackEdit : public QWidget
{
	Q_OBJECT
public:
	EffectStackEdit(QGroupBox* gbox,QWidget *parent );
private:
	void clearAllItems();
	QVBoxLayout *vbox;
	QList<QWidget*> items;
	QDomElement params;
	QMap<QString,void*> valueItems;
	void collectAllParameters();
public slots:
	void transferParamDesc(const QDomElement&,int ,int);
	void slotSliderMoved(int);
signals:
	void parameterChanged(const QDomElement& );
};
	
#endif
