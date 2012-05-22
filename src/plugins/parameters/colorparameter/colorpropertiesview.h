/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/


#ifndef COLORPROPERTIESVIEW_H
#define COLORPROPERTIESVIEW_H

#include <QWidget>

class KColorButton;


class ColorPropertiesView : public QWidget
{
    Q_OBJECT

public:
    ColorPropertiesView(const QString &name, const QColor &color, bool hasAlpha, QWidget* parent = 0);

public slots:
    void setValue(const QColor &value);

signals:
    void valueChanged(const QColor &value);
    // TODO:
//     void displayMessage(const QString&, int);

private:
    KColorButton *m_button;
};

#endif
