/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef DOUBLEPROPERTIESVIEW_H
#define DOUBLEPROPERTIESVIEW_H

#include <QWidget>

class DragValue;


class DoublePropertiesView : public QWidget
{
    Q_OBJECT

public:
    DoublePropertiesView(const QString &name, double value, double min, double max, const QString &comment, int id, const QString suffix = QString(), int decimals = 0, QWidget* parent = 0);
    ~DoublePropertiesView() {};

public slots:
    /** @brief Sets the value to @param value. */
    void setValue(double value);

private slots:
    void valueChanged(double value, bool final);

private:
    DragValue *m_dragValue;

signals:
    void valueChanged(double);
};

#endif
