/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractpropertiesviewcontainer.h"
#include "ui_propertiesviewcontainer_ui.h"
#include "kdenlivesettings.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <KIcon>
#include <KLocale>


AbstractPropertiesViewContainer::AbstractPropertiesViewContainer(QWidget* parent) :
    AbstractViewContainer(parent),
    m_ui(new Ui::PropertiesViewContainer_UI)
{
    m_ui->setupUi(this);
    // Create layout that will be used by children
    new QVBoxLayout(m_ui->frameContainer);
}

AbstractPropertiesViewContainer::~AbstractPropertiesViewContainer()
{
    delete m_ui;
}

void AbstractPropertiesViewContainer::addChild(QWidget* child)
{
    m_ui->frameContainer->layout()->addWidget(child);
    m_ui->buttonCollapse->setEnabled(true);
}

void AbstractPropertiesViewContainer::setContainerDisabled(bool disable)
{
    //TODO always true. Correct ? if yes remove this "if"
    if (1) {
        m_ui->frameContainer->setDisabled(disable);
    }
    emit disabled(disable);
}

void AbstractPropertiesViewContainer::setCollapsed()
{
    const bool collapse = !m_ui->frameContainer->isHidden();
    m_ui->frameContainer->setHidden(collapse);

    if (collapse) {
        m_ui->buttonCollapse->setArrowType(Qt::RightArrow);
    } else {
        m_ui->buttonCollapse->setArrowType(Qt::DownArrow);
    }

    emit collapsed(collapse);
}


void AbstractPropertiesViewContainer::setUpHeader(const AbstractPropertiesViewContainer::HeaderButtons& buttons)
{
    if (buttons & CollapseButton) {
        if (m_ui->frameContainer->layout()->count() == 0) {
            m_ui->buttonCollapse->setEnabled(false);
        }
        connect(m_ui->buttonCollapse, SIGNAL(clicked()), this, SLOT(setCollapsed()));
    } else {
        m_ui->buttonCollapse->setHidden(true);
    }

    if (buttons & EnableButton) {
        m_ui->buttonEnable->setChecked(false);
        m_ui->buttonEnable->setIcon(KIcon("visible"));
//         buttonEnable->setToolTip(i18n());
        connect(m_ui->buttonEnable, SIGNAL(toggled(bool)), this, SLOT(setContainerDisabled(bool)));
    } else {
        m_ui->buttonEnable->setHidden(true);
    }

    if (buttons & ResetButton) {
        m_ui->buttonReset->setIcon(KIcon("view-refresh"));
        m_ui->buttonReset->setToolTip(i18n("Reset"));
        connect(m_ui->buttonReset, SIGNAL(clicked()), this, SIGNAL(reset()));
    } else {
        m_ui->buttonReset->setHidden(true);
    }

    if (buttons & MenuButton) {
        m_ui->buttonMenu->setIcon(KIcon("kdenlive-menu"));
    } else {
        m_ui->buttonMenu->setHidden(true);
    }
}

QFrame* AbstractPropertiesViewContainer::frameHeader()
{
    return m_ui->frameHeader;
}

#include "abstractpropertiesviewcontainer.moc"
