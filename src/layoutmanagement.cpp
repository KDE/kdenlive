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
#include <QInputDialog>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KXMLGUIFactory>
#include <klocalizedstring.h>

LayoutManagement::LayoutManagement(QObject *parent)
    : QObject(parent)
{
    // Prepare layout actions
    KActionCategory *layoutActions = new KActionCategory(i18n("Layouts"), pCore->window()->actionCollection());
    m_loadLayout = new KSelectAction(i18n("Load Layout"), pCore->window()->actionCollection());
    // Required to enable user to add the load layout action to toolbar
    layoutActions->addAction(QStringLiteral("load_layouts"), m_loadLayout);
    connect(m_loadLayout, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &LayoutManagement::slotLoadLayout);

    QAction *saveLayout = new QAction(i18n("Save Layout"), pCore->window()->actionCollection());
    layoutActions->addAction(QStringLiteral("save_layout"), saveLayout);
    connect(saveLayout, &QAction::triggered, this, &LayoutManagement::slotSaveLayout);
    
    QAction *manageLayout = new QAction(i18n("Manage Layouts"), pCore->window()->actionCollection());
    layoutActions->addAction(QStringLiteral("manage_layout"), manageLayout);
    connect(manageLayout, &QAction::triggered, this, &LayoutManagement::slotManageLayouts);
    connect(pCore->window(), &MainWindow::GUISetupDone, this, &LayoutManagement::slotOnGUISetupDone);
}

void LayoutManagement::initializeLayouts()
{
    if (m_loadLayout == nullptr) {
        return;
    }
    
    // Load default base layouts
    KConfig defaultConfig(QStringLiteral("kdenlivedefaultlayouts.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup defaultOrder(&defaultConfig, "Order");
    KConfigGroup defaultLayout(&defaultConfig, "Layouts");
    QStringList defaultLayouts;
    
    // Load User defined layouts
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"), KConfig::NoCascade);
    KConfigGroup layoutGroup(config, "Layouts");
    // If we don't have any layout saved, check in main config file
    if (!layoutGroup.exists()) {
        config = KSharedConfig::openConfig();
        KConfigGroup layoutGroup2(config, "Layouts");
        if (layoutGroup2.exists()) {
            // Migrate to new config file
            layoutGroup2.copyTo(&layoutGroup);
        }
    }
    m_loadLayout->clear();
    KConfigGroup layoutOrder(config, "Order");
    QStringList entries;
    if (!layoutOrder.exists()) {
        // This is an old or newly created config file, import defaults
        defaultLayouts = defaultOrder.entryMap().values();
        entries = layoutGroup.keyList();
    } else {
        // User sorted list
        entries = layoutOrder.entryMap().values();
    }
    
    // Add default layouts to user config in they don't exist
    bool addedDefault = false;
    for (const QString &lay : defaultLayouts)
    {
        if (!entries.contains(lay)) {
            entries.insert(defaultLayouts.indexOf(lay), lay);
            layoutGroup.writeEntry(lay, defaultLayout.readEntry(lay));
            addedDefault = true;
        }
    }
    
    if (addedDefault) {
        // Write updated order
        layoutOrder.deleteGroup();
        int j = 1;
        for (const QString &entry : entries) {
            layoutOrder.writeEntry(QString::number(j), entry);
            j++;
        }
    }
    KActionCategory *layoutActions = new KActionCategory(i18n("Layouts"), pCore->window()->actionCollection());
    int i = 1;
    for (const QString &layoutName : entries) {
        QAction *load = new QAction(QIcon(), i18n("Layout %1: %2", i, layoutName), this);
        load->setData(layoutName);
        layoutActions->addAction("load_layout" + QString::number(i), load);
        m_loadLayout->addAction(load);
        i++;
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
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    KConfigGroup layouts(config, "Layouts");
    if (!layouts.hasKey(layoutId)) {
        // Error, layout not found
        return;
    }
    QByteArray state = QByteArray::fromBase64(layouts.readEntry(layoutId).toLatin1());
    bool timelineVisible = true;
    if (state.startsWith("NO-TL")) {
        timelineVisible = false;
        state.remove(0, 5);
    }
    pCore->window()->centralWidget()->setHidden(!timelineVisible);
    pCore->window()->restoreState(state);
}

void LayoutManagement::slotSaveLayout()
{
    QString layoutName = QInputDialog::getText(pCore->window(), i18n("Save Layout"), i18n("Layout name:"), QLineEdit::Normal);
    if (layoutName.isEmpty()) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    KConfigGroup layouts(config, "Layouts");

    QByteArray st = pCore->window()->saveState();
    if (!pCore->window()->timelineVisible()) {
        st.prepend("NO-TL");
    }
    layouts.writeEntry(layoutName, st.toBase64());
    initializeLayouts();
}

void LayoutManagement::slotManageLayouts()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    KConfigGroup layouts(config, "Layouts");
    KConfigGroup order(config, "Order");
    QStringList names = order.entryMap().values();
    QDialog d(pCore->window());
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    l->addWidget(new QLabel(i18n("Current Layouts"), &d));
    QListWidget list(&d);
    list.setAlternatingRowColors(true);
    l->addWidget(&list); 
    // Delete button
    QToolButton tb(&d);
    tb.setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    tb.setAutoRaise(true);
    connect(&tb, &QToolButton::clicked, [this, &layouts, &list]() {
        if (list.currentItem()) {
            layouts.deleteEntry(list.currentItem()->data(Qt::UserRole).toString());
            delete list.currentItem();
        }
    });
    tb.setToolTip(i18n("Delete Layout"));
    auto *l2 = new QHBoxLayout;
    l->addLayout(l2);
    l2->addWidget(&tb);
    // Up button
    QToolButton tb2(&d);
    tb2.setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    tb2.setAutoRaise(true);
    connect(&tb2, &QToolButton::clicked, [&list]() {
        if (list.currentItem() && list.currentRow() > 0) {
            int updatedRow = list.currentRow() - 1;
            QListWidgetItem *item = list.takeItem(list.currentRow());
            list.insertItem(updatedRow, item);
            list.setCurrentRow(updatedRow);
        }
    });
    l2->addWidget(&tb2);

     // Down button
    QToolButton tb3(&d);
    tb3.setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    tb3.setAutoRaise(true);
    connect(&tb3, &QToolButton::clicked, [&list]() {
        if (list.currentItem() && list.currentRow() < list.count() - 1) {
            int updatedRow = list.currentRow() + 1;
            QListWidgetItem *item = list.takeItem(list.currentRow());
            list.insertItem(updatedRow, item);
            list.setCurrentRow(updatedRow);
        }
    });
    l2->addWidget(&tb3);
    
    // Reset button
    QToolButton tb4(&d);
    tb4.setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    tb4.setAutoRaise(true);
    tb4.setToolTip(i18n("Reset"));
    connect(&tb4, &QToolButton::clicked, [&list, &layouts]() {
        // Load default base layouts
        KConfig defaultConfig(QStringLiteral("kdenlivedefaultlayouts.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
        KConfigGroup defaultOrder(&defaultConfig, "Order");
        KConfigGroup defaultLayout(&defaultConfig, "Layouts");
        QStringList defaultLayoutNames = defaultOrder.entryMap().values();
        // Get list of current layouts
        QStringList currentNames;
        for (int i = 0; i < list.count(); i++) {
            currentNames << list.item(i)->data(Qt::UserRole).toString();
        }
        int pos = 0;
        for (const QString &name : defaultLayoutNames) {
            if (!currentNames.contains(name)) {
                // Insert default layout
                QListWidgetItem *item = new QListWidgetItem(name);
                item->setData(Qt::UserRole, name);
                item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                list.insertItem(pos, item);
                // Write layout data
                layouts.writeEntry(name, defaultLayout.readEntry(name));
                pos++;
            }
        }
    });
    l2->addWidget(&tb4);
    
    connect(&list, &QListWidget::currentRowChanged, [&list, &tb2, &tb3] (int row) {
        tb2.setEnabled(row > 0);
        tb3.setEnabled(row < list.count() - 1);
    });

    l2->addStretch();
    
    // Add layouts to list
    for (const QString &name : names) {
        QListWidgetItem *item = new QListWidgetItem(name, &list);
        item->setData(Qt::UserRole, name);
        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    list.setCurrentRow(0);
    l->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    // Reset order
    order.deleteGroup();
    // Update order and new names
    for (int i = 0; i < list.count(); i++) {
        QListWidgetItem *item = list.item(i);
        order.writeEntry(QString::number(i + 1), item->data(Qt::UserRole).toString());
        if (item->text() != item->data(Qt::UserRole).toString() && !item->text().isEmpty()) {
            layouts.writeEntry(item->text(), layouts.readEntry(item->data(Qt::UserRole).toString()));
            layouts.deleteEntry(item->data(Qt::UserRole).toString());
        }
    }
    config->reparseConfiguration();
    initializeLayouts();
}

void LayoutManagement::slotOnGUISetupDone()
{
    initializeLayouts();
}
