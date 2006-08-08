/***************************************************************************
                          createslideshowclip.cpp  -  description
                             -------------------
    begin                :  Jul 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "initeffects.h"
#include "krender.h"

initEffects::initEffects()
{
}

initEffects::~initEffects()
{
}

//static 
void initEffects::initializeEffects(EffectDescriptionList *effectList)
{
	// Build effects. We should find a more elegant way to do it, and ultimately 
    // retrieve it directly from mlt
    QXmlAttributes xmlAttr;
    EffectParamDescFactory effectDescParamFactory;

    // Grayscale
    EffectDesc *grey = new EffectDesc(i18n("Greyscale"), "greyscale");
    xmlAttr.append("type", QString::null, QString::null, "fixed");
    grey->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(grey);

    // Invert
    EffectDesc *invert = new EffectDesc(i18n("Invert"), "invert");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "fixed");
    invert->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(invert);

    // Sepia
    EffectDesc *sepia = new EffectDesc(i18n("Sepia"), "sepia");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "u");
    xmlAttr.append("description", QString::null, QString::null,
	"Chrominance U");
    xmlAttr.append("max", QString::null, QString::null, "255");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "75");
    sepia->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "v");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Chrominance V"));
    xmlAttr.append("max", QString::null, QString::null, "255");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "150");
    sepia->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(sepia);

    // Charcoal
    EffectDesc *charcoal = new EffectDesc(i18n("Charcoal"), "charcoal");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "x_scatter");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Horizontal scatter"));
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "2");
    charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "y_scatter");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Vertical scatter"));
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "2");
    charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "scale");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Scale"));
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "1");
    charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "mix");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Mix"));
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "0");
    charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "bool");
    xmlAttr.append("name", QString::null, QString::null, "invert");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Invert"));
    xmlAttr.append("default", QString::null, QString::null, "0");
    charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(charcoal);

    // Brightness
    EffectDesc *bright = new EffectDesc(i18n("Brightness"), "brightness");
    xmlAttr.clear();

    xmlAttr.append("type", QString::null, QString::null, "double");
    xmlAttr.append("name", QString::null, QString::null, "Intensity");
    xmlAttr.append("max", QString::null, QString::null, "3");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "1");
    bright->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(bright);

    // Audio volume
    EffectDesc *volume = new EffectDesc(i18n("Volume"), "volume");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "double");
    xmlAttr.append("name", QString::null, QString::null, "gain");
    xmlAttr.append("starttag", QString::null, QString::null, "gain");
    xmlAttr.append("max", QString::null, QString::null, "3");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "1");
    volume->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(volume);
    
    // Audio muting
    EffectDesc *mute = new EffectDesc(i18n("Mute"), "volume");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "gain");
    xmlAttr.append("max", QString::null, QString::null, "0");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "0");
    mute->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(mute);


    // Region obscure
    EffectDesc *obscure = new EffectDesc(i18n("Obscure"), "obscure");

    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "complex");
    xmlAttr.append("name", QString::null, QString::null,
	"X;Y;Width;Height;Averaging");
    xmlAttr.append("min", QString::null, QString::null, "0;0;0;0;3");
    xmlAttr.append("max", QString::null, QString::null,
	QString::number(KdenliveSettings::defaultwidth()) + ";" + QString::number(KdenliveSettings::defaultheight()) + ";1000;1000;100");
    xmlAttr.append("default", QString::null, QString::null,
	QString::number((int) KdenliveSettings::defaultwidth() / 2) + ";" + QString::number((int) KdenliveSettings::defaultheight() / 2) + ";100;100;20");
    obscure->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(obscure);

    // Mirror
    EffectDesc *mirror = new EffectDesc(i18n("Mirror"), "mirror");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "list");
    xmlAttr.append("paramlist", QString::null, QString::null, "horizontal,vertical,diagonal,xdiagonal,flip,flop");
    xmlAttr.append("default", QString::null, QString::null, "horizontal");
    xmlAttr.append("name", QString::null, QString::null,
	"mirror");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Mirroring direction"));
    mirror->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "bool");
    xmlAttr.append("name", QString::null, QString::null, "reverse");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Invert"));
    xmlAttr.append("default", QString::null, QString::null, "0");
    mirror->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    effectList->append(mirror);

    // Slowmotion
    EffectDesc *slowmo = new EffectDesc(i18n("Speed"), "slowmotion");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "_speed");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Start Speed"));
    xmlAttr.append("max", QString::null, QString::null, "300");
    xmlAttr.append("min", QString::null, QString::null, "1");
    xmlAttr.append("default", QString::null, QString::null, "100");
    slowmo->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "_speed_end");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("End Speed"));
    xmlAttr.append("max", QString::null, QString::null, "300");
    xmlAttr.append("min", QString::null, QString::null, "1");
    xmlAttr.append("default", QString::null, QString::null, "100");
    slowmo->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(slowmo);

    // Gamma
    EffectDesc *gamma = new EffectDesc(i18n("Gamma"), "gamma");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "gamma");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Gamma"));
    xmlAttr.append("max", QString::null, QString::null, "300");
    xmlAttr.append("min", QString::null, QString::null, "1");
    xmlAttr.append("default", QString::null, QString::null, "100");
    xmlAttr.append("factor", QString::null, QString::null, "100");
    gamma->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(gamma);


    // Pitch shifter
    EffectDesc *pitch = new EffectDesc(i18n("Pitch Shift"), "ladspa1433");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "pitch");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Shift"));
    xmlAttr.append("max", QString::null, QString::null, "400");
    xmlAttr.append("min", QString::null, QString::null, "25");
    xmlAttr.append("default", QString::null, QString::null, "100");
    xmlAttr.append("factor", QString::null, QString::null, "100");
    pitch->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    effectList->append(pitch);

    // Reverb
    EffectDesc *reverb = new EffectDesc(i18n("Reverb"), "ladspa1216");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "room");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Room size (m)"));
    xmlAttr.append("max", QString::null, QString::null, "300");
    xmlAttr.append("min", QString::null, QString::null, "1");
    xmlAttr.append("default", QString::null, QString::null, "75");
    reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "delay");
    xmlAttr.append("description", QString::null, QString::null,
	i18n("Delay (s)"));
    xmlAttr.append("max", QString::null, QString::null, "300");
    xmlAttr.append("min", QString::null, QString::null, "1");
    xmlAttr.append("default", QString::null, QString::null, "75");
    xmlAttr.append("factor", QString::null, QString::null, "10");
    reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));
    xmlAttr.clear();
    effectList->append(reverb);
}

//static 
char* initEffects::ladspaEffectString(int ladspaId, QStringList params)
{
    if (ladspaId == 1433 ) //Pitch
	return ladspaPitchEffectString(params);
    else if (ladspaId == 1216 ) //Reverb
	return ladspaReverbEffectString(params);
    else {
	kdDebug()<<"++++++++++  ASKING FOR UNKNOWN LADSPA EFFECT: "<<ladspaId<<endl;
	return("<jackrack></jackrack>");
    }
}

char* initEffects::ladspaPitchEffectString(QStringList params)
{
	return KRender::decodedString( QString("<?xml version=\"1.0\"?><!DOCTYPE jackrack SYSTEM \"http://purge.bash.sh/~rah/jack_rack_1.2.dtd\"><jackrack><channels>2</channels><samplerate>48000</samplerate><plugin><id>1433</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.0</value><value>1.0</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>4.000000</value><value>4.000000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[0]));
}

char* initEffects::ladspaReverbEffectString(QStringList params)
{
	return KRender::decodedString( QString("<?xml version=\"1.0\"?><!DOCTYPE jackrack SYSTEM \"http://purge.bash.sh/~rah/jack_rack_1.2.dtd\"><jackrack><channels>2</channels><samplerate>48000</samplerate>  <plugin><id>1216</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>0.500000</value><value>0.500000</value></controlrow><controlrow><lock>true</lock><value>0.750000</value><value>0.750000</value></controlrow><controlrow><lock>true</lock><value>-70.000000</value><value>-70.000000</value></controlrow><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>-17.500000</value><value>-17.500000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[0]).arg(params[1]).arg(params[1]));
}


