/***************************************************************************
                          effect.cpp  -  description
                             -------------------
    begin                : Sun Feb 16 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <ctime>
#include <cstdlib>
#include <qdom.h>

#include <kio/netaccess.h>
#include <kdebug.h>

#include "effect.h"
#include "effectdesc.h"
#include "effectparameter.h"
#include "effectparamdescfactory.h"
#include "effectdoublekeyframe.h"
#include "effectcomplexkeyframe.h"
#include "effectkeyframe.h"
#include "effectparamdesc.h"
#include "kdenlivesettings.h"
#include "docclipref.h"
#include "initeffects.h"


Effect::Effect(const EffectDesc & desc, const QString & id, const QString & group):m_desc(desc),
m_id(id), m_group(group), m_enabled(true)
{
	if (m_desc.tag().startsWith("ladspa", false)) { 
		//ladspa filter, needs a unique temp file for xml input file
		m_paramFile = KdenliveSettings::currenttmpfolder() + QString::number(time(0)) + QString::number(rand()) + ".rack";
	}
	
}

Effect::~Effect()
{
	// remove temp file is ther was one
	if (!m_paramFile.isEmpty() && KIO::NetAccess::exists(m_paramFile, false, 0)) {
		KIO::NetAccess::del(m_paramFile, 0);
 	}
}

void Effect::setEnabled(bool isOn)
{
	m_enabled = isOn;
}

void Effect::setGroup(const QString & group)
{
	m_group = group;
}

bool Effect::isEnabled()
{
	return m_enabled;
}

void Effect::setTempFile(QString tmpFile)
{
	m_paramFile = tmpFile;
}

QDomDocument Effect::toXML()
{
    QDomDocument doc;

    QDomElement effect = doc.createElement("effect");

    //effect.setAttribute("name", name());
    effect.setAttribute("id", m_desc.stringId());
    effect.setAttribute("group", m_group);
    effect.setAttribute("enabled", isEnabled());
    if (!m_paramFile.isEmpty()) effect.setAttribute("tempfile", m_paramFile);

    for (uint i = 0; i < m_desc.numParameters(); i++) {
	EffectParamDesc *paramdesc = m_desc.parameter(i);
	QDomElement param = doc.createElement("param");
	param.setAttribute("name", paramdesc->name());
	param.setAttribute("type", paramdesc->type());
	param.setAttribute("value", paramdesc->value());
	param.setAttribute("default", paramdesc->defaultValue());
	param.setAttribute("max", paramdesc->max());
	param.setAttribute("min", paramdesc->min());
	if (paramdesc->type() == "double")
		for (uint j = 0; j < (uint)m_paramList.at(i)->numKeyFrames(); j++) {
		QDomElement keyframe = doc.createElement("keyframe");
		keyframe.setAttribute("time",
		    m_paramList.at(i)->keyframe(j)->time());
		keyframe.setAttribute("value",
		    m_paramList.at(i)->keyframe(j)->toDoubleKeyFrame()->
		    value());
		param.appendChild(keyframe);
	    }
	if (paramdesc->type() == "complex")
		for (uint j = 0; j < (uint)m_paramList.at(i)->numKeyFrames(); j++) {
		QDomElement keyframe = doc.createElement("keyframe");
		keyframe.setAttribute("time",
		    m_paramList.at(i)->keyframe(j)->time());
		keyframe.setAttribute("value",
		    m_paramList.at(i)->keyframe(j)->toComplexKeyFrame()->
		    toString());
		param.appendChild(keyframe);
	    }
	effect.appendChild(param);
    }
    doc.appendChild(effect);
/*  kdDebug()<<"---------- EFFECT -------------------"<<endl;
    kdDebug()<<doc.toString()<<endl;
    kdDebug()<<"---------- EFFECT -------------------"<<endl;*/
    return doc;
}

QDomDocument Effect::toFullXML(const QString &effectName)
{
    QDomDocument doc;

    QDomElement effect = doc.createElement("effect");
    //effect.setAttribute("name", name());
    effect.setAttribute("tag", m_desc.tag());

    QDomElement name = doc.createElement("name");
    QDomText nametxt = doc.createTextNode(effectName);
    name.appendChild(nametxt);
    effect.appendChild(name);

    QDomElement props = doc.createElement("properties");
    props.setAttribute("id", m_desc.stringId() + effectName);
    props.setAttribute("tag", m_desc.tag());
    props.setAttribute("type", "custom");
    effect.appendChild(props);

    for (uint i = 0; i < m_desc.numParameters(); i++) {
	EffectParamDesc *paramdesc = m_desc.parameter(i);
	QDomElement param = doc.createElement("parameter");
	QDomElement paramname = doc.createElement("name");
	QDomText paramnametxt = doc.createTextNode(paramdesc->description());
	paramname.appendChild(paramnametxt);
	param.appendChild(paramname);

	param.setAttribute("type", paramdesc->type());
	param.setAttribute("name", paramdesc->name());
	param.setAttribute("default", paramdesc->value());
	param.setAttribute("max", paramdesc->max());
	param.setAttribute("min", paramdesc->min());
	param.setAttribute("factor", paramdesc->factor());
	if (paramdesc->type() == "double") {
		param.setAttribute("startag", paramdesc->startTag());
		param.setAttribute("endtag", paramdesc->endTag());
		for (uint j = 0; j < (uint)m_paramList.at(i)->numKeyFrames(); j++) {
		QDomElement keyframe = doc.createElement("keyframe");
		keyframe.setAttribute("time",
		    m_paramList.at(i)->keyframe(j)->time());
		keyframe.setAttribute("value",
		    m_paramList.at(i)->keyframe(j)->toDoubleKeyFrame()->
		    value());
		param.appendChild(keyframe);
	    }
	}
	if (paramdesc->type() == "complex")
		for (uint j = 0; j < (uint)m_paramList.at(i)->numKeyFrames(); j++) {
		QDomElement keyframe = doc.createElement("keyframe");
		keyframe.setAttribute("time",
		    m_paramList.at(i)->keyframe(j)->time());
		keyframe.setAttribute("value",
		    m_paramList.at(i)->keyframe(j)->toComplexKeyFrame()->
		    toString());
		param.appendChild(keyframe);
	    }
	effect.appendChild(param);
    }
    doc.appendChild(effect);
/*  kdDebug()<<"---------- EFFECT -------------------"<<endl;
    kdDebug()<<doc.toString()<<endl;
    kdDebug()<<"---------- EFFECT -------------------"<<endl;*/
    return doc;
}

uint Effect::addInitialKeyFrames(int ix)
{
    QMap <double, QString> list;
    QMap <double, QString>::Iterator it;

    list = effectDescription().parameter(ix)->initialKeyFrames();
    if (list.isEmpty()) {
	addKeyFrame(ix, 0.0);
	addKeyFrame(ix, 1.0);
    }
    else for ( it = list.begin(); it != list.end(); ++it ) {
	if (!effectDescription().parameter(ix)->isComplex()) { m_paramList.at(ix)->addKeyFrame(effectDescription().parameter(ix)->
	createKeyFrame(it.key(), it.data().toDouble()));
	}
	else {
	    m_paramList.at(ix)->addKeyFrame(effectDescription().parameter(ix)->createKeyFrame(it.key(), QStringList::split(";", it.data())));
	}
    }
}

uint Effect::addKeyFrame(const uint ix, double time)
{
    return m_paramList.at(ix)->addKeyFrame(effectDescription().
	parameter(ix)->createKeyFrame(time));
}

uint Effect::addKeyFrame(const uint ix, double time, double value)
{
    if (!parameter(ix)) return 0;
    return m_paramList.at(ix)->addKeyFrame(effectDescription().parameter(ix)->
	createKeyFrame(time, value));
}

void Effect::addKeyFrame(const uint ix, double time, QStringList values)
{
    if (!parameter(ix)) return;
    m_paramList.at(ix)->addKeyFrame(effectDescription().parameter(ix)->
	createKeyFrame(time, values));
}

void Effect::addParameter(const QString & name)
{
    m_paramList.append(new EffectParameter(name));
}

EffectParameter *Effect::parameter(const uint ix)
{
    return m_paramList.at(ix);
}

Effect *Effect::clone()
{
    return Effect::createEffect(m_desc, toXML().documentElement());
}

// static
Effect *Effect::createEffect(const EffectDesc & desc,
    const QDomElement & effect)
{
    Effect *result = 0;

    if (effect.tagName() == "effect") {
	QString id = effect.attribute("id", QString::null);
	QString group = effect.attribute("group", QString::null);
	QString tmpFile = effect.attribute("tempfile", QString::null);
/*	if (type != desc.stringId()) {
	    kdError() <<
		"Effect::createEffect() failed integrity check - desc.name() == "
		<< desc.name() << ", type == " << type << endl;
	}*/
	result = new Effect(desc, id, group);
	result->setEnabled(effect.attribute("enabled", "1").toInt());
	if (!tmpFile.isEmpty() && QFile(tmpFile).exists()) result->setTempFile(tmpFile);

	QDomNode node = effect.firstChild();
	uint index = 0;
	EffectParamDescFactory m_effectDescParamFactory;

	while (!node.isNull()) {
	    QDomElement e = node.toElement();
	    if (!e.isNull()) {
		if (e.tagName() == "param") {
		    result->addParameter(e.attribute("name", ""));
		    QDomNode keyNode = e.firstChild();
		    QString paramType = e.attribute("type", "");
		    if (paramType == "constant" || paramType == "list" || paramType == "bool" || paramType == "position" || paramType == "color" || paramType == "geometry")
			result->effectDescription().parameter(index)->setValue(e.attribute("value", "0"));
		    while (!keyNode.isNull()) {
			QDomElement k = keyNode.toElement();
			if (k.tagName() == "keyframe") {
			    if (paramType == "double")
			    	result->addKeyFrame(index, k.attribute("time", "").toDouble(), k.attribute("value", "").toDouble());
			    else if (paramType == "complex")
			    	result->addKeyFrame(index, k.attribute("time", "").toDouble(), QStringList::split(";", k.attribute("value", "")));
			}
			keyNode = keyNode.nextSibling();
		    }
		    index++;
		}
	    }
	    node = node.nextSibling();
	}
    } else {
	kdError() <<
	    "Trying to create an effect from xml tag that is not <effect>"
	    << endl;
    }
    return result;
}


QMap <QString, QString> Effect::getParameters(DocClipRef *clip)
{
    QMap <QString, QString> params;
    uint parameterNum = 0;
    uint keyFrameNum = 0;
    QString effecttype = effectDescription().parameter(parameterNum)->type();

    if (effectDescription().tag() == "sox") {
	// THIS is a SOX FILTER, process 
        		QStringList soxparams;
			soxparams<<QString(effectDescription().stringId()).remove("sox_");
			while (parameter(parameterNum)) {
		    	if (effectDescription().parameter(parameterNum)->type() == "constant") {
				double effectParam;
			    	if (effectDescription().parameter(parameterNum)->factor() != 1.0)
                            		effectParam = effectDescription().parameter(parameterNum)->value().toDouble() / effectDescription().parameter(parameterNum)->factor();
			    	else effectParam = effectDescription().parameter(parameterNum)->value().toDouble();
				soxparams.append(QString::number(effectParam));
			    }
			    //params.append(effect->effectDescription().parameter(parameterNum)->value());
			    parameterNum++;
			}
			params["effect"] = soxparams.join(" ");
			kdDebug()<<" / / /SOX PARAMS: "<<params["effect"]<<endl;
    }
    else if (effectDescription().tag().startsWith("ladspa", false)) {
		// THIS is a LADSPA FILTER, process 
			QStringList ladspaparams;
			while (parameter(parameterNum)) {
			    if (effectDescription().parameter(parameterNum)->type() == "constant") {
				double effectParam;
			    	if (effectDescription().parameter(parameterNum)->factor() != 1.0)
                            		effectParam = effectDescription().parameter(parameterNum)->value().toDouble() / effectDescription().parameter(parameterNum)->factor();
			    	else effectParam = effectDescription().parameter(parameterNum)->value().toDouble();
				ladspaparams.append(QString::number(effectParam));
			    }
			    else ladspaparams.append(effectDescription().parameter(parameterNum)->value());
			    parameterNum++;
			}
			int ladspaid = effectDescription().tag().right(effectDescription().tag().length() - 6).toInt();

			QString effectFile = tempFileName();
			if (!effectFile.isEmpty()) {
			    initEffects::ladspaEffectFile( effectFile, ladspaid, ladspaparams );
			    params["src"] = effectFile;
			}
			/*if (effectDescription().isMono()) {
			    // Audio Mono clip. Duplicate audio channel
			    entry.appendChild(monoFilter);
			}*/

		// end of LADSPA FILTER

	    }

    else if (effecttype == "double") {
	// Effect has one parameter with keyframes
	QString startTag, endTag;
	keyFrameNum = parameter(parameterNum)->numKeyFrames();
	startTag = effectDescription().parameter(parameterNum)->startTag();
	endTag = effectDescription().parameter(parameterNum)->endTag();

	double factor = effectDescription().parameter(parameterNum)->factor();
	QString keyframePrefix = "#0:";

	if (keyFrameNum > 1) {
	    for (uint count = 0; count < keyFrameNum - 1; count++) {
		uint in =(uint) clip->cropStartTime().frames(KdenliveSettings::defaultfps());
		uint duration =(uint) clip->cropDuration().frames(KdenliveSettings::defaultfps());

		params[keyframePrefix + "in"] = QString::number(in + (parameter(parameterNum)->keyframe(count)->time()) *duration);

		params[keyframePrefix + startTag] = QString::number(parameter(parameterNum)->keyframe(count)->toDoubleKeyFrame()->value() / factor);

		params[keyframePrefix + "out"] = QString::number(in + (parameter(parameterNum)->keyframe(count + 1)->time()) * duration);

		params[keyframePrefix + endTag] = QString::number(parameter(parameterNum)->keyframe(count + 1)->toDoubleKeyFrame()->value() / factor);

		// Add the other constant parameters if any
		uint parameterNumBis = parameterNum + 1;
		while (parameter(parameterNumBis)) {
		    params[keyframePrefix + effectDescription().parameter(parameterNumBis)->name()] = QString::number(effectDescription().parameter(parameterNumBis)->value().toDouble() / effectDescription().parameter(parameterNumBis)->factor());
		    parameterNumBis++;
		}
		keyframePrefix = "#" + QString::number(count + 1) + ":";
	    }
	}
    }
    else if (effecttype == "complex") {
	// Effect has keyframes with several sub-parameters
	QString startTag, endTag;
	keyFrameNum = parameter(parameterNum)->numKeyFrames();
	startTag = effectDescription().parameter(parameterNum)->startTag();
	endTag = effectDescription().parameter(parameterNum)->endTag();
	QString keyframePrefix = "#0:";
	if (keyFrameNum > 1) {
	    for (uint count = 0; count < keyFrameNum - 1;count++) {
		uint in = (uint)(clip->cropStartTime().frames(KdenliveSettings::defaultfps()));
		uint duration = (uint)(clip->cropDuration().frames(KdenliveSettings::defaultfps()));

		params[keyframePrefix + "in"] = QString::number(in + (parameter(parameterNum)->keyframe(count)->time()) *duration);

		params[keyframePrefix + "out"] = QString::number(in + (parameter(parameterNum)->keyframe(count + 1)->time()) * duration);

		params[keyframePrefix + startTag] =  parameter(parameterNum)->keyframe(count)->toComplexKeyFrame()->processComplexKeyFrame();

		params[keyframePrefix + endTag] = parameter(parameterNum)->keyframe(count + 1)->toComplexKeyFrame()->processComplexKeyFrame();

		keyframePrefix = "#" + QString::number(count + 1) + ":";
	    }
	}
    }
    else if (effecttype == "constant" || effecttype == "list" || effecttype == "bool" || effecttype == "color" || effecttype == "geometry")
	while (parameter(parameterNum)) {
	    if (effectDescription().parameter(parameterNum)->factor() != 1.0)
                params[effectDescription().parameter(parameterNum)->name()] = QString::number(effectDescription().parameter(parameterNum)->value().toDouble() / effectDescription().parameter(parameterNum)->factor());
	    else params[effectDescription().parameter(parameterNum)->name()] = effectDescription().parameter(parameterNum)->value();
	    parameterNum++;
	}
    return params;
}

/*	// Effect has one parameter with keyframes
	QString startTag, endTag;
	keyFrameNum = parameter(parameterNum)->numKeyFrames();
	startTag = effectDescription().parameter(parameterNum)->startTag();
	endTag = effectDescription().parameter(parameterNum)->endTag();

	double factor = effectDescription().parameter(parameterNum)->factor();

	if (keyFrameNum > 1) {
		
			for (uint count = 0; count < keyFrameNum - 1;
			    count++) {
                                clipFilter =
				sceneList.createElement("filter");
			    clipFilter.setAttribute("mlt_service",
				effect->effectDescription().tag());
				 uint in =(uint)
				m_cropStart.frames(framesPerSecond());
				 uint duration =(uint)
				cropDuration().frames(framesPerSecond());
			    clipFilter.setAttribute("in",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count)->time()) *
				    duration));
			    clipFilter.setAttribute(startTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count)->toDoubleKeyFrame()->
				    value() / factor));
			    clipFilter.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));
			    clipFilter.setAttribute(endTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count +
					1)->toDoubleKeyFrame()->value() / factor));
			    entry.appendChild(clipFilter);
			}
			// Add the other constant parameters if any
			uint parameterNumBis = parameterNum + 1;
			while (effect->parameter(parameterNumBis)) {
			    clipFilter.setAttribute(effect->effectDescription().parameter(parameterNumBis)->name(), QString::number( effect->effectDescription().parameter(parameterNumBis)->value().toDouble() / effect->effectDescription().parameter(parameterNumBis)->factor()));
			    parameterNumBis++;
			}
			parameterNum = parameterNumBis;
		    }*/

