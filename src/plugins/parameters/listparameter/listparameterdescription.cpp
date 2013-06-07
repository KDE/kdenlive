/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "listparameterdescription.h"
#include "listparameter.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QDomElement>
#include <QLocale>
#include <KLocale>
#include <KPluginFactory>


K_PLUGIN_FACTORY( ListParameterFactory, registerPlugin<ListParameterDescription>(); )
K_EXPORT_PLUGIN( ListParameterFactory( "kdenlivelistparameter" ) )


ListParameterDescription::ListParameterDescription(QObject *, const QVariantList&)
{
}


ListParameterDescription::~ListParameterDescription()
{
}

void ListParameterDescription::init(const QDomElement &parameter, const QLocale &locale)
{
    AbstractParameterDescription::init(parameter, locale);

    const QString itemsString = parameter.attribute("paramlist");
    m_items = itemsString.split(QLatin1Char(';'));
    if (m_items.count() == 1) {
        // previously ',' was used as a separator. support old custom effects
        m_items = itemsString.split(QLatin1Char(','));
    }

    m_defaultIndex = qMax(0, m_items.indexOf(parameter.attribute("default")));

    QLocale systemLocale;
    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QString listItemType = parameter.attribute("listitemtype");
    if (listItemType == "double" && locale != systemLocale) {
        for (int i = 0; i < m_items.count(); ++i) {
            m_items[i] = systemLocale.toString(locale.toDouble(m_items.at(i)));
        }
    }

    QDomElement displayItemsElement = parameter.firstChildElement("paramlistdisplay");
    QString displayItemsString;
    if (!displayItemsElement.isNull()) {
        displayItemsString = displayItemsElement.text().simplified();
    } else {
        displayItemsString = parameter.attribute("paramlistdisplay");
    }
    m_displayItemsOrig = displayItemsString.split(QLatin1Char(','));

    if (m_items.count() != m_displayItemsOrig.count()) {
        m_displayItemsOrig = m_items;
        m_displayItems = i18n(itemsString.toUtf8().constData()).split(QLatin1Char(','));
    } else {
        m_displayItems = i18n(displayItemsString.toUtf8().constData()).split(QLatin1Char(','));
    }
}

void ListParameterDescription::init(Mlt::Properties& properties, const QLocale &locale)
{
    AbstractParameterDescription::init(properties, locale);
    QString itemsString = properties.get("paramlist");
    m_items = itemsString.split(';');
    if (m_items.count() == 1) {
        // previously ',' was used as a separator. support old custom effects
        m_items = itemsString.split(QLatin1Char(','));
    }

    m_defaultIndex = qMax(0, m_items.indexOf(properties.get("default")));

    QLocale systemLocale;
    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QString listItemType = properties.get("listitemtype");
    if (listItemType == "double" && locale != systemLocale) {
        for (int i = 0; i < m_items.count(); ++i) {
            m_items[i] = systemLocale.toString(locale.toDouble(m_items.at(i)));
        }
    }

    QString displayItemsString = properties.get("paramlistdisplay");
    m_displayItemsOrig = displayItemsString.split(QLatin1Char(','));

    if (m_items.count() != m_displayItemsOrig.count()) {
        m_displayItemsOrig = m_items;
        m_displayItems = i18n(itemsString.toUtf8().constData()).split(QLatin1Char(','));
    } else {
        m_displayItems = i18n(displayItemsString.toUtf8().constData()).split(QLatin1Char(','));
    }
    // construct for widget type dropdown?

    m_valid = true;
}

AbstractParameter *ListParameterDescription::createParameter(AbstractParameterList* parent, const QString &value) const
{
    ListParameter *parameter = new ListParameter(this, parent, value);
    return static_cast<AbstractParameter*>(parameter);
}

int ListParameterDescription::defaultIndex() const
{
    return m_defaultIndex;
}

QStringList ListParameterDescription::items() const
{
    return m_items;
}

QStringList ListParameterDescription::displayItems() const
{
    return m_displayItems;
}

#include "listparameterdescription.moc"
