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

ParameterPlotter::ParameterPlotter (QWidget *parent):QWidget(parent){
	setupUi(this);
	kplotwidget=new PlotWrapper(this);
	QVBoxLayout *vbox=new QVBoxLayout (this);
	vbox->addWidget(kplotwidget);
	widget->setLayout(vbox);
	
	kplotwidget->setAntialiasing(true);
	kplotwidget->setLeftPadding(20);
	kplotwidget->setRightPadding(10);
	kplotwidget->setTopPadding(10);
	kplotwidget->setBottomPadding(20);
	movepoint=NULL;
	colors << Qt::white << Qt::red << Qt::green << Qt::blue << Qt::magenta << Qt::gray << Qt::cyan;
	maxy=0;
	m_moveX=false;
	m_moveY=true;
	m_moveTimeline=true;
	m_newPoints=false;
	activeIndexPlot=-1;
	buttonLeftRight->setIcon(KIcon("go-next"));//better icons needed
	buttonLeftRight->setToolTip(i18n("Allow horizontal moves"));
	buttonUpDown->setIcon(KIcon("go-up"));
	buttonUpDown->setToolTip(i18n("Allow vertical moves"));
	buttonShowInTimeline->setIcon(KIcon("kmplayer"));
	buttonShowInTimeline->setToolTip(i18n("Show keyframes in timeline"));
	buttonHelp->setIcon(KIcon("help-about"));
	buttonHelp->setToolTip(i18n("Parameter info"));
	buttonNewPoints->setIcon(KIcon("xedit"));
	buttonNewPoints->setToolTip(i18n("Add keyframe"));
	infoBox->hide();
	connect (buttonLeftRight, SIGNAL (clicked()), this , SLOT ( slotSetMoveX() ) );
	connect (buttonUpDown, SIGNAL (clicked()), this , SLOT ( slotSetMoveY() ) );
	connect (buttonShowInTimeline, SIGNAL (clicked()), this , SLOT ( slotShowInTimeline() ) );
	connect (buttonNewPoints, SIGNAL (clicked()), this , SLOT ( slotSetNew() ) );
	connect (buttonHelp, SIGNAL (clicked()), this , SLOT ( slotSetHelp() ) );
	connect (parameterList, SIGNAL (currentIndexChanged ( const QString &  ) ), this, SLOT( slotParameterChanged(const QString&) ) );
	updateButtonStatus();
}
/*
    <name>Lines</name>
    <description>Lines from top to bottom</description>
    <author>Marco Gittler</author>
    <properties tag="lines" id="lines" />
    <parameter default="5" type="constant" value="5" min="0" name="num" max="255" >
      <name>Num</name>
    </parameter>
    <parameter default="4" type="constant" value="4" min="0" name="width" max="255" >
      <name>Width</name>
    </parameter>
  </effect>

*/
void ParameterPlotter::setPointLists(const QDomElement& d,int startframe,int endframe){
	
	//QListIterator <QPair <QString, QMap< int , QVariant > > > nameit(params);
	itemParameter=d;
	QDomNodeList namenode = d.elementsByTagName("parameter");
	
	int max_y=0;
	kplotwidget->removeAllPlotObjects ();
	parameterNameList.clear();
	plotobjects.clear();
	

	for (int i=0;i< namenode.count() ;i++){
		KPlotObject *plot=new KPlotObject(colors[plotobjects.size()%colors.size()]);
		plot->setShowLines(true);
		//QPair<QString, QMap< int , QVariant > > item=nameit.next();
		QDomNode pa=namenode.item(i);
		QDomNode na=pa.firstChildElement("name");
		
		parameterNameList << na.toElement().text();
		
		
		max_y=pa.attributes().namedItem("max").nodeValue().toInt();
		int val=pa.attributes().namedItem("value").nodeValue().toInt();
		plot->addPoint((i+1)*20,val);
		/*TODO keyframes
		while (pointit.hasNext()){
			pointit.next();
			plot->addPoint(QPointF(pointit.key(),pointit.value().toDouble()),item.first,1);
			if (pointit.value().toInt() >maxy)
				max_y=pointit.value().toInt();
		}*/
		plotobjects.append(plot);
	}
	maxx=endframe;
	maxy=max_y;
	kplotwidget->setLimits(0,endframe,0,maxy+10);
	kplotwidget->addPlotObjects(plotobjects);

}

void ParameterPlotter::createParametersNew(){
	
	QList<KPlotObject*> plotobjs=kplotwidget->plotObjects();
	if (plotobjs.size() != parameterNameList.size() ){
		kDebug() << "ERROR size not equal";
	}
	QDomNodeList namenode = itemParameter.elementsByTagName("parameter");
	for (int i=0;i<namenode.count() ;i++){
		QList<KPlotPoint*> points=plotobjs[i]->points();
		QDomNode pa=namenode.item(i);
		
		
		
		
		
		
		QMap<int,QVariant> vals;
		foreach (KPlotPoint *o,points){
			//vals[o->x()]=o->y();
			pa.attributes().namedItem("value").setNodeValue(QString::number(o->y()));
		}
		QPair<QString,QMap<int,QVariant> > pair("contrast",vals);
		//ret.append(pair);
	}
	
	emit parameterChanged(itemParameter);
	
}


void ParameterPlotter::mouseMoveEvent ( QMouseEvent * event ) {
	
	if (movepoint!=NULL){
		QList<KPlotPoint*> list=   kplotwidget->pointsUnderPoint (event->pos()-QPoint(kplotwidget->leftPadding(), kplotwidget->topPadding() )  ) ;
		int i=0,j=-1;
		foreach (KPlotObject *o, kplotwidget->plotObjects() ){
			QList<KPlotPoint*> points=o->points();
			for(int p=0;p<points.size();p++){
				if (points[p]==movepoint){
					QPoint delta=event->pos()-oldmousepoint;
					if (m_moveY)
						movepoint->setY(movepoint->y()-delta.y()*kplotwidget->dataRect().height()/kplotwidget->pixRect().height() );
					if (p>0 && p<points.size()-1){
						double newx=movepoint->x()+delta.x()*kplotwidget->dataRect().width()/kplotwidget->pixRect().width();
						if ( newx>points[p-1]->x() && newx<points[p+1]->x() && m_moveX)
							movepoint->setX(movepoint->x()+delta.x()*kplotwidget->dataRect().width()/kplotwidget->pixRect().width() );
					}
					if (m_moveTimeline && (m_moveX|| m_moveY) )
						emit updateFrame(0);
					kplotwidget->replacePlotObject(i,o);
					oldmousepoint=event->pos();
				}
			}
			i++;
		}
		createParametersNew();
	}
}

void ParameterPlotter::replot(const QString & name){
	//removeAllPlotObjects();
	int i=0;
	bool drawAll=name.isEmpty() || name=="all";
	activeIndexPlot=-1;
	foreach (KPlotObject* p,kplotwidget->plotObjects() ){
		p->setShowPoints(drawAll || parameterNameList[i]==name);
		p->setShowLines(drawAll || parameterNameList[i]==name);
		if ( parameterNameList[i]==name )
			activeIndexPlot = i;
		kplotwidget->replacePlotObject(i++,p);
	}
}

void ParameterPlotter::mousePressEvent ( QMouseEvent * event ) {
	//topPadding and other padding can be wrong and this (i hope) will be correctet in newer kde versions
	QPoint inPlot=event->pos()-QPoint(kplotwidget->leftPadding(), kplotwidget->topPadding() );
	QList<KPlotPoint*> list=   kplotwidget->pointsUnderPoint (inPlot ) ;
	if (event->button()==Qt::LeftButton){
		if (list.size() > 0){
			movepoint=list[0];
			oldmousepoint=event->pos();
		}else{
			if (m_newPoints && activeIndexPlot>=0){
				//setup new points
				KPlotObject* p=kplotwidget->plotObjects()[activeIndexPlot];
				QList<KPlotPoint*> points=p->points();
				QList<QPointF> newpoints;
				
				double newx=inPlot.x()*kplotwidget->dataRect().width()/kplotwidget->pixRect().width();
				double newy=(height()-inPlot.y()-kplotwidget->bottomPadding()-kplotwidget->topPadding() )*kplotwidget->dataRect().height()/kplotwidget->pixRect().height();
				bool inserted=false;
				foreach (KPlotPoint* pt,points){
					if (pt->x() >newx && !inserted){
						newpoints.append(QPointF(newx,newy));
						inserted=true;
					}
					newpoints.append(QPointF(pt->x(),pt->y()));
				}
				p->clearPoints();
				foreach (QPointF qf, newpoints ){
					p->addPoint(qf);
				}
				kplotwidget->replacePlotObject(activeIndexPlot,p);
			}
			movepoint=NULL;
		}
	}else if (event->button()==Qt::LeftButton){
		//menu for deleting or exact setup of point
	}
}


void ParameterPlotter::slotSetMoveX(){
	m_moveX=!m_moveX;
	updateButtonStatus();
}

void ParameterPlotter::slotSetMoveY(){
	m_moveY=!m_moveY;
	updateButtonStatus();
}

void ParameterPlotter::slotSetNew(){
	m_newPoints=!m_newPoints;
	updateButtonStatus();
}

void ParameterPlotter::slotSetHelp(){
	infoBox->setVisible(!infoBox->isVisible());
	buttonHelp->setDown(infoBox->isVisible());
}

void ParameterPlotter::slotShowInTimeline(){
	
	m_moveTimeline=!m_moveTimeline;
	updateButtonStatus();
	
}

void ParameterPlotter::updateButtonStatus(){
	buttonLeftRight->setDown(m_moveY);
	buttonUpDown->setDown(m_moveX);
	
	buttonShowInTimeline->setEnabled( m_moveX || m_moveY);
	buttonShowInTimeline->setDown(m_moveTimeline);
	
	///TODO buttonNewPoints->setEnabled(currentText()!="all");
	buttonNewPoints->setDown(m_newPoints);
}

void ParameterPlotter::slotParameterChanged(const QString& text){
	
	//ui.buttonNewPoints->setEnabled(text!="all");
	replot(text);
	updateButtonStatus();
/*
ui.parameterList->clear();
		ui.parameterList->addItem("all");
		QDomNodeList namenode = effects.at(activeRow).elementsByTagName("parameter");
		for (int i=0;i<namenode.count();i++){
			QDomNode pa=namenode.item(i);
			QDomNode na=pa.firstChildElement("name");
			ui.parameterList->addItem(na.toElement().text() );
		}*/
}
