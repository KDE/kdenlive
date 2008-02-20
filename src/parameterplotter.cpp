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
	colors << Qt::white << Qt::red << Qt::green << Qt::blue << Qt::magenta << Qt::gray << Qt::cyan;
	maxy=0;
	m_moveX=false;
	m_moveY=true;
	m_moveTimeline=true;
}

void ParameterPlotter::setPointLists(const QList< QPair<QString, QMap< int , QVariant > > >& params,int startframe, int endframe){
	
	QListIterator <QPair <QString, QMap< int , QVariant > > > nameit(params);
	int max_y=0;
	parameterNameList.clear();
	
	
	while (nameit.hasNext() ){	
		KPlotObject *plot=new KPlotObject(colors[plotobjects.size()%colors.size()]);
		plot->setShowLines(true);
		QPair<QString, QMap< int , QVariant > > item=nameit.next();
		parameterNameList << item.first;

		QMapIterator <int,QVariant> pointit=item.second;
		while (pointit.hasNext()){
			pointit.next();
			plot->addPoint(QPointF(pointit.key(),pointit.value().toDouble()),item.first,1);
			if (pointit.value().toInt() >maxy)
				max_y=pointit.value().toInt();
		}
		plotobjects.append(plot);
	}
	maxx=endframe;
	maxy=max_y;
	setLimits(0,endframe,0,maxy+10);
	addPlotObjects(plotobjects);
}

void ParameterPlotter::createParametersNew(){
	QList< QPair<QString, QMap<int,QVariant> > > ret;
	QList<KPlotObject*> plotobjs=plotObjects();
	if (plotobjs.size() != parameterNameList.size() ){
		kDebug() << "ERROR size not equal";
	}
	
	for (int i=0;i<parameterNameList.size() ;i++){
		QList<KPlotPoint*> points=plotobjs[i]->points();
		QMap<int,QVariant> vals;
		foreach (KPlotPoint *o,points){
			vals[o->x()]=o->y();
		}
		QPair<QString,QMap<int,QVariant> > pair("contrast",vals);
		ret.append(pair);
	}
	
	emit parameterChanged(ret);
	
}


void ParameterPlotter::mouseMoveEvent ( QMouseEvent * event ) {
	
	if (movepoint!=NULL){
		QList<KPlotPoint*> list=   pointsUnderPoint (event->pos()-QPoint(leftPadding(), topPadding() )  ) ;
		int i=0,j=-1;
		foreach (KPlotObject *o, plotObjects() ){
			QList<KPlotPoint*> points=o->points();
			for(int p=0;p<points.size();p++){
				if (points[p]==movepoint){
					QPoint delta=event->pos()-oldmousepoint;
					if (m_moveY)
					movepoint->setY(movepoint->y()-delta.y()*dataRect().height()/pixRect().height() );
					if (p>0 && p<points.size()-1){
						double newx=movepoint->x()+delta.x()*dataRect().width()/pixRect().width();
						if ( newx>points[p-1]->x() && newx<points[p+1]->x() && m_moveX)
							movepoint->setX(movepoint->x()+delta.x()*dataRect().width()/pixRect().width() );
					}
					replacePlotObject(i,o);
					oldmousepoint=event->pos();
				}
			}
			i++;
		}
		createParametersNew();
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

void ParameterPlotter::setMoveX(bool b){
	m_moveX=b;
}

void ParameterPlotter::setMoveY(bool b){
	m_moveY=b;
}

void ParameterPlotter::setMoveTimeLine(bool b){
	m_moveTimeline=b;
}

bool ParameterPlotter::isMoveX(){
	return m_moveX;
}

bool ParameterPlotter::isMoveY(){
	return m_moveY;
}

bool ParameterPlotter::isMoveTimeline(){
	return m_moveTimeline;
}
