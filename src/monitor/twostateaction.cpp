/*
Copyright (C) 2014  Simon A. Eugster <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "twostateaction.h"

TwostateAction::TwostateAction(QIcon &inactiveIcon, QString inactiveText, QIcon &activeIcon, QString activeText, QObject *parent) :
    QAction(parent),
    m_active(false)
{
    m_activeIcon = activeIcon;
    m_activeText = activeText;
    m_inactiveIcon = inactiveIcon;
    m_inactiveText = inactiveText;
    setActive(m_active);
}

void TwostateAction::setActive(bool active)
{
    m_active = active;
    if (m_active) {
        this->setIcon(m_activeIcon);
        this->setToolTip(m_activeText);
    } else {
        this->setIcon(m_inactiveIcon);
        this->setToolTip(m_inactiveText);
    }
}
