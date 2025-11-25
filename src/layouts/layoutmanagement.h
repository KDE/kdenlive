/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "layoutcollection.h"
#include <QMap>
#include <QObject>
#include <QTimer>

class KSelectAction;
class KActionCategory;
class QAction;
class QButtonGroup;
class QAbstractButton;
class QHBoxLayout;
class QLabel;
class LayoutSwitcher;

class LayoutManagement : public QObject
{
    Q_OBJECT

public:
    explicit LayoutManagement(QObject *parent);

public Q_SLOTS:
    /** @brief Loads a layout by LayoutInfo. */
    bool slotLoadLayout(LayoutInfo layout);
    /** @brief Load a layout by its name. */
    bool slotLoadLayoutById(const QString &layoutId);
    bool slotLoadLayoutFromData(const QString &layoutData);
    /** @brief Load a layout by its name. */
    void adjustLayoutToDar();

private Q_SLOTS:
    /** @brief Saves the widget layout. */
    void slotSaveLayout();
    /** @brief Loads a layout from a QAction containing its ID (for Toolbar buttons). */
    void slotLoadLayoutFromAction(QAction *action);
    /** @brief Manage layout. */
    void slotManageLayouts();
    /** @brief Hide the autosave indicator . */
    void hideAutoSave();
    /** @brief Show the autosave indicator for 2 seconds. */
    void startAutoSave();
    /** @brief Update Layout Switcher container palette on palette/theme change. */
    void slotUpdatePalette();

private:
    /** @brief Populates the "load layout" menu. */
    void initializeLayouts();
    /** @brief Updates the autosave icon with highlight color. */
    void updateAutosaveIcon();
    QWidget *m_container;
    QWidget *m_autosaveContainer;
    QWidget *m_switcherContainer;
    LayoutSwitcher *m_layoutSwitcher;
    QLabel *m_autosaveLabel;
    QTimer m_autosaveDisplayTimer;
    KSelectAction *m_loadLayout;
    QList<QAction *> m_layoutActions;
    LayoutCollection m_layoutCollection;
    KActionCategory *m_layoutCategory;
    QString m_currentLayoutId;
};
