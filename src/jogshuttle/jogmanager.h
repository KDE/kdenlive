/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef JOGMANAGER_H
#define JOGMANAGER_H

#include <QObject>

class JogShuttle;
class JogShuttleAction;

/**
 * @class JogManager
 * @brief Turns JogShuttle support on/off according to KdenliveSettings and connects between JogShuttleAction and the actual actions.
 */

class JogManager : public QObject
{
    Q_OBJECT

public:
    explicit JogManager(QObject *parent = nullptr);

private slots:
    void slotDoAction(const QString &actionName);
    void slotConfigurationChanged();

private:
    JogShuttle *m_shuttle{nullptr};
    JogShuttleAction *m_shuttleAction{nullptr};
};

#endif
