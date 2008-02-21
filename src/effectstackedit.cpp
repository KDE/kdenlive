/***************************************************************************
                          effecstackview.cpp  -  description
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

#include <KDebug>
#include <KLocale>
#include "effectstackedit.h"
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include "ui_constval_ui.h"

EffectStackEdit::EffectStackEdit(QGroupBox* gbox,QWidget *parent): QWidget(parent)
{
	
	vbox=new QVBoxLayout;
	gbox->setSizePolicy(QSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum));
	QVBoxLayout *tmpvbox=new QVBoxLayout;
	tmpvbox->addLayout(vbox);
	tmpvbox->addItem(new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Expanding));
	gbox->setLayout(tmpvbox);
	
}
void EffectStackEdit::transferParamDesc(const QDomElement& d,int ,int){
	params=d;
	QDomNodeList namenode = params.elementsByTagName("parameter");
	
	clearAllItems();
		QString outstr;
		QTextStream str(&outstr);
		d.save(str,2);
		kDebug() << outstr;
	for (int i=0;i< namenode.count() ;i++){
		QDomNode pa=namenode.item(i);
		QDomNode na=pa.firstChildElement("name");
		QString type=pa.attributes().namedItem("type").nodeValue();
		QWidget * toFillin=NULL,*labelToFillIn=NULL;
		if (type=="double" || type=="constant"){
			toFillin=new QWidget;
			Ui::Constval_UI *ctval=new Ui::Constval_UI;
			ctval->setupUi(toFillin);
			
			ctval->horizontalSlider->setMinimum(pa.attributes().namedItem("min").nodeValue().toInt());
			ctval->horizontalSlider->setMaximum(pa.attributes().namedItem("max").nodeValue().toInt());
			ctval->spinBox->setMinimum(ctval->horizontalSlider->minimum());
			ctval->spinBox->setMaximum(ctval->horizontalSlider->maximum());
			ctval->horizontalSlider->setValue(pa.attributes().namedItem("default").nodeValue().toInt());
			ctval->title->setText(na.toElement().text() );
			valueItems[na.toElement().text()]=ctval;
			connect (ctval->horizontalSlider, SIGNAL(valueChanged(int)) , this, SLOT (slotSliderMoved(int)));
			
		
		}

		if (toFillin){
			items.append(toFillin);
			vbox->addWidget(toFillin);
		}	
	}
}
void EffectStackEdit::collectAllParameters(){
	QDomNodeList namenode = params.elementsByTagName("parameter");

	for (int i=0;i< namenode.count() ;i++){
		QDomNode pa=namenode.item(i);
		QDomNode na=pa.firstChildElement("name");
		QString type=pa.attributes().namedItem("type").nodeValue();
		if (type=="double" || type=="constant"){
			QSlider* slider=((Ui::Constval_UI*)valueItems[na.toElement().text()])->horizontalSlider;
			pa.attributes().namedItem("value").setNodeValue(QString::number(slider->value()));
		}
	}
	emit parameterChanged(params);
}
void EffectStackEdit::slotSliderMoved(int){
	collectAllParameters();
}

void EffectStackEdit::clearAllItems(){
	foreach (QWidget* w,items){
		kDebug() << "delete" << w;
		vbox->removeWidget(w);
		delete w;
	}
	items.clear();
	valueItems.clear();
}
