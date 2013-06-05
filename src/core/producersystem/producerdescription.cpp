/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "producerdescription.h"
#include "producerrepository.h"
#include "effectsystem/abstractparameterdescription.h"
#include "effectsystem/abstracteffectlist.h"
#include "effectsystem/abstracteffectrepositoryitem.h"
#include "producer.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QDomElement>
#include <QLocale>
#include <KLocale>
#include <KDebug>


ProducerDescription::ProducerDescription(const QString &producerName, ProducerRepository *repository)
{
    m_valid = false;

    Mlt::Properties *metadata = repository->repository()->metadata(producer_type, producerName.toUtf8().constData());
    if (metadata && metadata->is_valid()) {
        if (metadata->get("title") && metadata->get("identifier")) {
            QLocale locale;
            locale.setNumberOptions(QLocale::OmitGroupSeparator);

            m_id = metadata->get("identifier");
            m_tag = m_id;
            m_nameOrig = metadata->get("title");
            m_name = i18n(m_nameOrig.toUtf8());
            m_descriptionOrig = metadata->get("description");
            m_description = i18n(m_descriptionOrig.toUtf8());
            m_authorsOrig = metadata->get("creator");
            m_authors = i18n(m_authorsOrig.toUtf8());
            m_version = locale.toDouble(QString(metadata->get("version")));

            Mlt::Properties tags(metadata->get_data("tags"));
            if (QString(tags.get(0)) == "Audio")
                m_type = AudioEffect;
            else
                m_type = VideoEffect;

            Mlt::Properties parameters(metadata->get_data("parameters"));
            int size = 0;
            for (int i = 0; parameters.is_valid() && i < parameters.count(); ++i) {
		size = 0;
                Mlt::Properties parameterProperties(parameters.get_data(i, size));
		// Convert mlt param type to kdenlive paramtype, in progress
		convertMltParameterType(parameterProperties);
                QString parameterType = parameterProperties.get("type");
		if (parameterType.isEmpty()) {
		    //kDebug()<<"// Parameter: "<<parameterProperties.get("identifier")<<" has NO TYPE";
		    continue;
		}
		if (parameterType == "integer" && parameterProperties.get_int("minimum") == 0 && parameterProperties.get_int("maximum") == 0) {
		    //kDebug()<<"// Parameter: "<<parameterProperties.get("identifier")<<" has NO RANGE, cannot use";
		    continue;
		}
                AbstractParameterDescription *parameter = repository->newParameterDescription(parameterType);
                if (parameter) {
                    parameter->init(parameterProperties, locale);
                    if (parameter->isValid()) {
                        append(parameter);
                    } else {
                        delete parameter;
                    }
                }
            }

            m_valid = true;
        }
    }
    if (metadata) {
        delete metadata;
    }
}

ProducerDescription::ProducerDescription(QDomElement description, double version, ProducerRepository *repository) :
    m_version(version)
{
    m_id = description.attribute("id");
    m_tag = description.attribute("tag");
    m_type = AbstractEffectRepositoryItem::getType(description.attribute("type"));
    m_unique = description.attribute("unique", "0").toInt();
    m_nameOrig = getTextFromElement(description.firstChildElement("name"));
    m_name = i18n(m_nameOrig.toUtf8());
    m_descriptionOrig = getTextFromElement(description.firstChildElement("description"));
    m_description = i18n(m_descriptionOrig.toUtf8());
    m_authorsOrig = getTextFromElement(description.firstChildElement("author"));
    m_authors = i18n(m_authorsOrig.toUtf8());

    QLocale locale;
    if (description.hasAttribute("LC_NUMERIC")) {
        locale = QLocale(description.attribute("LC_NUMERIC"));
    }
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    QDomNodeList parameters = description.elementsByTagName("parameter");
    for (int i = 0; i < parameters.count(); ++i) {
        QDomElement parameterElement = parameters.at(i).toElement();
        QString parameterType = parameterElement.attribute("type");
	//kDebug() << "+ + + "<<parameterType;
        AbstractParameterDescription *parameter = repository->newParameterDescription(parameterType);
        if (parameter) {
            parameter->init(parameterElement, locale);
            if (parameter->isValid()) {
                append(parameter);
            } else {
                delete parameter;
            }
        }
    }
    m_valid = true;
}


ProducerDescription::~ProducerDescription()
{
    qDeleteAll(begin(), end());
}

QList< AbstractParameterDescription* > ProducerDescription::parameters()
{
    return *this;
}

/*void ProducerDescription::setParameterValue(Mlt::Properties props)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    
    for (int i = 0; i < count(); ++i) {
	if (at(i)->name() == paramName) {
	    at(i)->init(props);
	}
    }
}*/

QString ProducerDescription::tag() const
{
    return m_tag;
}

QString ProducerDescription::displayName() const
{
    return m_name;
}

QString ProducerDescription::description() const
{
    return m_description;
}

QString ProducerDescription::authors() const
{
    return m_authors;
}

double ProducerDescription::version() const
{
    return m_version;
}

bool ProducerDescription::isUnique() const
{
    return m_unique;
}

bool ProducerDescription::isValid() const
{
    return m_valid;
}

QString ProducerDescription::getId() const
{
    return m_tag;
}

int ProducerDescription::paramCount() const
{
    return count();
}

QString ProducerDescription::getTextFromElement(QDomElement element)
{
    if (!element.isNull()) {
        return element.text().simplified().toUtf8().constData();
    }
    return QString();
}

void ProducerDescription::convertMltParameterType(Mlt::Properties &properties)
{
    QString type = properties.get("type");
    if (QString(properties.get("identifier")) == "argument") properties.set("identifier", "resource");
    QString widgetType = properties.get("widget");
    if (type == "string") {
	if (widgetType == "combo") {
	    properties.set("type", "list");
	    Mlt::Properties values(properties.get_data("values"));
	    QStringList stringValues;
	    for (int i = 0; i < values.count(); ++i) {
		stringValues << values.get(i);
	    }
	    properties.set("paramlist", stringValues.join(";").toUtf8().constData());
	    properties.set("paramlistdisplay", stringValues.join(",").toUtf8().constData());
	}
	else if (widgetType == "color") {
	    properties.set("type", "color");
	}
    }
}


