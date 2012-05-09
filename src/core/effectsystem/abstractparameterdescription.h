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

class QDomElement;
class QLocale;

namespace Mlt
{
    class Properties;
}

//proper location?
enum ParameterType { DoubleParameterType};


// make template to be able to store default?
class KDE_EXPORT AbstractParameterDescription : public QObject
{
    Q_OBJECT

public:
    AbstractParameterDescription();
    virtual void init(QDomElement parameter, QLocale locale);
    virtual void init(Mlt::Properties &properties, QLocale locale);
    virtual ~AbstractParameterDescription();

    QString getName() const;
//     ParameterType getType() const;
    QString getDisplayName() const;
    QString getComment() const;
    bool isValid() const;

protected:
    bool m_valid;

private:
//     ParameterType m_type;
    QString m_name;
    QString m_displayName;
    QString m_displayNameOrig;
    QString m_comment;
    QString m_commentOrig;
};

#endif
