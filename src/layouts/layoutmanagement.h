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
class QFrame;
class LayoutSwitcher;

class LayoutManagement : public QObject
{
    Q_OBJECT

public:
    explicit LayoutManagement(QObject *parent);
    /** @brief Load a layout by its name. */
    bool loadLayout(const QString &layoutId);

private Q_SLOTS:
    /** @brief Saves the widget layout. */
    void slotSaveLayout();
    /** @brief Loads a layout by ID (for LayoutSwitcher). */
    void slotLoadLayout(const QString &layoutId);
    /** @brief Loads a layout from a QAction containing its ID (for Toolbar buttons). */
    void slotLoadLayout(QAction *action);
    /** @brief Manage layout. */
    void slotManageLayouts();
    /** @brief Arrange the Qt::DockWidgetAreas in rows. */
    void slotDockAreaRows();
    /** @brief Arrange the Qt::DockWidgetAreas in columns. */
    void slotDockAreaColumns();
    /** @brief Hide the autosave indicator . */
    void hideAutoSave();
    /** @brief Show the autosave indicator for 2 seconds. */
    void startAutoSave();
    /** @brief Update Layout Switcher container palette on palette/theme change. */
    void slotUpdatePalette();

private:
    /** @brief Saves the given layout asking the user for a name.
     * @param layout
     * @param suggestedName name that is filled in to the save layout dialog
     * @return names of the saved layout. First is the visible name, second the internal name (they are different if the layout is a default one)
     */
    std::pair<QString, QString> saveLayout(const QString &layout, const QString &suggestedName);
    /** @brief Populates the "load layout" menu. */
    void initializeLayouts();
    QWidget *m_container;
    LayoutSwitcher *m_layoutSwitcher;
    QFrame *m_autosaveLabel{nullptr};
    QTimer m_autosaveDisplayTimer;
    KSelectAction *m_loadLayout;
    QList<QAction *> m_layoutActions;
    LayoutCollection m_layoutCollection;
    KActionCategory *m_layoutCategory;
    QString m_currentLayoutId;

Q_SIGNALS:
    /** @brief Layout changed, ensure title bars are correctly displayed. */
    void updateTitleBars();
    /** @brief Connect/disconnect stuff to update titlebars on dock location changed. */
    void connectDocks(bool doConnect);
};
