/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "effectdescription.h"
#include "abstractparameterdescription.h"
#include "doubleparameterdescription.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QDomElement>
#include <QLocale>
#include <KLocale>
#include <KDebug>


EffectDescription::EffectDescription(Mlt::Repository* repository, const QString filterName) :
    AbstractEffectRepositoryItem()
{
    m_valid = false;

    Mlt::Properties *metadata = repository->metadata(filter_type, filterName.toUtf8().constData());
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
            for (int i = 0; parameters.is_valid() && i < parameters.count(); i++) {
                Mlt::Properties parameterProperties(parameters.get_data(i, size));
                QString parameterType = parameterProperties.get("type");

                if (parameterType == "integer" || parameterType == "float") {
                    DoubleParameterDescription *parameter = new DoubleParameterDescription(parameterProperties, locale);
                    if (parameter->isValid()) {
                        append(static_cast<AbstractParameterDescription*>(parameter));
                    } else {
                        delete parameter;
                    }
                } else {
                }
            }

            m_valid = true;
        }
    }
    if (metadata) {
        delete metadata;
    }

//     kDebug() << "eff descr n" << m_id << m_tag << count();
}

EffectDescription::EffectDescription(QDomElement description, double version) :
    AbstractEffectRepositoryItem(),
    m_version(version)
{
    m_id = description.attribute("id");
    m_tag = description.attribute("tag");
    m_type = getType(description.attribute("type"));
    m_unique = static_cast<bool>(description.attribute("unique", "0").toInt());
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

        if (parameterType == "double" || parameterType == "constant") {
            DoubleParameterDescription *parameter = new DoubleParameterDescription(parameterElement, locale);
            if (parameter->isValid()) {
                append(static_cast<AbstractParameterDescription*>(parameter));
            } else {
                delete parameter;
            }
        } else {
        }
    }
    kDebug() << "eff descr y" << m_id << m_tag << count();
}


EffectDescription::~EffectDescription()
{
    qDeleteAll(begin(), end());
}

QString EffectDescription::getTag() const
{
    return m_tag;
}

QString EffectDescription::getName() const
{
    return m_name;
}

QString EffectDescription::getDescription() const
{
    return m_description;
}

QString EffectDescription::getAuthors() const
{
    return m_authors;
}

double EffectDescription::getVersion() const
{
    return m_version;
}

bool EffectDescription::getUniqueness() const
{
    return m_unique;
}

QString EffectDescription::getTextFromElement(QDomElement element)
{
    if (!element.isNull()) {
        return element.text().simplified().toUtf8().constData();
    }
    return QString();
}


