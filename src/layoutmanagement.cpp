/*
Copyright (C) 2012  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "layoutmanagement.h"
#include "core.h"
#include "mainwindow.h"
#include <QMenu>
#include <QInputDialog>

#include <KConfigGroup>
#include <KXMLGUIFactory>
#include <KSharedConfig>
#include <klocalizedstring.h>

LayoutManagement::LayoutManagement(QObject *parent) :
    QObject(parent)
{
    // Prepare layout actions
    KActionCategory *layoutActions = new KActionCategory(i18n("Layouts"), pCore->window()->actionCollection());
    m_loadLayout = new KSelectAction(i18n("Load Layout"), pCore->window()->actionCollection());
    for (int i = 1; i < 5; ++i) {
        QAction *load = new QAction(QIcon(), i18n("Layout %1", i), this);
        load->setData('_' + QString::number(i));
        layoutActions->addAction("load_layout" + QString::number(i), load);
        m_loadLayout->addAction(load);
        QAction *save = new QAction(QIcon(), i18n("Save As Layout %1", i), this);
        save->setData('_' + QString::number(i));
        layoutActions->addAction("save_layout" + QString::number(i), save);
    }
    // Required to enable user to add the load layout action to toolbar
    layoutActions->addAction(QStringLiteral("load_layouts"), m_loadLayout);
    connect(m_loadLayout, SIGNAL(triggered(QAction *)), SLOT(slotLoadLayout(QAction *)));

    connect(pCore->window(), &MainWindow::GUISetupDone, this, &LayoutManagement::slotOnGUISetupDone);
}

void LayoutManagement::initializeLayouts()
{
    QMenu *saveLayout = static_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("layout_save_as"), pCore->window()));
    if (m_loadLayout == nullptr || saveLayout == nullptr) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup layoutGroup(config, "Layouts");
    QStringList entries = layoutGroup.keyList();
    QList<QAction *> loadActions = m_loadLayout->actions();
    QList<QAction *> saveActions = saveLayout->actions();
    for (int i = 1; i < 5; ++i) {
        // Rename the layouts actions
        foreach (const QString &key, entries) {
            if (key.endsWith(QStringLiteral("_%1").arg(i))) {
                // Found previously saved layout
                QString layoutName = key.section(QLatin1Char('_'), 0, -2);
                for (int j = 0; j < loadActions.count(); ++j) {
                    if (loadActions.at(j)->data().toString().endsWith('_' + QString::number(i))) {
                        loadActions[j]->setText(layoutName);
                        loadActions[j]->setData(key);
                        break;
                    }
                }
                for (int j = 0; j < saveActions.count(); ++j) {
                    if (saveActions.at(j)->data().toString().endsWith('_' + QString::number(i))) {
                        saveActions[j]->setText(i18n("Save as %1", layoutName));
                        saveActions[j]->setData(key);
                        break;
                    }
                }
            }
        }
    }
}

void LayoutManagement::slotLoadLayout(QAction *action)
{
    if (!action) {
        return;
    }

    QString layoutId = action->data().toString();
    if (layoutId.isEmpty()) {
        return;
    }

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup layouts(config, "Layouts");
    QByteArray state = QByteArray::fromBase64(layouts.readEntry(layoutId).toLatin1());
    pCore->window()->restoreState(state);
}

void LayoutManagement::slotSaveLayout(QAction *action)
{
    QString originallayoutName = action->data().toString();
    int layoutId = originallayoutName.section(QLatin1Char('_'), -1).toInt();

    QString layoutName = QInputDialog::getText(pCore->window(), i18n("Save Layout"), i18n("Layout name:"), QLineEdit::Normal,
                         originallayoutName.section(QLatin1Char('_'), 0, -2));
    if (layoutName.isEmpty()) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup layouts(config, "Layouts");
    layouts.deleteEntry(originallayoutName);

    QByteArray st = pCore->window()->saveState();
    layoutName.append('_' + QString::number(layoutId));
    layouts.writeEntry(layoutName, st.toBase64());
    initializeLayouts();
}

void LayoutManagement::slotOnGUISetupDone()
{
    QMenu *saveLayout = static_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("layout_save_as"), pCore->window()));
    if (saveLayout) {
        connect(saveLayout, &QMenu::triggered, this, &LayoutManagement::slotSaveLayout);
    }

    initializeLayouts();
}

