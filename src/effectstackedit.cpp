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
#include <QCheckBox>
#include "ui_constval_ui.h"
#include "ui_listval_ui.h"
#include "ui_boolval_ui.h"

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
	kDebug() << "in";
	params=d;
	QDomNodeList namenode = params.elementsByTagName("parameter");
	
	clearAllItems();
		QString outstr;
		QTextStream str(&outstr);
		d.save(str,2);
		kDebug() << outstr;
	for (int i=0;i< namenode.count() ;i++){
		kDebug() << "in form";
		QDomNode pa=namenode.item(i);
		QDomNode na=pa.firstChildElement("name");
		QDomNamedNodeMap nodeAtts=pa.attributes();
		QString type=nodeAtts.namedItem("type").nodeValue();
		QWidget * toFillin=NULL,*labelToFillIn=NULL;
		//TODO constant, list, bool, complex , color, geometry, position
		if (type=="double" || type=="constant"){
			toFillin=new QWidget;
			Ui::Constval_UI *ctval=new Ui::Constval_UI;
			ctval->setupUi(toFillin);
			
			ctval->horizontalSlider->setMinimum(nodeAtts.namedItem("min").nodeValue().toInt());
			ctval->horizontalSlider->setMaximum(nodeAtts.namedItem("max").nodeValue().toInt());
			ctval->spinBox->setMinimum(ctval->horizontalSlider->minimum());
			ctval->spinBox->setMaximum(ctval->horizontalSlider->maximum());
			if (nodeAtts.namedItem("value").isNull())
				ctval->horizontalSlider->setValue(nodeAtts.namedItem("default").nodeValue().toInt());
			else
				ctval->horizontalSlider->setValue(nodeAtts.namedItem("value").nodeValue().toInt());
			ctval->title->setTitle(na.toElement().text() );
			valueItems[na.toElement().text()]=ctval;
			connect (ctval->horizontalSlider, SIGNAL(valueChanged(int)) , this, SLOT (slotSliderMoved(int)));
		}
		if (type=="list"){
			toFillin=new QWidget;
			Ui::Listval_UI *lsval=new Ui::Listval_UI;
			lsval->setupUi(toFillin);
			nodeAtts.namedItem("paramlist");
			
			lsval->list->addItems(nodeAtts.namedItem("paramlist").nodeValue().split(","));
			/*if (nodeAtts.namedItem("value").isNull())
				lsval->list->setCurrentText(nodeAtts.namedItem("default").nodeValue());
			else
				lsval->list->setCurrentText(nodeAtts.namedItem("value").nodeValue());
			*/
			connect (lsval->list, SIGNAL(currentIndexChanged(int)) , this, SLOT (slotSliderMoved(int)));
			lsval->title->setTitle(na.toElement().text() );
			valueItems[na.toElement().text()]=lsval;
		}
		if (type=="bool"){
			toFillin=new QWidget;
			Ui::Boolval_UI *bval=new Ui::Boolval_UI;
			bval->setupUi(toFillin);
			if (nodeAtts.namedItem("value").isNull())
				bval->checkBox->setCheckState(nodeAtts.namedItem("default").nodeValue()=="0" ? Qt::Unchecked : Qt::Checked);
			else
				bval->checkBox->setCheckState(nodeAtts.namedItem("value").nodeValue()=="0" ? Qt::Unchecked : Qt::Checked);		
			
			connect (bval->checkBox, SIGNAL(stateChanged(int)) , this, SLOT (slotSliderMoved(int)));
			bval->title->setTitle(na.toElement().text() );
			valueItems[na.toElement().text()]=bval;
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
		if (type=="list"){
			KComboBox *box=((Ui::Listval_UI*)valueItems[na.toElement().text()])->list;
			pa.attributes().namedItem("value").setNodeValue(box->currentText());
		}
		if (type=="bool"){
			QCheckBox *box=((Ui::Boolval_UI*)valueItems[na.toElement().text()])->checkBox;
			pa.attributes().namedItem("value").setNodeValue(box->checkState() == Qt::Checked ? "1" :"0" );
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
