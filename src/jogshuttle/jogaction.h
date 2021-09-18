/*
    SPDX-FileCopyrightText: 2010 Pascal Fleury (fleury@users.sourceforge.net)

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

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
