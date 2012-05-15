/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTPROPERTIESVIEWCONTAINER_H
#define ABSTRACTPROPERTIESVIEWCONTAINER_H

#include <QWidget>
#include <kdemacros.h>

class QToolButton;
class QFrame;
namespace Ui
{
    class PropertiesViewContainer_UI;
}


class KDE_EXPORT AbstractPropertiesViewContainer : public QWidget
{
    Q_OBJECT

public:
    explicit AbstractPropertiesViewContainer(QWidget* parent = 0);
    virtual ~AbstractPropertiesViewContainer();

    void addChild(QWidget *child);


    enum HeaderButton {
        NoButton = 0x0,
        CollapseButton = 0x1,
        EnableButton = 0x2,
        ResetButton = 0x4,
        MenuButton = 0x8,
        AllButtons = CollapseButton | EnableButton | ResetButton | MenuButton
    };
    Q_DECLARE_FLAGS(HeaderButtons, HeaderButton)

    void setUpHeader(const HeaderButtons &buttons = AllButtons);

protected slots:
    virtual void setContainerDisabled(bool disable);
    virtual void setCollapsed();

protected:
    QFrame *frameHeader();

    Ui::PropertiesViewContainer_UI *m_ui;

signals:
    void disabled(bool disabled);
    void collapsed(bool collapsed);
    void reset();
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractPropertiesViewContainer::HeaderButtons)

#endif
