/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "listpropertiesview.h"
#include "core/effectsystem/abstractpropertiesviewcontainer.h"
#include <QWidget>
#include <QLayout>
#include <KDebug>


ListPropertiesView::ListPropertiesView(const QString& name, const QStringList& items, int initialIndex, const QString& comment, QWidget* parent) :
    QWidget(parent)
{
    m_ui.setupUi(this);
    if (comment.isEmpty())
        m_ui.labelName->setText(name);
    else
        m_ui.labelName->setText("<a href=\"#\">" + name + "</a>");
    m_ui.comboList->addItems(items);
    m_ui.comboList->setCurrentIndex(initialIndex);
    QString replaced = comment;
    m_ui.labelComment->setText(replaced.replace("\n", "<br/>"));
    connect(m_ui.labelName, SIGNAL(linkActivated(QString)), this, SLOT(showInfo()));
    m_ui.widgetComment->setHidden(true);

    connect(m_ui.comboList, SIGNAL(currentIndexChanged(int)), this, SIGNAL(currentIndexChanged(int)));

//     if (!comment.isEmpty())

    static_cast<AbstractViewContainer*>(parent)->addChild(this);
}

void ListPropertiesView::setCurrentIndex(int index)
{
    m_ui.comboList->blockSignals(true);
    m_ui.comboList->setCurrentIndex(index);
    m_ui.comboList->blockSignals(false);
}

void ListPropertiesView::showInfo()
{
    m_ui.widgetComment->setVisible(m_ui.widgetComment->isHidden());
}

#include "listpropertiesview.moc"
