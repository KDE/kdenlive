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
#include "ui_booleanpropertiesview_ui.h"


class BooleanPropertiesView : public QWidget
{
    Q_OBJECT

public:
    BooleanPropertiesView(const QString &name, bool value, const QString &comment, QWidget *parent);
    ~BooleanPropertiesView() {}

public slots:
    /** @brief Sets the value to @param value. */
    void setValue(bool value);

private:
    Ui::BooleanParameter_UI m_ui;

signals:
    void valueChanged(bool);
};

#endif
