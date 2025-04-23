/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "layouts/layoutmanagement.h"
#include "core.h"
#include "mainwindow.h"
#include <KMessageBox>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QInputDialog>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QVBoxLayout>

#include "layouts/layoutmanagerdialog.h"
#include "layouts/layoutswitcher.h"
#include <KColorScheme>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>
#include <KXMLGUIFactory>

LayoutManagement::LayoutManagement(QObject *parent)
    : QObject(parent)
{
    // Initialize layout categories in KDE action framework
    m_layoutCategory = new KActionCategory(i18n("Layouts"), pCore->window()->actionCollection());
    m_loadLayout = new KSelectAction(i18n("Load Layout"), pCore->window()->actionCollection());
    pCore->window()->actionCollection()->setShortcutsConfigurable(m_loadLayout, false);

    // Required to enable user to add the load layout action to toolbar
    m_layoutCategory->addAction(QStringLiteral("load_layouts"), m_loadLayout);
    connect(m_loadLayout, &KSelectAction::actionTriggered, this, static_cast<void (LayoutManagement::*)(QAction *)>(&LayoutManagement::slotLoadLayout));

    QAction *saveLayout = new QAction(i18n("Save Layout…"), pCore->window()->actionCollection());
    m_layoutCategory->addAction(QStringLiteral("save_layout"), saveLayout);
    connect(saveLayout, &QAction::triggered, this, &LayoutManagement::slotSaveLayout);

    QAction *manageLayout = new QAction(i18n("Manage Layouts…"), pCore->window()->actionCollection());
    m_layoutCategory->addAction(QStringLiteral("manage_layout"), manageLayout);
    connect(manageLayout, &QAction::triggered, this, &LayoutManagement::slotManageLayouts);

    // Create 9 layout actions
    for (int i = 1; i < 10; i++) {
        QAction *load = new QAction(QIcon(), QString(), this);
        m_layoutActions << m_layoutCategory->addAction("load_layout" + QString::number(i), load);
    }

    // Dock Area Orientation
    QAction *rowDockAreaAction = new QAction(QIcon::fromTheme(QStringLiteral("object-rows")), i18n("Arrange Dock Areas In Rows"), this);
    m_layoutCategory->addAction(QStringLiteral("horizontal_dockareaorientation"), rowDockAreaAction);
    connect(rowDockAreaAction, &QAction::triggered, this, &LayoutManagement::slotDockAreaRows);

    QAction *colDockAreaAction = new QAction(QIcon::fromTheme(QStringLiteral("object-columns")), i18n("Arrange Dock Areas In Columns"), this);
    m_layoutCategory->addAction(QStringLiteral("vertical_dockareaorientation"), colDockAreaAction);
    connect(colDockAreaAction, &QAction::triggered, this, &LayoutManagement::slotDockAreaColumns);

    // Create layout switcher for the menu bar
    MainWindow *main = pCore->window();
    m_container = new QWidget(main);
    m_layoutSwitcher = new LayoutSwitcher(m_container);
    connect(m_layoutSwitcher, &LayoutSwitcher::layoutSelected, this,
            static_cast<void (LayoutManagement::*)(const QString &)>(&LayoutManagement::slotLoadLayout));

    auto *l1 = new QHBoxLayout;
    l1->addStretch();
    l1->setContentsMargins(6, 0, 0, 0);
    l1->setSpacing(0);

    // TOOD: This autosave label & timer is not about autosaving layouts but the current project. Need to find a better location for this.
    m_autosaveLabel = new QFrame(main);
    m_autosaveLabel->setAutoFillBackground(false);
    m_autosaveLabel->setToolTip(i18n("Auto Save"));
    QPalette pal = m_autosaveLabel->palette();
    int iconSize = main->style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_autosaveLabel->setFixedSize(iconSize, iconSize);
    pal.setColor(QPalette::Active, QPalette::Button, QColor(80, 250, 80));
    m_autosaveLabel->setPalette(pal);
    l1->addWidget(m_autosaveLabel);
    l1->addWidget(m_layoutSwitcher);
    m_container->setLayout(l1);

    KColorScheme scheme(main->palette().currentColorGroup(), KColorScheme::Button);
    QColor bg = scheme.background(KColorScheme::AlternateBackground).color();
    pal = m_container->palette();
    pal.setColor(QPalette::Active, QPalette::Button, bg);
    m_container->setPalette(pal);
    m_container->setAutoFillBackground(true);
    // TODO: Setting up right corner of the menu bar should also probably sit elsewhere. Wouldn't expect to find this in the layout management class.
    main->menuBar()->setCornerWidget(m_container, Qt::TopRightCorner);

    m_autosaveDisplayTimer.setInterval(2000);
    m_autosaveDisplayTimer.setSingleShot(true);
    connect(&m_autosaveDisplayTimer, &QTimer::timeout, this, &LayoutManagement::hideAutoSave);
    connect(pCore.get(), &Core::startAutoSave, this, &LayoutManagement::startAutoSave);

    // Load layouts and initialize UI
    initializeLayouts();
}

void LayoutManagement::startAutoSave()
{
    m_autosaveLabel->setAutoFillBackground(true);
    m_autosaveDisplayTimer.start();
}

void LayoutManagement::hideAutoSave()
{
    m_autosaveLabel->setAutoFillBackground(false);
}

void LayoutManagement::initializeLayouts()
{
    if (m_loadLayout == nullptr) {
        return;
    }

    // Get the currently selected layout
    m_currentLayoutId = m_layoutSwitcher->currentLayout();
    MainWindow *main = pCore->window();

    // Load layouts from config
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"), KConfig::NoCascade);
    m_layoutCollection.loadFromConfig(config);

    // Get all layouts and use the first 5 for the switcher
    QList<LayoutInfo> allLayouts = m_layoutCollection.getAllLayouts();
    QList<QPair<QString, QString>> switcherLayouts;
    int maxSwitcher = 5;
    for (int i = 0; i < allLayouts.size() && i < maxSwitcher; ++i) {
        const LayoutInfo &layout = allLayouts.at(i);
        switcherLayouts.append(qMakePair(layout.internalId, layout.displayName));
    }

    // Update the switcher with the layouts
    m_layoutSwitcher->setLayouts(switcherLayouts, m_currentLayoutId);

    // Clear and recreate the menu actions
    m_loadLayout->removeAllActions();

    // Set up actions
    for (int i = 0; i < allLayouts.count(); i++) {
        const LayoutInfo &layout = allLayouts.at(i);
        QAction *load = nullptr;

        if (i >= m_layoutActions.count()) {
            load = new QAction(QIcon(), QString(), this);
            m_layoutActions << m_layoutCategory->addAction("load_layout" + QString::number(i + 1), load);
        } else {
            load = m_layoutActions.at(i);
        }

        if (layout.internalId.isEmpty()) {
            load->setText(QString());
            load->setIcon(QIcon());
        } else {
            load->setText(i18n("Layout %1: %2", i + 1, layout.displayName));
        }

        load->setData(layout.internalId);

        if (!layout.internalId.isEmpty()) {
            load->setEnabled(true);
            m_loadLayout->addAction(load);
        } else {
            load->setEnabled(false);
        }
    }

    // Required to trigger a refresh of the container buttons
    main->menuBar()->resize(main->menuBar()->sizeHint());
}

void LayoutManagement::slotLoadLayout(const QString &layoutId)
{
    if (layoutId.isEmpty()) {
        return;
    }
    loadLayout(layoutId);
}

void LayoutManagement::slotLoadLayout(QAction *action)
{
    if (!action) return;
    QString layoutId = action->data().toString();
    if (layoutId.isEmpty()) return;
    slotLoadLayout(layoutId);
}

bool LayoutManagement::loadLayout(const QString &layoutId)
{
    // Get the layout from our collection
    LayoutInfo layout = m_layoutCollection.getLayout(layoutId);
    if (!layout.isValid() || layout.data.isEmpty()) {
        // Layout not found or has no data
        return false;
    }

    // Set as current layout
    m_currentLayoutId = layoutId;

    // Disconnect docks during layout change
    Q_EMIT connectDocks(false);

    // Parse layout data
    QByteArray state = QByteArray::fromBase64(layout.data.toLatin1());
    bool timelineVisible = true;
    if (state.startsWith("NO-TL")) {
        timelineVisible = false;
        state.remove(0, 5);
    }

    // Apply layout
    pCore->window()->centralWidget()->setHidden(!timelineVisible);

    // Restore state disables all toolbars, so remember state
    QList<KToolBar *> barsList = pCore->window()->toolBars();
    QMap<QString, bool> toolbarVisibility;
    for (auto &tb : barsList) {
        toolbarVisibility.insert(tb->objectName(), tb->isVisible());
    }

    // Apply window state
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

    pCore->window()->tabifyBins();

    // Reconnect docks
    Q_EMIT connectDocks(true);

    // Update layout switcher
    m_layoutSwitcher->setCurrentLayout(layoutId);

    // Update title bars
    Q_EMIT updateTitleBars();
    return true;
}

std::pair<QString, QString> LayoutManagement::saveLayout(const QString &layout, const QString &suggestedName)
{
    // Get a suggested name for the layout
    QString visibleName = m_layoutCollection.getTranslatedName(suggestedName);

    // Ask user for a name
    QString layoutName = QInputDialog::getText(pCore->window(), i18nc("@title:window", "Save Layout"), i18n("Layout name:"), QLineEdit::Normal, visibleName);
    if (layoutName.isEmpty()) {
        return {nullptr, nullptr};
    }

    // See if this is a default layout with translation
    QString internalId = layoutName;
    LayoutInfo existingLayout = m_layoutCollection.getLayout(layoutName);

    // Check if this layout already exists
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));

    if (m_layoutCollection.hasLayout(internalId)) {
        // Layout already exists, confirm overwrite
        int res = KMessageBox::questionTwoActions(pCore->window(), i18n("The layout %1 already exists. Do you want to replace it?", layoutName), {},
                                                  KStandardGuiItem::overwrite(), KStandardGuiItem::cancel());
        if (res != KMessageBox::PrimaryAction) {
            return {nullptr, nullptr};
        }
    }

    // Create or update the layout
    LayoutInfo newLayout(internalId, layoutName, layout);
    m_layoutCollection.addLayout(newLayout);

    // Save to config
    m_layoutCollection.saveToConfig(config);

    // Return the created layout names
    return {layoutName, internalId};
}

void LayoutManagement::slotSaveLayout()
{
    // Get current layout name if any
    QString saveName = m_layoutSwitcher->currentLayout();

    // Get current window state
    QByteArray st = pCore->window()->saveState();
    if (!pCore->window()->timelineVisible()) {
        st.prepend("NO-TL");
    }

    // Save the layout
    std::pair<QString, QString> names = saveLayout(st.toBase64(), saveName);

    // Update UI if saved successfully
    if (names.first != nullptr) {
        m_currentLayoutId = names.second;
        m_layoutSwitcher->setCurrentLayout(names.second);
        initializeLayouts();
    }
}

void LayoutManagement::slotManageLayouts()
{
    // Save current layouts
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlive-layoutsrc"));
    QString currentLayoutId = m_layoutSwitcher->currentLayout();

    // Use the new dialog
    LayoutManagerDialog dlg(pCore->window(), LayoutCollection(m_layoutCollection), currentLayoutId);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    // Get the updated collection and selected layout
    m_layoutCollection = dlg.getUpdatedCollection();
    m_layoutCollection.saveToConfig(config);
    initializeLayouts();
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
