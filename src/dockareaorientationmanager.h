/*
Copyright (C) 2020  Julius Künzel <jk.kdedev@smartlab.uber.space>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef DOCKAREAORIENTATIONMANAGER_H
#define DOCKAREAORIENTATIONMANAGER_H

#include <QObject>
class QAction;

/**
 * @class DockAreaOrientationManager
 * @brief Handles functionality to set the orientation of the DockWidgetAreas
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
