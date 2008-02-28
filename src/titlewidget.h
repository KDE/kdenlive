/***************************************************************************
                          titlewidget.h  -  description
                             -------------------
    begin                : Feb 28 2008
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

#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H

#include "ui_titlewidget_ui.h"
#include <QDialog>

class TitleWidget : public QDialog , public Ui::TitleWidget_UI{
	Q_OBJECT
public:
	TitleWidget(QDialog *parent=0);
private:
	QGraphicsPolygonItem *startViewport,*endViewport;
public slots:
	void slotNewText();
	void slotNewRect();
	void slotChangeBackground();
	void selectionChanged();
	void textChanged();
	void rectChanged();
	void fontBold();
	void setupViewports();
};


#endif
