/***************************************************************************
                          parameterplotter.cpp  -  description
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

#include "parameterplotter.h"
#include <QVariant>
#include <KPlotObject>
#include <QMouseEvent>
#include <KDebug>
#include <KPlotPoint>

ParameterPlotter::ParameterPlotter (QWidget *parent):KPlotWidget (parent){
	setAntialiasing(true);
	setLeftPadding(20);
	setRightPadding(10);
	setTopPadding(10);
	setBottomPadding(20);
	movepoint=NULL;
	plot=new KPlotObject();
	plot->setShowLines(true);
}

void ParameterPlotter::setPointLists(const QList< QPair<QString, QMap< int , QVariant > > >& params,int startframe, int endframe){
	
	QListIterator <QPair <QString, QMap< int , QVariant > > > nameit(params);
	int maxy=0;
	while (nameit.hasNext() ){
	
		QPair<QString, QMap< int , QVariant > > item=nameit.next();
		QString name(item.first);
		QMapIterator <int,QVariant> pointit=item.second;
		while (pointit.hasNext()){
			pointit.next();
			plot->addPoint(QPointF(pointit.key(),pointit.value().toDouble()),"",1);
			if (pointit.value().toInt() >maxy)
				maxy=pointit.value().toInt();
		}
		addPlotObject(plot);
	}
	
	setLimits(0,endframe,0,maxy);
	
}

QList< QPair<QString, QMap<int,QVariant> > > ParameterPlotter::getPointLists(){
	return pointlists;
}

void ParameterPlotter::mouseMoveEvent ( QMouseEvent * event ) {
	
	if (movepoint!=NULL){
		QList<KPlotPoint*> list=   pointsUnderPoint (event->pos()-QPoint(leftPadding(), topPadding() )  ) ;
		int i=0,j=-1;
		foreach (KPlotObject *o, plotObjects() ){
			foreach (KPlotPoint* p, o->points()){
				if (p==movepoint){
					QPoint delta=event->pos()-oldmousepoint;
					movepoint->setY(movepoint->y()-delta.y()*dataRect().height()/pixRect().height() );
					
					movepoint->setX(movepoint->x()+delta.x()*dataRect().width()/pixRect().width() );
					replacePlotObject(i,o);
					oldmousepoint=event->pos();
				}
			}
			i++;
		}
	}
}

void ParameterPlotter::mousePressEvent ( QMouseEvent * event ) {
	QList<KPlotPoint*> list=   pointsUnderPoint (event->pos()-QPoint(leftPadding(), topPadding() )  ) ;
	if (list.size() > 0){
		movepoint=list[0];
		oldmousepoint=event->pos();
	}else
		movepoint=NULL;
}
