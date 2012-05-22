/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "effectpropertiesview.h"
#include <QLabel>
#include <QHBoxLayout>


EffectPropertiesView::EffectPropertiesView(const QString& name, const QString& comment, QWidget* parent) :
    AbstractPropertiesViewContainer(parent)
{
    setUpHeader(AllButtons);

    QLabel *labelName = new QLabel(name, frameHeader());
    static_cast<QHBoxLayout *>(frameHeader()->layout())->insertWidget(2, labelName);

    QFont font;
    font.setBold(true);
    frameHeader()->setFont(font);

    parent->layout()->addWidget(this);
}

EffectPropertiesView::~EffectPropertiesView()
{
}



#include "effectpropertiesview.moc"
