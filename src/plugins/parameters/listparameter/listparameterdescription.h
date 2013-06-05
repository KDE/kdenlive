/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef LISTPARAMETERDESCRIPTION_H
#define LISTPARAMETERDESCRIPTION_H

#include "core/effectsystem/abstractparameterdescription.h"
#include <QVariantList>
#include <QStringList>
#include <kdemacros.h>


class KDE_EXPORT ListParameterDescription : public AbstractParameterDescription
{
    Q_OBJECT

public:
    ListParameterDescription(QObject *, const QVariantList&);
    ~ListParameterDescription();

    void init(QDomElement parameter, QLocale locale);
    void init(Mlt::Properties &properties, QLocale locale);

    AbstractParameter *createParameter(AbstractParameterList *parent, const QString &value = QString()) const;

    int defaultIndex() const;
    QStringList items() const;
    QStringList displayItems() const;

private:
    int m_defaultIndex;
    QStringList m_items;
    QStringList m_displayItemsOrig;
    QStringList m_displayItems;
};

#endif
