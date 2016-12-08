/*
Copyright (C) 2012  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef LAYOUTMANAGEMENT_H
#define LAYOUTMANAGEMENT_H

#include <QObject>

class KSelectAction;
class QAction;

class LayoutManagement : public QObject
{
    Q_OBJECT

public:
    explicit LayoutManagement(QObject *parent);

private slots:
    /** @brief Saves the widget layout. */
    void slotSaveLayout(QAction *action);
    /** @brief Loads a saved widget layout. */
    void slotLoadLayout(QAction *action);
    void slotOnGUISetupDone();

private:
    /** @brief Populates the "load layout" menu. */
    void initializeLayouts();

    KSelectAction *m_loadLayout;
};

#endif
