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
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    KConfigGroup layouts(config, "Layouts");
    QStringList names = layouts.keyList();
    if (names.isEmpty()) {
        //initialize default Layouts
        names = QStringList({i18n("Logging"), i18n("Editing"), i18n("Audio"), i18n("Effects"), i18n("Color Correction")});
        QList <QByteArray> layoutData = {"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"};
        int i = 0;
        for (const QString &layoutName : names) {
            if (i > 1) {
                i = 0;
            }
            layouts.writeEntry(layoutName, layoutData.at(i));
            i++;
        }
    }
    int i = 1;
    for (const QString &layoutName : names) {
        QAction *load = new QAction(QIcon(), i18n("Layout %1: %2", i, layoutName), this);
        load->setData(layoutName);
        layoutActions->addAction("load_layout" + QString::number(i), load);
        m_loadLayout->addAction(load);
        i++;
    }
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
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
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
    if (!layoutGroup.exists()) {
        return;
    }
    QStringList entries = layoutGroup.keyList();
    m_loadLayout->clear();
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
    QStringList names = layouts.keyList();
    bool nameChanged = false;
    QDialog d(pCore->window());
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    l->addWidget(new QLabel(i18n("Current Layouts"), &d));
    QListWidget list(&d);
    l->addWidget(&list); 
    QToolButton tb(&d);
    tb.setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    tb.setAutoRaise(true);
    connect(&tb, &QToolButton::clicked, [&layouts, &list, &nameChanged]() {
        if (list.currentItem()) {
            layouts.deleteEntry(list.currentItem()->data(Qt::UserRole).toString());
            delete list.currentItem();
            nameChanged = true;
        }
    });
    tb.setToolTip(i18n("Delete Layout"));
    l->addWidget(&tb); 
    for (const QString &name : names) {
        QListWidgetItem *item = new QListWidgetItem(name, &list);
        item->setData(Qt::UserRole, name);
        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    l->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    for (int i = 0; i < list.count(); i++) {
        QListWidgetItem *item = list.item(i);
        if (item->text() != item->data(Qt::UserRole).toString() && !item->text().isEmpty()) {
            nameChanged = true;
            layouts.writeEntry(item->text(), layouts.readEntry(item->data(Qt::UserRole).toString()));
            layouts.deleteEntry(item->data(Qt::UserRole).toString());
        }
    }
    if (nameChanged) {
        initializeLayouts();
    }
}

void LayoutManagement::slotOnGUISetupDone()
{
    initializeLayouts();
}
