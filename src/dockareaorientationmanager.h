/*
    SPDX-FileCopyrightText: 2020 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

    This file is part of Kdenlive. See www.kdenlive.org.
*/

#ifndef DOCKAREAORIENTATIONMANAGER_H
#define DOCKAREAORIENTATIONMANAGER_H

#include <QObject>
class QAction;

/** @class DockAreaOrientationManager
    @brief Handles functionality to set the orientation of the DockWidgetAreas
 */
class DockAreaOrientationManager : public QObject
{
    Q_OBJECT

public:
    explicit DockAreaOrientationManager(QObject *parent = nullptr);

private:
    QAction *m_verticalAction;
    QAction *m_horizontalAction;

private slots:
    void slotVerticalOrientation();
    void slotHorizontalOrientation();
};

#endif
