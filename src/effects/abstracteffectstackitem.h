/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTEFFECTSTACKITEM_H
#define ABSTRACTEFFECTSTACKITEM_H

#include <QObject>

class QWidget;


class AbstractEffectStackItem : public QObject
{
    Q_OBJECT

public:
    AbstractEffectStackItem(AbstractEffectStackItem* parent = 0);
    ~AbstractEffectStackItem();

    QWidget *getWidget();

protected:
    AbstractEffectStackItem *m_parent;
    QWidget *m_ui;

signals:
    void showComment();
    void enable(bool enable);
    void enabled(bool enabled);
    void focused(AbstractEffectStackItem *item);
};

#endif
