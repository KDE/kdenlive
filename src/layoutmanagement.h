/*
Copyright (C) 2012  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
Copyright (C) 2020  Julius Künzel <jk.kdedev@smartlab.uber.space>
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
    /** @brief Saves the given layout asking the user for a name.
     * @param layout
     * @param suggestedName name that is filled in to the save layout dialog
     * @return names of the saved layout. First is the visible name, second the internal name (they are different if the layout is a default one)
    */
    std::pair<QString, QString> saveLayout(QString layout, QString suggestedName);
    /** @brief Populates the "load layout" menu. */
    void initializeLayouts();
    QWidget *m_container;
    QButtonGroup *m_containerGrp;
    QHBoxLayout *m_containerLayout;
    KSelectAction *m_loadLayout;
    QList <QAction *> m_layoutActions;

signals:
    /** @brief Layout changed, ensure title bars are correctly displayed. */
    void updateTitleBars();
};

#endif
