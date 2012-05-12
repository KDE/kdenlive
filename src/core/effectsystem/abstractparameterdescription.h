/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTPARAMETERDESCRIPTION_H
#define ABSTRACTPARAMETERDESCRIPTION_H

#include <QObject>
#include <QString>
#include <kdemacros.h>

class AbstractParameterList;
class AbstractParameter;
class QDomElement;
class QLocale;
namespace Mlt
{
    class Properties;
}


/**
 * @class AbstractParameterDescription
 * @brief Abstract base class for parameter descriptions.
 * 
 * A parameter description is created from MLT metadata or a parameter element in a XML effect
 * description. It stores all necessary values and provides them to the actual parameters that are
 * also created through their description object.
 */

class KDE_EXPORT AbstractParameterDescription : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an invalid parameter description.
     */
    AbstractParameterDescription();
    virtual ~AbstractParameterDescription();

    /**
     * @brief Initializes the description based on a parameter element.
     * @param parameter parameter element as used in Kdenlive effect descriptions
     * @param locale locale used for the XML effect description file
     * 
     * In most cases you want call this version from your reimplementation since it sets name,
     * displayed name and comment. It also flags the description as valid.
     */
    virtual void init(QDomElement parameter, QLocale locale);
    /**
     * @brief Initializes the description based on MLT metadata.
     * @param properties the properties containing the metadata
     * @param locale the current locale in use
     * @see init(QDomElement parameter, QLocale locale)
     */
    virtual void init(Mlt::Properties &properties, QLocale locale);

    /**
     * @brief Should create and return a parameter object based on this description.
     * @param parent the parameter list that will contain the constructed parameter
     */
    virtual AbstractParameter *createParameter(AbstractParameterList *parent) const = 0;

    /**
     * @brief Returns the internal name/identifier of the parameter.
     */
    QString name() const;

    /**
     * @brief Returns a localized user visible name to be used in UI.
     */
    QString displayName() const;

    /**
     * @brief Returns a comment for the parameter.
     */
    QString comment() const;

    /**
     * @brief Returns the validness.
     * 
     * The description is valid once init was sucessfully called.
     */
    bool isValid() const;

protected:
    /** Determines the validness of the description. */
    bool m_valid;

private:
    QString m_name;
    QString m_displayName;
    QString m_displayNameOrig;
    QString m_comment;
    QString m_commentOrig;
};

#endif
