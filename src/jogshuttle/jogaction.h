/***************************************************************************
 *   Copyright (C) 2010 by Pascal Fleury (fleury@users.sourceforge.net)    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef JOGACTION_H
#define JOGACTION_H

#include "jogshuttle.h"
#include <QObject>
#include <QStringList>

class JogShuttleAction : public QObject
{
    Q_OBJECT public : explicit JogShuttleAction(const JogShuttle *jogShuttle, QStringList actionMap, QObject *parent = nullptr);
    ~JogShuttleAction() override;

private:
    const JogShuttle *m_jogShuttle;
    // this is indexed by button ID, having QString() for any non-used ones.
    QStringList m_actionMap;

public slots:
    void slotShuttlePos(int);
    void slotButton(int);

signals:
    void rewind(double);
    void forward(double);
    void action(const QString &);
};

#endif
