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
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include "ui_constval_ui.h"
#include "ui_listval_ui.h"
#include "ui_boolval_ui.h"
#include "ui_colorval_ui.h"
#include "complexparameter.h"

EffectStackEdit::EffectStackEdit(QFrame* frame,QWidget *parent): QObject(parent)
{
	QScrollArea *area;
	QVBoxLayout *vbox1=new QVBoxLayout(frame);
	QVBoxLayout *vbox2=new QVBoxLayout(frame);
	vbox=new QVBoxLayout(frame);
	vbox1->setContentsMargins (0,0,0,0);
	vbox1->setSpacing(0);
	vbox2->setContentsMargins (0,0,0,0);
	vbox2->setSpacing(0);
	vbox->setContentsMargins (0,0,0,0);
	vbox->setSpacing(0);
	frame->setLayout(vbox1);
	QFont widgetFont = frame->font();
	widgetFont.setPointSize(widgetFont.pointSize() - 2);
	frame->setFont(widgetFont);
	
	area=new QScrollArea(frame);
	QWidget *wid=new QWidget(area);
	area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	wid->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum));
	//area->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::MinimumExpanding));

	vbox1->addWidget(area);
	wid->setLayout(vbox2);
	vbox2->addLayout(vbox);
	vbox2->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
	area->setWidget(wid);
	area->setWidgetResizable(true);
	wid->show();
	
}
void EffectStackEdit::transferParamDesc(const QDomElement& d,int ,int){
	kDebug() << "in";
	params=d;
	QDomNodeList namenode = params.elementsByTagName("parameter");
	
	clearAllItems();

	for (int i=0;i< namenode.count() ;i++){
		kDebug() << "in form";
		QDomNode pa=namenode.item(i);
		QDomNode na=pa.firstChildElement("name");
		QDomNamedNodeMap nodeAtts=pa.attributes();
		QString type=nodeAtts.namedItem("type").nodeValue();
		QString paramName=na.toElement().text();
		QWidget * toFillin=new QWidget;
		QString value=nodeAtts.namedItem("value").isNull()?
			nodeAtts.namedItem("default").nodeValue():
			nodeAtts.namedItem("value").nodeValue();
		
		//TODO constant, list, bool, complex , color, geometry, position
		if (type=="double" || type=="constant"){
			createSliderItem(paramName,value.toInt(),nodeAtts.namedItem("min").nodeValue().toInt(),nodeAtts.namedItem("max").nodeValue().toInt() );
			delete toFillin;
			toFillin=NULL;
		}else if (type=="list"){
			
			Ui::Listval_UI *lsval=new Ui::Listval_UI;
			lsval->setupUi(toFillin);
			nodeAtts.namedItem("paramlist");
			QStringList listitems=nodeAtts.namedItem("paramlist").nodeValue().split(",");
			lsval->list->addItems(listitems);
			lsval->list->setCurrentIndex(listitems.indexOf(value));;
			connect (lsval->list, SIGNAL(currentIndexChanged(int)) , this, SLOT (collectAllParameters()));
			lsval->title->setTitle(na.toElement().text() );
			valueItems[paramName]=lsval;
			uiItems.append(lsval);
		}else if (type=="bool"){
			Ui::Boolval_UI *bval=new Ui::Boolval_UI;
			bval->setupUi(toFillin);
			bval->checkBox->setCheckState(value=="0" ? Qt::Unchecked : Qt::Checked);
			
			connect (bval->checkBox, SIGNAL(stateChanged(int)) , this, SLOT (collectAllParameters()));
			bval->checkBox->setText(na.toElement().text() );
			valueItems[paramName]=bval;
			uiItems.append(bval);
		}else if(type=="complex"){
			QStringList names=nodeAtts.namedItem("name").nodeValue().split(";");
			QStringList max=nodeAtts.namedItem("max").nodeValue().split(";");
			QStringList min=nodeAtts.namedItem("min").nodeValue().split(";");
			QStringList val=value.split(";");
			kDebug() << "in complex"<<names.size() << " " << max.size() << " " << min.size() << " " << val.size()  ;
			if ( (names.size() == max.size() ) && 
			     (names.size()== min.size()) && 
			     (names.size()== val.size()) )
			{
				for (int i=0;i< names.size();i++){
					createSliderItem(names[i],val[i].toInt(),min[i].toInt(),max[i].toInt());
				};
			}
			ComplexParameter *pl=new ComplexParameter;
			connect (pl, SIGNAL ( parameterChanged()),this, SLOT( collectAllParameters ()) );
			pl->setupParam(d,0,100);
			vbox->addWidget(pl);
			valueItems[paramName+"complex"]=pl;
			items.append(pl);
		}else if (type=="color"){
			Ui::Colorval_UI *cval=new Ui::Colorval_UI;
			cval->setupUi(toFillin);
			bool ok;
			cval->kcolorbutton->setColor (value.toUInt(&ok,16));
			kDebug() << value.toUInt(&ok,16);
			
			connect (cval->kcolorbutton, SIGNAL(clicked()) , this, SLOT (collectAllParameters()));
			cval->label->setText(na.toElement().text() );
			valueItems[paramName]=cval;
			uiItems.append(cval);
		}else{
			delete toFillin;
			toFillin=NULL;
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
		QString setValue;
		if (type=="double" || type=="constant"){
			QSlider* slider=((Ui::Constval_UI*)valueItems[na.toElement().text()])->horizontalSlider;
			setValue=QString::number(slider->value());
		}else 
		if (type=="list"){
			KComboBox *box=((Ui::Listval_UI*)valueItems[na.toElement().text()])->list;
			setValue=box->currentText();
		}else 
		if (type=="bool"){
			QCheckBox *box=((Ui::Boolval_UI*)valueItems[na.toElement().text()])->checkBox;
			setValue=box->checkState() == Qt::Checked ? "1" :"0" ;
		}else
		if (type=="color"){
			KColorButton *color=((Ui::Colorval_UI*)valueItems[na.toElement().text()])->kcolorbutton;
			setValue.sprintf("0x%08x",color->color().rgba());
		}else
			if (type=="complex"){
				ComplexParameter *complex=((ComplexParameter*)valueItems[na.toElement().text()+"complex"]);
				namenode.item(i)=complex->getParamDesc();
			}
		if (!setValue.isEmpty()){
			pa.attributes().namedItem("value").setNodeValue(setValue);
		}
	}
	emit parameterChanged(params);
}

void EffectStackEdit::createSliderItem(const QString& name, int val ,int min, int max){
	QWidget* toFillin=new QWidget;
	Ui::Constval_UI *ctval=new Ui::Constval_UI;
	ctval->setupUi(toFillin);
	
	ctval->horizontalSlider->setMinimum(min);
	ctval->horizontalSlider->setMaximum(max);
	ctval->spinBox->setMinimum(min);
	ctval->spinBox->setMaximum(max);
	ctval->horizontalSlider->setPageStep((int) (max - min)/10);	
	ctval->horizontalSlider->setValue(val);
	ctval->label->setText(name);
	valueItems[name]=ctval;
	uiItems.append(ctval);
	connect (ctval->horizontalSlider, SIGNAL(valueChanged(int)) , this, SLOT (collectAllParameters()));
	items.append(toFillin);
	vbox->addWidget(toFillin);
}

void EffectStackEdit::slotSliderMoved(int){
	collectAllParameters();
}

void EffectStackEdit::clearAllItems(){
	foreach (QWidget* w,items){
		vbox->removeWidget(w);
		delete w;
	}
	foreach(void * p, uiItems){
		delete p;
	}
	uiItems.clear();
	items.clear();
	valueItems.clear();
}
