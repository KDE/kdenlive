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
class QButtonGroup;
class QAbstractButton;
class QHBoxLayout;

class LayoutManagement : public QObject
{
    Q_OBJECT

public:
    explicit LayoutManagement(QObject *parent);
    /** @brief Load a layout by its name. */
    bool loadLayout(const QString &layoutId, bool selectButton);

private slots:
    /** @brief Saves the widget layout. */
    void slotSaveLayout();
    /** @brief Loads a layout from its button. */
    void activateLayout(QAbstractButton *button);
    /** @brief Loads a saved widget layout. */
    void slotLoadLayout(QAction *action);
    /** @brief Manage layout. */
    void slotManageLayouts();

private:
    /** @brief Populates the "load layout" menu. */
    void initializeLayouts();
    QWidget *m_container;
    QButtonGroup *m_containerGrp;
    QHBoxLayout *m_containerLayout;
    KSelectAction *m_loadLayout;
    QList <QAction *> m_layoutActions;
};

#endif
