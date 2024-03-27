/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <jk.kdedev@smartlab.uber.space>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "layoutmanagement.h"
#include "core.h"
#include "mainwindow.h"
#include "utils/KMessageBox_KdenliveCompat.h"
#include <KMessageBox>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QVBoxLayout>

#include "kwidgetsaddons_version.h"
#include <KColorScheme>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>
#include <KXMLGUIFactory>

LayoutManagement::LayoutManagement(QObject *parent)
    : QObject(parent)
{
    m_translatedNames = {{QStringLiteral("kdenlive_logging"), i18n("Logging")},
                         {QStringLiteral("kdenlive_editing"), i18n("Editing")},
                         {QStringLiteral("kdenlive_audio"), i18n("Audio")},
                         {QStringLiteral("kdenlive_effects"), i18n("Effects")},
                         {QStringLiteral("kdenlive_color"), i18n("Color")}};

    // Prepare layout actions
    KActionCategory *layoutActions = new KActionCategory(i18n("Layouts"), pCore->window()->actionCollection());
    m_loadLayout = new KSelectAction(i18n("Load Layout"), pCore->window()->actionCollection());
    pCore->window()->actionCollection()->setShortcutsConfigurable(m_loadLayout, false);

    // Required to enable user to add the load layout action to toolbar
    layoutActions->addAction(QStringLiteral("load_layouts"), m_loadLayout);
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 240, 0)
    connect(m_loadLayout, &KSelectAction::actionTriggered, this, &LayoutManagement::slotLoadLayout);
#else
    connect(m_loadLayout, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &LayoutManagement::slotLoadLayout);
#endif

    QAction *saveLayout = new QAction(i18n("Save Layout…"), pCore->window()->actionCollection());
    layoutActions->addAction(QStringLiteral("save_layout"), saveLayout);
    connect(saveLayout, &QAction::triggered, this, &LayoutManagement::slotSaveLayout);

    QAction *manageLayout = new QAction(i18n("Manage Layouts…"), pCore->window()->actionCollection());
    layoutActions->addAction(QStringLiteral("manage_layout"), manageLayout);
    connect(manageLayout, &QAction::triggered, this, &LayoutManagement::slotManageLayouts);
    // Create 9 layout actions
    for (int i = 1; i < 10; i++) {
        QAction *load = new QAction(QIcon(), QString(), this);
        m_layoutActions << layoutActions->addAction("load_layout" + QString::number(i), load);
    }

    // Dock Area Orientation
    QAction *rowDockAreaAction = new QAction(QIcon::fromTheme(QStringLiteral("object-rows")), i18n("Arrange Dock Areas In Rows"), this);
    layoutActions->addAction(QStringLiteral("horizontal_dockareaorientation"), rowDockAreaAction);
    connect(rowDockAreaAction, &QAction::triggered, this, &LayoutManagement::slotDockAreaRows);

    QAction *colDockAreaAction = new QAction(QIcon::fromTheme(QStringLiteral("object-columns")), i18n("Arrange Dock Areas In Columns"), this);
    layoutActions->addAction(QStringLiteral("vertical_dockareaorientation"), colDockAreaAction);
    connect(colDockAreaAction, &QAction::triggered, this, &LayoutManagement::slotDockAreaColumns);

    // Create layout switcher for the menu bar
    MainWindow *main = pCore->window();
    m_container = new QWidget(main);
    m_containerGrp = new QButtonGroup(m_container);
    connect(m_containerGrp, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &LayoutManagement::activateLayout);
    auto *l1 = new QVBoxLayout;
    l1->addStretch();
    m_containerLayout = new QHBoxLayout;
    m_containerLayout->setSpacing(0);
    m_containerLayout->setContentsMargins(0, 0, 0, 0);
    l1->addLayout(m_containerLayout);
    m_container->setLayout(l1);
    KColorScheme scheme(main->palette().currentColorGroup(), KColorScheme::Button);
    QColor bg = scheme.background(KColorScheme::AlternateBackground).color();
    QString style = QString("padding-left: %4; padding-right: %4;background-color: rgb(%1,%2,%3);")
                        .arg(bg.red())
                        .arg(bg.green())
                        .arg(bg.blue())
                        .arg(main->fontInfo().pixelSize() / 2);
    m_container->setStyleSheet(style);
    main->menuBar()->setCornerWidget(m_container, Qt::TopRightCorner);
    initializeLayouts();
}

void LayoutManagement::initializeLayouts()
{
    if (m_loadLayout == nullptr) {
        return;
    }
    QString current;
    if (m_containerGrp->checkedButton()) {
        current = m_containerGrp->checkedButton()->text();
    }
    MainWindow *main = pCore->window();
    // Delete existing buttons
    while (auto item = m_containerLayout->takeAt(0)) {
        delete item->widget();
    }

    // Load default base layouts
    KConfig defaultConfig(QStringLiteral("kdenlivedefaultlayouts.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup defaultOrder(&defaultConfig, "Order");
    KConfigGroup defaultLayout(&defaultConfig, "Layouts");
    QStringList defaultLayouts;

    // Load User defined layouts
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"), KConfig::NoCascade);
    KConfigGroup layoutGroup(config, "Layouts");
    KConfigGroup layoutOrder(config, "Order");
    // If we don't have any layout saved, check in main config file
    if (!layoutGroup.exists()) {
        config = KSharedConfig::openConfig();
        KConfigGroup layoutGroup2(config, "Layouts");
        if (layoutGroup2.exists()) {
            // Migrate to new config file
            layoutGroup2.copyTo(&layoutGroup);
        }
    }
    m_loadLayout->removeAllActions();
    QStringList entries;
    bool addedDefault = false;
    if (!layoutOrder.exists()) {
        // This is an old or newly created config file, import defaults
        defaultLayouts = defaultOrder.entryMap().values();
        entries = layoutGroup.keyList();
        addedDefault = true;
    } else {
        // User sorted list
        entries = layoutOrder.entryMap().values();
    }

    // Add default layouts to user config in they don't exist
    for (const QString &lay : qAsConst(defaultLayouts)) {
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
        for (const QString &entry : qAsConst(entries)) {
            layoutOrder.writeEntry(QString::number(j), entry);
            j++;
        }
        config->reparseConfiguration();
    }
    for (int i = 1; i < 10; i++) {
        QString layoutName;
        if (i <= entries.count()) {
            layoutName = entries.at(i - 1);
        } else {
            break;
        }
        QAction *load = m_layoutActions.at(i - 1);
        if (layoutName.isEmpty()) {
            load->setText(QString());
            load->setIcon(QIcon());
        } else {
            load->setText(i18n("Layout %1: %2", i, translatedName(layoutName)));
            if (i < 6) {
                auto *lab = new QPushButton(translatedName(layoutName), m_container);
                lab->setProperty("layoutid", layoutName);
                lab->setFocusPolicy(Qt::NoFocus);
                lab->setCheckable(true);
                lab->setFlat(true);
                lab->setFont(main->menuBar()->font());
                m_containerGrp->addButton(lab);
                m_containerLayout->addWidget(lab);
                if (!current.isEmpty() && current == layoutName) {
                    lab->setChecked(true);
                }
            }
        }

        load->setData(layoutName);
        if (!layoutName.isEmpty()) {
            load->setEnabled(true);
            m_loadLayout->addAction(load);
        } else {
            load->setEnabled(false);
        }
    }
    // Required to trigger a refresh of the container buttons
    main->menuBar()->resize(main->menuBar()->sizeHint());
}

void LayoutManagement::activateLayout(QAbstractButton *button)
{
    if (!button) {
        return;
    }
    loadLayout(button->property("layoutid").toString(), false);
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
    loadLayout(layoutId, true);
}

bool LayoutManagement::loadLayout(const QString &layoutId, bool selectButton)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    KConfigGroup layouts(config, "Layouts");
    if (!layouts.hasKey(layoutId)) {
        // Error, layout not found
        return false;
    }
    Q_EMIT connectDocks(false);
    QByteArray state = QByteArray::fromBase64(layouts.readEntry(layoutId).toLatin1());
    bool timelineVisible = true;
    if (state.startsWith("NO-TL")) {
        timelineVisible = false;
        state.remove(0, 5);
    }
    pCore->window()->centralWidget()->setHidden(!timelineVisible);
    // restore state disables all toolbars, so remember state
    QList<KToolBar *> barsList = pCore->window()->toolBars();
    QMap<QString, bool> toolbarVisibility;
    for (auto &tb : barsList) {
        toolbarVisibility.insert(tb->objectName(), tb->isVisible());
    }
    pCore->window()->processRestoreState(state);
    // Restore toolbar status
    QMapIterator<QString, bool> i(toolbarVisibility);
    while (i.hasNext()) {
        i.next();
        KToolBar *tb = pCore->window()->toolBar(i.key());
        if (tb) {
            tb->setVisible(i.value());
        }
    }
    // pCore->window()->tabifyBins();
    Q_EMIT connectDocks(true);
    if (selectButton) {
        // Activate layout button
        QList<QAbstractButton *> buttons = m_containerGrp->buttons();
        bool buttonFound = false;
        for (auto *button : qAsConst(buttons)) {
            if (button->property("layoutid").toString() == layoutId) {
                QSignalBlocker bk(m_containerGrp);
                button->setChecked(true);
                buttonFound = true;
            }
        }
        if (!buttonFound && m_containerGrp->checkedButton()) {
            m_containerGrp->setExclusive(false);
            m_containerGrp->checkedButton()->setChecked(false);
            m_containerGrp->setExclusive(true);
        }
    }
    Q_EMIT updateTitleBars();
    return true;
}

std::pair<QString, QString> LayoutManagement::saveLayout(const QString &layout, const QString &suggestedName)
{

    QString visibleName = translatedName(suggestedName);

    QString layoutName = QInputDialog::getText(pCore->window(), i18nc("@title:window", "Save Layout"), i18n("Layout name:"), QLineEdit::Normal, visibleName);
    if (layoutName.isEmpty()) {
        return {nullptr, nullptr};
    }

    QString saveName;
    if (m_translatedNames.contains(layoutName)) {
        saveName = m_translatedNames.key(layoutName);
    } else {
        saveName = layoutName;
    }

    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    KConfigGroup layouts(config, "Layouts");
    KConfigGroup order(config, "Order");

    if (layouts.hasKey(saveName)) {
        // Layout already exists
        int res = KMessageBox::questionTwoActions(pCore->window(), i18n("The layout %1 already exists. Do you want to replace it?", layoutName), {},
                                                  KStandardGuiItem::overwrite(), KStandardGuiItem::cancel());
        if (res != KMessageBox::PrimaryAction) {
            return {nullptr, nullptr};
        }
    }

    layouts.writeEntry(saveName, layout);
    if (!order.entryMap().values().contains(saveName)) {
        int pos = order.keyList().constLast().toInt() + 1;
        order.writeEntry(QString::number(pos), saveName);
    }
    return {layoutName, saveName};
}

void LayoutManagement::slotSaveLayout()
{
    QAbstractButton *button = m_containerGrp->checkedButton();
    QString saveName;
    if (button) {
        saveName = button->text();
    }

    QByteArray st = pCore->window()->saveState();
    if (!pCore->window()->timelineVisible()) {
        st.prepend("NO-TL");
    }
    std::pair<QString, QString> names = saveLayout(st.toBase64(), saveName);

    // Activate layout button
    if (names.first != nullptr) {
        QList<QAbstractButton *> buttons = m_containerGrp->buttons();
        for (auto *button : qAsConst(buttons)) {
            if (button->text() == names.first) {
                QSignalBlocker bk(m_containerGrp);
                button->setChecked(true);
            }
        }
    }
}

void LayoutManagement::slotManageLayouts()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    KConfigGroup layouts(config, "Layouts");
    KConfigGroup order(config, "Order");
    QStringList names = order.entryMap().values();
    QString current;
    if (m_containerGrp->checkedButton()) {
        current = m_containerGrp->checkedButton()->text();
    }
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
    connect(&tb, &QToolButton::clicked, this, [&layouts, &list]() {
        if (list.currentItem()) {
            layouts.deleteEntry(list.currentItem()->data(Qt::UserRole).toString());
            delete list.currentItem();
        }
    });
    tb.setToolTip(i18n("Delete the layout."));
    auto *l2 = new QHBoxLayout;
    l->addLayout(l2);
    l2->addWidget(&tb);
    // Up button
    QToolButton tb2(&d);
    tb2.setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    tb2.setAutoRaise(true);
    connect(&tb2, &QToolButton::clicked, this, [&list]() {
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
    connect(&tb3, &QToolButton::clicked, this, [&list]() {
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
    connect(&tb4, &QToolButton::clicked, this, [this, &config, &list, &layouts, current]() {
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
        // Reset selected layout if it is a default one
        if (list.currentItem()) {
            QString selectedName = list.currentItem()->data(Qt::UserRole).toString();
            if (defaultLayoutNames.contains(selectedName)) {
                layouts.writeEntry(selectedName, defaultLayout.readEntry(selectedName));
                if (!current.isEmpty() && selectedName == current) {
                    config->reparseConfiguration();
                    loadLayout(current, false);
                }
            }
        }

        // Re-add missing default layouts
        for (const QString &name : qAsConst(defaultLayoutNames)) {
            if (!currentNames.contains(name) && m_translatedNames.contains(name)) {
                // Insert default layout
                QListWidgetItem *item = new QListWidgetItem(translatedName(name));
                item->setData(Qt::UserRole, name);
                item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                list.insertItem(pos, item);
                // Write layout data
                layouts.writeEntry(name, defaultLayout.readEntry(name));
            }
            pos++;
        }
    });
    l2->addWidget(&tb4);

    // Import button
    QToolButton tb5(&d);
    tb5.setIcon(QIcon::fromTheme(QStringLiteral("document-import")));
    tb5.setAutoRaise(true);
    tb5.setToolTip(i18n("Import"));
    connect(&tb5, &QToolButton::clicked, this, [this, &d, &list]() {
        QScopedPointer<QFileDialog> fd(new QFileDialog(&d, i18nc("@title:window", "Load Layout")));
        fd->setMimeTypeFilters(QStringList() << QStringLiteral("application/kdenlivelayout"));
        fd->setFileMode(QFileDialog::ExistingFile);
        if (fd->exec() != QDialog::Accepted) {
            return;
        }
        QStringList selection = fd->selectedFiles();
        QString url;
        if (!selection.isEmpty()) {
            url = selection.first();
        }
        if (url.isEmpty()) {
            return;
        }
        QFile file(url);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            KMessageBox::error(&d, i18n("Cannot open file %1", QUrl::fromLocalFile(url).fileName()));
            return;
        }
        QString state = QString::fromUtf8(file.readAll());
        file.close();

        QFileInfo fileInfo(url);
        QString suggestedName(fileInfo.baseName());

        std::pair<QString, QString> names = saveLayout(state, suggestedName);

        if (names.first != nullptr && names.second != nullptr && list.findItems(names.first, Qt::MatchFlag::MatchExactly).length() == 0) {
            auto *item = new QListWidgetItem(names.first, &list);
            item->setData(Qt::UserRole, names.second);
            item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
    });
    l2->addWidget(&tb5);

    // Export selected button
    QToolButton tb6(&d);
    tb6.setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    tb6.setAutoRaise(true);
    tb6.setToolTip(i18n("Export selected"));
    connect(&tb6, &QToolButton::clicked, this, [&d, &list]() {
        if (!list.currentItem()) {
            // Error, no layout selected
            KMessageBox::error(&d, i18n("No layout selected"));
            return;
        }

        QListWidgetItem *item = list.item(list.currentRow());
        QString layoutId = item->data(Qt::UserRole).toString();

        KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
        KConfigGroup layouts(config, "Layouts");
        if (!layouts.hasKey(layoutId)) {
            // Error, layout not found
            KMessageBox::error(&d, i18n("Cannot find layout %1", layoutId));
            return;
        }

        QScopedPointer<QFileDialog> fd(new QFileDialog(&d, i18nc("@title:window", "Export Layout")));
        fd->setMimeTypeFilters(QStringList() << QStringLiteral("application/kdenlivelayout"));
        fd->selectFile(layoutId + ".kdenlivelayout");
        fd->setDefaultSuffix(QStringLiteral("kdenlivelayout"));
        fd->setFileMode(QFileDialog::AnyFile);
        fd->setAcceptMode(QFileDialog::AcceptSave);
        if (fd->exec() != QDialog::Accepted) {
            return;
        }
        QStringList selection = fd->selectedFiles();
        QString url;
        if (!selection.isEmpty()) {
            url = selection.first();
        }
        if (url.isEmpty()) {
            return;
        }
        QFile file(url);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::error(&d, i18n("Cannot open file %1", QUrl::fromLocalFile(url).fileName()));
            return;
        }
        file.write(layouts.readEntry(layoutId).toUtf8());
        file.close();
    });
    l2->addWidget(&tb6);

    connect(&list, &QListWidget::currentRowChanged, this, [&list, &tb2, &tb3](int row) {
        tb2.setEnabled(row > 0);
        tb3.setEnabled(row < list.count() - 1);
    });

    l2->addStretch();

    // Add layouts to list
    for (const QString &name : qAsConst(names)) {
        auto *item = new QListWidgetItem(translatedName(name), &list);
        item->setData(Qt::UserRole, name);
        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    int ix = 0;
    if (!current.isEmpty()) {
        QList<QListWidgetItem *> res = list.findItems(current, Qt::MatchExactly);
        if (!res.isEmpty()) {
            ix = list.row(res.first());
        }
    }
    list.setCurrentRow(ix);
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
        QString layoutId = item->data(Qt::UserRole).toString();
        if (m_translatedNames.contains(layoutId)) {
            // This is a default layout, no rename
            if (item->text() != translatedName(layoutId)) {
                // A default layout was renamed
                order.writeEntry(QString::number(i + 1), item->text());
                layouts.writeEntry(item->text(), layouts.readEntry(layoutId));
                layouts.deleteEntry(layoutId);
            } else {
                order.writeEntry(QString::number(i + 1), layoutId);
            }
            continue;
        }
        order.writeEntry(QString::number(i + 1), layoutId);
        if (item->text() != layoutId && !item->text().isEmpty()) {
            layouts.writeEntry(item->text(), layouts.readEntry(layoutId));
            layouts.deleteEntry(layoutId);
        }
    }
    config->reparseConfiguration();
    initializeLayouts();
}

const QString LayoutManagement::translatedName(const QString &name)
{
    return m_translatedNames.contains(name) ? m_translatedNames.constFind(name).value() : name;
}

void LayoutManagement::slotDockAreaRows()
{
    // Use the corners for top and bottom DockWidgetArea
    pCore->window()->setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    pCore->window()->setCorner(Qt::TopLeftCorner, Qt::TopDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
}

void LayoutManagement::slotDockAreaColumns()
{
    // Use the corners for left and right DockWidgetArea
    pCore->window()->setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    pCore->window()->setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
}
