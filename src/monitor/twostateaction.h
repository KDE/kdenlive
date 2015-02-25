/*
Copyright (C) 2014  Simon A. Eugster <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TWOSTATEACTION_H
#define TWOSTATEACTION_H

#include <QAction>

/**
 * @brief The TwostateAction class
 * Represents an action with two states, like the play button:
 * active means playing, so pause icon and tooltip are shown,
 * inactive shows the other icon/tooltip pair.
 */
class TwostateAction : public QAction
{
    Q_OBJECT
public:
    explicit TwostateAction(QIcon &inactiveIcon, QString inactiveText,
                            QIcon &activeIcon, QString activeText,
                            QObject *parent = 0);

    void setActive(bool active);

private:
    bool m_active;
    QIcon m_activeIcon;
    QIcon m_inactiveIcon;
    QString m_activeText;
    QString m_inactiveText;
};

#endif // TWOSTATEACTION_H
