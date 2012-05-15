/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "booleanpropertiesview.h"
#include "core/effectsystem/abstractpropertiesviewcontainer.h"
#include <QWidget>
#include <QLayout>


BooleanPropertiesView::BooleanPropertiesView(const QString &name, bool value, const QString &comment, QWidget *parent) :
    QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.checkSetting->setChecked(value);
    m_ui.labelName->setText(name);
    m_ui.labelComment->setText(comment);
    m_ui.widgetComment->setHidden(true);
    connect(m_ui.checkSetting, SIGNAL(toggled(bool)), this, SIGNAL(valueChanged(bool)));

//     if (!comment.isEmpty())

    static_cast<AbstractPropertiesViewContainer*>(parent)->addChild(this);
}

void BooleanPropertiesView::setValue(bool value)
{
    m_ui.checkSetting->blockSignals(true);
    m_ui.checkSetting->setChecked(value);
    m_ui.checkSetting->blockSignals(false);
}

#include "booleanpropertiesview.moc"
