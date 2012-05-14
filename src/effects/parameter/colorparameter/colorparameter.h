/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef COLORPARAMETER_H
#define COLORPARAMETER_H

#include "core/effectsystem/abstractparameter.h"
#include "colorparameterdescription.h"



class ColorParameter : public AbstractParameter
{
    Q_OBJECT

public:
    ColorParameter(const ColorParameterDescription *parameterDescription, AbstractParameterList *parent);
    ~ColorParameter() {};

    void set(const char*data);
    QColor value() const;

    static QColor stringToColor(QString string);
    static QString colorToString(const QColor &color, bool hasAlpha = false);

public slots:
    void set(const QColor &value);

    void checkPropertiesViewState();

private:
    const ColorParameterDescription *m_description;

signals:
    void valueUpdated(const QColor &value);
};

#endif
