/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "listpropertiesview.h"
#include <QWidget>
#include <QLayout>


ListPropertiesView::ListPropertiesView(const QString& name, const QStringList& items, int initialIndex, const QString& comment, QWidget* parent) :
    QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.labelName->setText(name);
    m_ui.comboList->addItems(items);
    m_ui.comboList->setCurrentIndex(initialIndex);
    m_ui.labelComment->setText(comment);
    m_ui.widgetComment->setHidden(true);

    connect(m_ui.comboList, SIGNAL(currentIndexChanged(int)), this, SIGNAL(currentIndexChanged(int)));

//     if (!comment.isEmpty())

    parent->layout()->addWidget(this);
}

void ListPropertiesView::setCurrentIndex(int index)
{
    m_ui.comboList->blockSignals(true);
    m_ui.comboList->setCurrentIndex(index);
    m_ui.comboList->blockSignals(false);
}

#include "listpropertiesview.moc"
