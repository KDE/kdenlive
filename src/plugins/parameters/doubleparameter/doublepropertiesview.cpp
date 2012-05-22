/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "doublepropertiesview.h"
#include "core/widgets/dragvalue.h"
#include "core/effectsystem/abstractpropertiesviewcontainer.h"
#include <QWidget>
#include <QLayout>
#include <QGridLayout>


DoublePropertiesView::DoublePropertiesView(const QString& name, double value, double min, double max, const QString& comment, int id, const QString suffix, int decimals, QWidget* parent) :
    QWidget(parent)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    QGridLayout *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // WARNING: Using "value" as "default"
    m_dragValue = new DragValue(name, value, decimals, min, max, id, suffix, this);
    layout->addWidget(m_dragValue, 0, 1);

    m_dragValue->setValue(value);
    connect(m_dragValue, SIGNAL(valueChanged(double, bool)), this, SLOT(valueChanged(double,bool)));

    static_cast<AbstractPropertiesViewContainer*>(parent)->addChild(this);
}

void DoublePropertiesView::setValue(double value)
{
    m_dragValue->blockSignals(true);
    m_dragValue->setValue(value);
    m_dragValue->blockSignals(false);
}

void DoublePropertiesView::valueChanged(double value, bool final)
{
    if (final) {
        emit valueChanged(value);
    }
}

#include "doublepropertiesview.moc"
