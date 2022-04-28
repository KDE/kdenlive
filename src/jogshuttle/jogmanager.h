/*
    SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

    This file is part of Kdenlive. See www.kdenlive.org.
*/

#pragma once

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
