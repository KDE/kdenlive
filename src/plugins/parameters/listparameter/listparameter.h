/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef LISTPARAMETER_H
#define LISTPARAMETER_H

#include "core/effectsystem/abstractparameter.h"
#include "listparameterdescription.h"



class ListParameter : public AbstractParameter
{
    Q_OBJECT

public:
    ListParameter(const ListParameterDescription *parameterDescription, AbstractParameterList *parent, const QString &value = QString());
    ~ListParameter() {}

    void set(const char*data);
    int currentIndex() const;

public slots:
    void setCurrentIndex(int index);

    void checkPropertiesViewState();

protected:
    void resetValue();

private:
    const ListParameterDescription *m_description;

signals:
    void currentIndexUpdated(int index);
};

#endif
