/***************************************************************************
                          initeffects.cpp  -  description
                             -------------------
    begin                :  Jul 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
    copyright            : (C) 2008 Marco Gittler
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


#include <qfile.h>
#include <qregexp.h>
#include <qdir.h>

#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>

#include "initeffects.h"
#include "effectparamdesc.h"
#include "krender.h"

struct mlt_repository_s
{
	struct mlt_properties_s parent; // a list of object files
	mlt_properties consumers; // lists of entry points
	mlt_properties filters;
	mlt_properties producers;
	mlt_properties transitions;
};

initEffects::initEffects()
{
}

initEffects::~initEffects()
{
}

//static
void initEffects::parseEffectFiles(EffectDescriptionList *effectList)
{
    QStringList::Iterator more;
    QStringList::Iterator it;
    QStringList fileList;
    QString itemName;


    // Build effects. Retrieve the list of MLT's available effects first.
	 mlt_repository repository = mlt_factory_init ( NULL ) ;//(mlt_repository) KRender::m_mlt_repository;
	 if (repository == NULL ){
		 kdDebug() << "Repository did not finish init " ;
		 return;
	 }
    Mlt::Properties filters ( repository->filters );
    QStringList filtersList;

    for (int i=0 ; i <filters.count() ; i++){
      filtersList << filters.get_name(i);
    }

    // Build effects. check producers first.

    Mlt::Properties producers ( repository->producers );
    QStringList producersList;
    for (int i=0 ; i <producers.count() ; i++){
      producersList << producers.get_name(i);
    }

    KGlobal::dirs()->addResourceType("ladspa_plugin", "lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/local/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/local/lib/ladspa");

    kdDebug()<<"//  INIT EFFECT SEARCH"<<endl;

    QStringList direc = KGlobal::dirs()->findDirs("data", "kdenlive/effects");

    QDir directory;
    for ( more = direc.begin() ; more != direc.end() ; ++more ) {
	directory = QDir(*more);
	fileList = directory.entryList( QDir::Files );
	for ( it = fileList.begin() ; it != fileList.end() ; ++it ){
	    itemName = KURL(*more + *it).path();
	    parseEffectFile(effectList, itemName, filtersList, producersList);
	    // kdDebug()<<"//  FOUND EFFECT FILE: "<<itemName<<endl;
	}
    }
}

// static
void initEffects::parseEffectFile(EffectDescriptionList *effectList, QString name, QStringList filtersList, QStringList producersList)
{
    QDomDocument doc;
    QFile file(name);
    doc.setContent(&file, false);
    QDomElement documentElement = doc.documentElement();
    QDomNodeList effects = doc.elementsByTagName("effect");

    if (effects.count() == 0) {
	kdDebug()<<"// EFFECT FILET: "<<name<<" IS BROKEN"<<endl;
	return;
    }
    QString groupName;
    if (doc.elementsByTagName("effectgroup").item(0).toElement().tagName() == "effectgroup") {
	groupName = documentElement.attribute("name", QString::null);
    }

    int i = 0;

    while (!effects.item(i).isNull()) {
    documentElement = effects.item(i).toElement();
    QString tag = documentElement.attribute("tag", QString::null);
    bool ladspaOk = true;
    if (tag == "ladspa") {
	QString library = documentElement.attribute("library", QString::null);
	if (locate("ladspa_plugin", library).isEmpty()) ladspaOk = false;
    }

    // Parse effect file
    if ((filtersList.findIndex(tag) != -1 || producersList.findIndex(tag) != -1) && ladspaOk) {
	// kdDebug()<<"++ ADDING EFFECT: "<<tag<<endl;
    	QDomNode n = documentElement.firstChild();
	QString id, effectName, effectTag, paramType;
	int paramCount = 0;
	EFFECTTYPE type;

        // Create Effect
        EffectParamDescFactory effectDescParamFactory;
        EffectDesc *effect = NULL;

	// parse effect file
	QDomNode namenode = documentElement.elementsByTagName("name").item(0);
	if (!namenode.isNull()) effectName = i18n(namenode.toElement().text());
	if (!groupName.isEmpty()) effectName.prepend("_" + groupName + "_");

	QDomNode propsnode = documentElement.elementsByTagName("properties").item(0);
	if (!propsnode.isNull()) {
	    QDomElement propselement = propsnode.toElement();
	    id = propselement.attribute("id", QString::null);
	    effectTag = propselement.attribute("tag", QString::null);
	    if (propselement.attribute("type", QString::null) == "audio") type = AUDIOEFFECT;
	    else if (propselement.attribute("type", QString::null) == "custom") type = CUSTOMEFFECT;
	    else type = VIDEOEFFECT;
	}

	QString effectDescription;
	QDomNode descnode = documentElement.elementsByTagName("description").item(0);
	if (!descnode.isNull()) effectDescription = descnode.toElement().text() + "<br />";

	QString effectAuthor;
	QDomNode authnode = documentElement.elementsByTagName("author").item(0);
	if (!authnode.isNull()) effectAuthor = authnode.toElement().text() + "<br />";

	if (effectName.isEmpty() || id.isEmpty() || effectTag.isEmpty()) return;

	effect = new EffectDesc(effectName, id, effectTag, effectDescription, effectAuthor, type);

	QDomNodeList paramList = documentElement.elementsByTagName("parameter");
	if (paramList.count() == 0) {
	    QDomElement fixed = doc.createElement("parameter");
	    fixed.setAttribute("type", "fixed");
	    effect->addParameter(effectDescParamFactory.createParameter(fixed));
	}
	else for (int i = 0; i < paramList.count(); i++) {
	    QDomElement e = paramList.item(i).toElement();
	    if (!e.isNull()) {
		paramCount++;
 		QDomNamedNodeMap attrs = e.attributes();
		int i = 0;
		QString value;
		while (!attrs.item(i).isNull()) {
		    QDomNode n = attrs.item(i);
		    value = n.nodeValue();
		    if (value.find("MAX_WIDTH") != -1)
			value.replace("MAX_WIDTH", QString::number(KdenliveSettings::defaultwidth()));
		    if (value.find("MID_WIDTH") != -1)
			value.replace("MID_WIDTH", QString::number(KdenliveSettings::defaultwidth() / 2));
		    if (value.find("MAX_HEIGHT") != -1)
			value.replace("MAX_HEIGHT", QString::number(KdenliveSettings::defaultheight()));
		    if (value.find("MID_HEIGHT") != -1)
			value.replace("MID_HEIGHT", QString::number(KdenliveSettings::defaultheight() / 2));
		    n.setNodeValue(value);
		    i++;
		}
		effect->addParameter(effectDescParamFactory.createParameter(e));
	    }
	}
        effectList->append(effect);
	}
	i++;
    }
}

//static 
char* initEffects::ladspaEffectString(int ladspaId, QStringList params)
{
    if (ladspaId == 1433 ) //Pitch
	return ladspaPitchEffectString(params);
    else if (ladspaId == 1216 ) //Room Reverb
	return ladspaRoomReverbEffectString(params);
    else if (ladspaId == 1423 ) //Reverb
	return ladspaReverbEffectString(params);
    else if (ladspaId == 1901 ) //Reverb
	return ladspaEqualizerEffectString(params);
    else {
	kdDebug()<<"++++++++++  ASKING FOR UNKNOWN LADSPA EFFECT: "<<ladspaId<<endl;
	return("<jackrack></jackrack>");
    }
}

//static 
void initEffects::ladspaEffectFile(const QString & fname, int ladspaId, QStringList params)
{
    char *filterString;
    switch (ladspaId) {
    case 1433: //Pitch
	filterString = ladspaPitchEffectString(params);
	break;
    case 1905: //Vinyl
	filterString = ladspaVinylEffectString(params);
	break;
    case 1216 : //Room Reverb
	filterString = ladspaRoomReverbEffectString(params);
	break;
    case 1423: //Reverb
	filterString = ladspaReverbEffectString(params);
	break;
    case 1195: //Declipper
	filterString = ladspaDeclipEffectString(params);
	break;
    case 1901:  //Reverb
	filterString = ladspaEqualizerEffectString(params);
	break;
    case 1913: // Limiter
	filterString = ladspaLimiterEffectString(params);
	break;
    case 1193: // Pitch Shifter
	filterString = ladspaPitchShifterEffectString(params);
	break;
    case 1417: // Rate Scaler
	filterString = ladspaRateScalerEffectString(params);
	break;
    case 1217: // Phaser
	filterString = ladspaPhaserEffectString(params);
	break;
    default: 
	kdDebug()<<"++++++++++  ASKING FOR UNKNOWN LADSPA EFFECT: "<<ladspaId<<endl;
	return;
	break;
    }

	QFile f( fname.ascii() );
    	if ( f.open( IO_WriteOnly ) ) 
	{
        	QTextStream stream( &f );
		stream << filterString;
		f.close();
    	}
    	else kdDebug()<<"++++++++++  ERROR CANNOT WRITE TO: "<<KdenliveSettings::currenttmpfolder() +  fname<<endl;
	delete filterString;
}

QString jackString = "<?xml version=\"1.0\"?><!DOCTYPE jackrack SYSTEM \"http://purge.bash.sh/~rah/jack_rack_1.2.dtd\"><jackrack><channels>2</channels><samplerate>48000</samplerate><plugin><id>";


char* initEffects::ladspaDeclipEffectString(QStringList)
{
	return KRender::decodedString( QString(jackString + "1195</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall></plugin></jackrack>"));
}

/*
char* initEffects::ladspaVocoderEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1441</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]));
}*/

char* initEffects::ladspaVinylEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1905</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow><controlrow><value>%4</value></controlrow><controlrow><value>%5</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]).arg(params[4]));
}

char* initEffects::ladspaPitchEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1433</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.0</value><value>1.0</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>4.000000</value><value>4.000000</value></controlrow></plugin></jackrack>").arg(params[0]));
}

char* initEffects::ladspaRoomReverbEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1216</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>0.750000</value><value>0.750000</value></controlrow><controlrow><lock>true</lock><value>-70.000000</value><value>-70.000000</value></controlrow><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>-17.500000</value><value>-17.500000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]));
}

char* initEffects::ladspaReverbEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1423</id><enabled>true</enabled>  <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values>    <lockall>true</lockall><controlrow><lock>true</lock><value>%1</value>      <value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>0.250000</value><value>0.250000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]));
}

char* initEffects::ladspaEqualizerEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1901</id><enabled>true</enabled>    <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow>    <controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]));
}

char* initEffects::ladspaLimiterEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1913</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]));
}

char* initEffects::ladspaPitchShifterEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1193</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]));
}

char* initEffects::ladspaRateScalerEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1417</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]));
}

char* initEffects::ladspaPhaserEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1217</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]));
}



