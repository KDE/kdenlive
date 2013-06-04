/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef LISTPARAMETEREFFECTSTACKITEM_H
#define LISTPARAMETEREFFECTSTACKITEM_H

#include <QWidget>
#include "ui_listpropertiesview_ui.h"


class ListPropertiesView : public QWidget
{
    Q_OBJECT

public:
    ListPropertiesView(const QString &name, const QStringList &items, int initialIndex, const QString &comment, QWidget *parent);
    ~ListPropertiesView() {};

public slots:
    void setCurrentIndex(int index);
    
private slots:
    void showInfo();

private:
    Ui::ListPropertiesView_UI m_ui;

signals:
    void currentIndexChanged(int);
};

#endif
