/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "colorpropertiesview.h"
#include "core/widgets/colorpickerwidget.h"
#include "core/effectsystem/abstractpropertiesviewcontainer.h"
#include <QLabel>
#include <QHBoxLayout>
#include <KColorButton>
#include <kdeversion.h>


ColorPropertiesView::ColorPropertiesView(const QString &name, const QColor &color, bool hasAlpha, QWidget* parent) :
        QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(name, this);

    QWidget *rightSide = new QWidget(this);
    QHBoxLayout *rightSideLayout = new QHBoxLayout(rightSide);
    rightSideLayout->setContentsMargins(0, 0, 0, 0);
    rightSideLayout->setSpacing(0);

    m_button = new KColorButton(color, rightSide);
#if KDE_IS_VERSION(4,5,0)
    m_button->setAlphaChannelEnabled(hasAlpha);
#endif
    ColorPickerWidget *picker = new ColorPickerWidget(rightSide);

    layout->addWidget(label);
    rightSideLayout->addWidget(m_button);
    rightSideLayout->addWidget(picker, 0, Qt::AlignRight);
    layout->addWidget(rightSide);

    connect(picker, SIGNAL(colorPicked(QColor)), this, SIGNAL(valueChanged(QColor)));
//     connect(picker, SIGNAL(displayMessage(const QString&, int)), this, SIGNAL(displayMessage(const QString&, int)));
    connect(m_button, SIGNAL(changed(const QColor&)), this, SIGNAL(valueChanged(const QColor&)));

    static_cast<AbstractViewContainer*>(parent)->addChild(this);
}
void ColorPropertiesView::setValue(const QColor &color)
{
    m_button->blockSignals(true);
    m_button->setColor(color);
    m_button->blockSignals(false);
}

#include "colorpropertiesview.moc"
