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
#include <QMap>

class Transform{
	public:
		Transform(){
			scalex=1.0;
			scaley=1.0;
			rotate=0.0;
		}
		double scalex,scaley;
		double rotate;
};

class TitleWidget : public QDialog , public Ui::TitleWidget_UI{
	Q_OBJECT
public:
	TitleWidget(QDialog *parent=0);
private:
	QGraphicsPolygonItem *startViewport,*endViewport;
	void initViewports();
	QMap<QGraphicsItem*,Transform > transformations;
public slots:
	void slotNewText();
	void slotNewRect();
	void slotChangeBackground();
	void selectionChanged();
	void textChanged();
	void rectChanged();
	void fontBold();
	void setupViewports();
	void zIndexChanged(int);
	void svgSelected(const KUrl&);
	void itemScaled(int);
	void itemRotate(int);
};


#endif
