/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef BOOLEANPARAMETEREFFECTSTACKITEM_H
#define BOOLEANPARAMETEREFFECTSTACKITEM_H

#include <QWidget>
#include "ui_booleanparameterview_ui.h"


class BooleanParameterEffectStackItem : public QWidget
{
    Q_OBJECT

public:
    BooleanParameterEffectStackItem(QString name, bool value, QString comment, QWidget *parent);
    ~BooleanParameterEffectStackItem() {};

public slots:
    /** @brief Sets the value to @param value. */
    void setValue(bool value);

private:
    Ui::BooleanParameter_UI m_ui;

signals:
    void valueChanged(bool);
};

#endif
