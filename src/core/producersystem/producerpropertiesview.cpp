/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "producerpropertiesview.h"
#include "producerdescription.h"
#include "ui_producerviewcontainer_ui.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QWheelEvent>
#include <KDebug>

ProducerPropertiesView::ProducerPropertiesView(ProducerDescription *description, QWidget* parent) :
    AbstractViewContainer(parent)
    , m_ui(new Ui::ProducerViewContainer_UI)
{
    m_ui->setupUi(this);
    m_ui->producerTitle->setText(i18n("Editing clip: %1",description->displayName()));
    QString producerInfo;
    producerInfo.append(description->description()+ "<br>");
    producerInfo.append(i18n("Author: ") +  description->authors() + "<br>");
    m_ui->producerTitle->setToolTip(producerInfo);
    // Create layout that will be used by children
    new QVBoxLayout(m_ui->frameContainer);
    parent->layout()->addWidget(this);
    setMouseTracking(true);
}

ProducerPropertiesView::~ProducerPropertiesView()
{
    delete m_ui;
}

void ProducerPropertiesView::addChild(QWidget* child)
{
    m_ui->frameContainer->layout()->addWidget(child);
}

void ProducerPropertiesView::finishLayout()
{
    QSpacerItem *spacer = new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    m_ui->frameContainer->layout()->addItem(spacer);
}

KPushButton *ProducerPropertiesView::buttonDone()
{
    return m_ui->doneButton;
}

#include "producerpropertiesview.moc"
