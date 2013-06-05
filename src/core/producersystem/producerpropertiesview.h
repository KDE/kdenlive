/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PRODUCERPROPERTIESVIEW_H
#define PRODUCERPROPERTIESVIEW_H

#include <QWidget>
#include "effectsystem/abstractviewcontainer.h"

class ProducerDescription;
class KPushButton;

namespace Ui
{
    class ProducerViewContainer_UI;
}

class ProducerPropertiesView : public AbstractViewContainer
{
    Q_OBJECT

public:
    explicit ProducerPropertiesView(ProducerDescription *description, QWidget* parent = 0);
    ~ProducerPropertiesView();
    
    void addChild(QWidget *child);
    KPushButton *buttonDone();
    void finishLayout();

private:
    Ui::ProducerViewContainer_UI *m_ui;
    
};

#endif
