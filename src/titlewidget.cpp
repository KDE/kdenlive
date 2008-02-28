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

#include "titlewidget.h"
#include "graphicsscenerectmove.h"
#include <QGraphicsView>
#include <KDebug>
#include <QGraphicsItem>
int settingUp=false;

TitleWidget::TitleWidget (QDialog *parent):QDialog(parent){
	setupUi(this);
	connect (newTextButton,SIGNAL(clicked()), this, SLOT( slotNewText()));
	connect (newRectButton,SIGNAL(clicked()), this, SLOT( slotNewRect()));
	connect (kcolorbutton, SIGNAL ( clicked()), this, SLOT( slotChangeBackground()) ) ;
	connect (horizontalSlider, SIGNAL ( valueChanged(int) ), this, SLOT( slotChangeBackground()) ) ;
	connect (ktextedit, SIGNAL(textChanged()), this , SLOT (textChanged()));
	connect (fontColorButton, SIGNAL ( clicked()), this, SLOT( textChanged()) ) ;
	//connect (fontBold, SIGNAL ( clicked()), this, SLOT( setBold()) ) ;
	
	connect (kfontrequester, SIGNAL ( fontSelected(const QFont &)), this, SLOT( textChanged()) ) ;
	connect(textAlpha, SIGNAL( valueChanged(int) ), this, SLOT (textChanged()));
	//connect (ktextedit, SIGNAL(selectionChanged()), this , SLOT (textChanged()));
	
	connect(rectFAlpha, SIGNAL( valueChanged(int) ), this, SLOT (rectChanged()));
	connect(rectBAlpha, SIGNAL( valueChanged(int) ), this, SLOT (rectChanged()));
	connect(rectFColor, SIGNAL( clicked() ), this, SLOT (rectChanged()));
	connect(rectBColor, SIGNAL( clicked() ), this, SLOT (rectChanged()));
	connect(rectLineWidth, SIGNAL( valueChanged(int) ), this, SLOT (rectChanged()));
	
	connect (startViewportX,SIGNAL(valueChanged(int)), this, SLOT( setupViewports()));
	connect (startViewportY,SIGNAL(valueChanged(int)), this, SLOT( setupViewports()));
	connect (startViewportSize,SIGNAL(valueChanged(int)), this, SLOT( setupViewports()));
	connect (endViewportX,SIGNAL(valueChanged(int)), this, SLOT( setupViewports()));
	connect (endViewportY,SIGNAL(valueChanged(int)), this, SLOT( setupViewports()));
	connect (endViewportSize,SIGNAL(valueChanged(int)), this, SLOT( setupViewports()));	
	
	GraphicsSceneRectMove *scene=new GraphicsSceneRectMove(this);
	
	
	
 // a gradient background
	QRadialGradient *gradient=new QRadialGradient(0, 0, 10);
	gradient->setSpread(QGradient::RepeatSpread);
	//scene->setBackgroundBrush(*gradient);
	
	graphicsView->setScene(scene);
	connect (graphicsView->scene(), SIGNAL (selectionChanged()), this , SLOT( selectionChanged()));
	initViewports();
	
	graphicsView->show();
	graphicsView->setRenderHint(QPainter::Antialiasing);
	graphicsView->setInteractive(true);
	graphicsView->resize(400, 300);
	//update();
}
	       
void TitleWidget::initViewports(){
	startViewport=new QGraphicsPolygonItem(QPolygonF(QRectF(0,0,0,0)));
	endViewport=new QGraphicsPolygonItem(QPolygonF(QRectF(0,0,0,0)));
	
	QPen startpen(Qt::DotLine);
	QPen endpen(Qt::DashDotLine);
	startpen.setColor(QColor(100,200,100,140));
	endpen.setColor(QColor(200,100,100,140));
	
	startViewport->setPen(startpen);
	endViewport->setPen(endpen);
	
	startViewportSize->setValue(40);
	endViewportSize->setValue(40);
	
	graphicsView->scene()->addItem(startViewport);
	graphicsView->scene()->addItem(endViewport);
}
	       
void TitleWidget::slotNewRect(){
	
	QGraphicsRectItem * ri=graphicsView->scene()->addRect(-50,-50,100,100);
	ri->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);
}
	       
void TitleWidget::slotNewText(){
	QGraphicsTextItem *tt=graphicsView->scene()->addText("Text here");
	tt->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);
	tt->setTextInteractionFlags (Qt::TextEditorInteraction);
	connect (tt->document(), SIGNAL (contentsChanged()), this, SLOT(selectionChanged()));
	kDebug() << tt->metaObject()->className();
	/*QGraphicsRectItem * ri=graphicsView->scene()->addRect(-50,-50,100,100);
	ri->setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);*/
	
}

void TitleWidget::selectionChanged(){
	QList<QGraphicsItem*> l=graphicsView->scene()->selectedItems();
	if (l.size()==1){
		kDebug() << (l[0])->type();
		if ((l[0])->type()==8  ){
			QGraphicsTextItem* i=((QGraphicsTextItem*)l[0]);
			if (l[0]->hasFocus() )
			ktextedit->setHtml(i->toHtml());
			toolBox->setCurrentIndex(1);
		}else
		if ((l[0])->type()==3){
			settingUp=true;
			QGraphicsRectItem *rec=((QGraphicsRectItem*)l[0]);
			toolBox->setCurrentIndex(2);
			rectFAlpha->setValue(rec->pen().color().alpha());
			rectBAlpha->setValue(rec->brush().isOpaque() ? rec->brush().color().alpha() : 0);
			kDebug() << rec->brush().color().alpha();
			QColor fcol=rec->pen().color();
			QColor bcol=rec->brush().color();
			//fcol.setAlpha(255);
			//bcol.setAlpha(255);
			rectFColor->setColor(fcol);
			rectBColor->setColor(bcol);
			settingUp=false;
			rectLineWidth->setValue(rec->pen().width());
		}
		else{
			//toolBox->setCurrentIndex(0);
		}
	}
}

void TitleWidget::slotChangeBackground(){
	QColor color=kcolorbutton->color();
	color.setAlpha(horizontalSlider->value());
	graphicsView->scene()->setBackgroundBrush(QBrush(color));
}

void TitleWidget::textChanged(){
	QList<QGraphicsItem*> l=graphicsView->scene()->selectedItems();
	if (l.size()==1 && (l[0])->type()==8 && !l[0]->hasFocus()){
		kDebug() << ktextedit->document()->toHtml();
		((QGraphicsTextItem*)l[0])->setHtml(ktextedit->toHtml());
	}
}

void TitleWidget::rectChanged(){
	QList<QGraphicsItem*> l=graphicsView->scene()->selectedItems();
	if (l.size()==1 && (l[0])->type()==3 && !settingUp){
		QGraphicsRectItem *rec=(QGraphicsRectItem*)l[0];
		QColor f=rectFColor->color();
		f.setAlpha(rectFAlpha->value());
		QPen penf(f);
		penf.setWidth(rectLineWidth->value());
		rec->setPen(penf);
		QColor b=rectBColor->color();
		b.setAlpha(rectBAlpha->value());
		rec->setBrush(QBrush(b));
	}
}

void TitleWidget::fontBold(){
	QList<QGraphicsItem*> l=graphicsView->scene()->selectedItems();
	if (l.size()==1 && (l[0])->type()==8 && !l[0]->hasFocus()){
		//ktextedit->document()->setTextOption();
	}
}

void TitleWidget::setupViewports(){
	double aspect_ratio=4.0/3.0;//read from project
	
	QRectF sp(startViewportX->value(), startViewportY->value(),0,0);
	QRectF ep(endViewportX->value(),endViewportY->value(),0,0);
	
	double sv_size=startViewportSize->value();
	double ev_size=endViewportSize->value();
	sp.adjust(-sv_size,-sv_size/aspect_ratio,sv_size,sv_size/aspect_ratio);
	ep.adjust(-ev_size,-ev_size/aspect_ratio,ev_size,ev_size/aspect_ratio);	
	
	startViewport->setPolygon(QPolygonF(sp));
	endViewport->setPolygon(QPolygonF(ep));
	
	
	
	
}
#include "moc_titlewidget.cpp"

