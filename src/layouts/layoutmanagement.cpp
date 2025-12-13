/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "layouts/layoutmanagement.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "layouts/layoutmanagerdialog.h"
#include "layouts/layoutswitcher.h"
#include "mainwindow.h"

#include <KMessageBox>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QVBoxLayout>

#include <KColorScheme>
#include <KConfigGroup>
#include <KIconEffect>
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
    connect(m_loadLayout, &KSelectAction::actionTriggered, this,
            static_cast<void (LayoutManagement::*)(QAction *)>(&LayoutManagement::slotLoadLayoutFromAction));

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

    // Create layout switcher for the menu bar
    MainWindow *main = pCore->window();
    m_container = new QWidget(main);
    m_layoutSwitcher = new LayoutSwitcher(m_container);
    connect(m_layoutSwitcher, &LayoutSwitcher::layoutSelected, this,
            static_cast<bool (LayoutManagement::*)(const QString &)>(&LayoutManagement::slotLoadLayoutById));

    auto *l1 = new QHBoxLayout;
    l1->addStretch();
    // space between the corner widget and the menu bar
    l1->setContentsMargins(6, 0, 0, 0);
    // space between the autosave label and the layout switcher
    l1->setSpacing(0);

    // Create separate containers for autosave label and layout switcher so we can apply different background colors
    m_autosaveContainer = new QWidget(m_container);
    m_autosaveContainer->setAutoFillBackground(false);
    m_switcherContainer = new QWidget(m_container);
    m_switcherContainer->setAutoFillBackground(true);

    auto *autosaveLayout = new QHBoxLayout(m_autosaveContainer);
    autosaveLayout->setContentsMargins(4, 0, 4, 0);
    autosaveLayout->setSpacing(0);

    auto *switcherLayout = new QHBoxLayout(m_switcherContainer);
    switcherLayout->setContentsMargins(0, 0, 0, 0);
    switcherLayout->setSpacing(0);

    // TODO: This autosave label & timer is not about autosaving layouts but the current project. Need to find a better location for this.
    m_autosaveLabel = new QLabel(QString(), m_autosaveContainer);
    m_autosaveLabel->setToolTip(i18n("Auto Save"));
    int iconSize = main->style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_autosaveLabel->setFixedSize(iconSize, iconSize);
    m_autosaveLabel->setScaledContents(true);
    updateAutosaveIcon();
    m_autosaveLabel->hide(); // Initially hidden

    // Set fixed width for autosave container to prevent switcher from moving when icon is shown/hidden (8px for content margins)
    m_autosaveContainer->setFixedWidth(iconSize + 8);

    autosaveLayout->addWidget(m_autosaveLabel);
    switcherLayout->addWidget(m_layoutSwitcher);

    l1->addWidget(m_autosaveContainer);
    l1->addWidget(m_switcherContainer);
    m_container->setLayout(l1);

    slotUpdatePalette();
    connect(pCore.get(), &Core::updatePalette, this, &LayoutManagement::slotUpdatePalette);
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
    m_autosaveContainer->setAutoFillBackground(true);
    m_autosaveLabel->show();
    m_autosaveDisplayTimer.start();
}

void LayoutManagement::hideAutoSave()
{
    m_autosaveLabel->hide();
    m_autosaveContainer->setAutoFillBackground(false);
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
    m_layoutCollection.loadLayouts();

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

void LayoutManagement::slotLoadLayoutFromAction(QAction *action)
{
    if (!action) return;
    const QString layoutId = action->data().toString();
    if (layoutId.isEmpty()) return;
    slotLoadLayoutById(layoutId);
}

bool LayoutManagement::slotLoadLayoutById(const QString &layoutId)
{
    // Get the layout from our collection
    LayoutInfo layout = m_layoutCollection.getLayout(layoutId);
    return slotLoadLayout(layout);
}

bool LayoutManagement::slotLoadLayout(LayoutInfo layout, bool onlyIfNoPrevious)
{
    if (!layout.isValid() || (onlyIfNoPrevious && m_firstLayoutLoaded)) {
        // Layout not found or has no data
        return false;
    }

    // Set as current layout
    m_currentLayoutId = layout.internalId;

    // Parse layout data
    KDDockWidgets::LayoutSaver dockLayout(KDDockWidgets::RestoreOption_RelativeToMainWindow);
    bool restore = false;
    if (pCore->isVertical() && !layout.verticalPath.isEmpty()) {
        restore = dockLayout.restoreFromFile(layout.verticalPath);
    } else if (!layout.path.isEmpty()) {
        restore = dockLayout.restoreFromFile(layout.path);
    } else {
        restore = dockLayout.restoreFromFile(layout.verticalPath);
    }
    if (!restore) {
        pCore->displayBinMessage(i18n("The layout %1 could not be restored, should be removed and recreated.", layout.displayName), KMessageWidget::Warning);
        return false;
    }
    m_firstLayoutLoaded = true;
    m_layoutSwitcher->setCurrentLayout(layout.internalId);
    if (!KdenliveSettings::showtitlebars()) {
        Q_EMIT pCore->hideBars(!KdenliveSettings::showtitlebars());
    }
    return true;
}

void LayoutManagement::adjustLayoutToDar()
{
    if (!m_currentLayoutId.isEmpty()) {
        LayoutInfo lay = m_layoutCollection.getLayout(m_currentLayoutId);
        if (lay.isValid()) {
            if (lay.hasHorizontalData() && lay.hasVerticalData()) {
                slotLoadLayoutById(lay.internalId);
            }
        }
    }
}

bool LayoutManagement::slotLoadLayoutFromData(const QString &layoutData, bool onlyIfNoPrevious)
{
    if (layoutData.isEmpty() || (onlyIfNoPrevious && m_firstLayoutLoaded)) {
        // Layout not found or has no data
        return false;
    }

    KDDockWidgets::LayoutSaver dockLayout(KDDockWidgets::RestoreOption_RelativeToMainWindow);
    bool restore = dockLayout.restoreLayout(layoutData.toUtf8());
    if (!restore) {
        pCore->displayBinMessage(i18n("The layout from the project file could not be restored."), KMessageWidget::Warning);
        return false;
    }
    // Loaded a layout from Kdenlive settings or document
    m_currentLayoutId.clear();
    m_layoutSwitcher->setCurrentLayout(QString());
    if (!KdenliveSettings::showtitlebars() && m_firstLayoutLoaded) {
        Q_EMIT pCore->hideBars(!KdenliveSettings::showtitlebars());
    }
    m_firstLayoutLoaded = true;
    return true;
}

void LayoutManagement::slotSaveLayout()
{
    // Get a suggested name for the layout
    QString saveName = m_layoutCollection.getTranslatedName(m_layoutSwitcher->currentLayout());

    // Ask user for a name
    saveName = QInputDialog::getText(pCore->window(), i18nc("@title:window", "Save Layout"), i18n("Layout name:"), QLineEdit::Normal, saveName);
    if (saveName.isEmpty()) {
        return;
    }
    // Ensure the layout directory exists
    QDir layoutsFolder(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    layoutsFolder.mkdir(QStringLiteral("layouts"));
    if (!layoutsFolder.cd(QStringLiteral("layouts"))) {
        return;
    }
    LayoutInfo layout(saveName, saveName);
    // Get current window state
    QString fileName = saveName;
    if (pCore->isVertical()) {
        fileName.append(QStringLiteral("_vertical"));
    }
    fileName = layoutsFolder.absoluteFilePath(fileName + QStringLiteral(".json"));
    if (QFile::exists(fileName)) {
        if (KMessageBox::questionTwoActions(pCore->window(), i18n("The layout %1 already exists. Do you want to replace it?", saveName), {},
                                            KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
            return;
        }
    }
    KDDockWidgets::LayoutSaver dockLayout(KDDockWidgets::RestoreOption_RelativeToMainWindow);
    if (pCore->isVertical()) {
        layout.verticalPath = fileName;
    } else {
        layout.path = fileName;
    }
    const QByteArray layoutData = dockLayout.serializeLayout();
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(layoutData, &jsonError);
    QJsonArray info;
    QJsonObject kdenliveData;
    kdenliveData.insert(QLatin1String("displayName"), QJsonValue(saveName));
    kdenliveData.insert(QLatin1String("layoutVersion"), QJsonValue(1));
    kdenliveData.insert(QLatin1String("vertical"), QJsonValue(pCore->isVertical()));
    info.push_back(kdenliveData);
    bool ok;
    if (doc.isObject()) {
        QJsonObject updatedObject = doc.object();
        qDebug() << ":::: JSON INSERTED INFO: " << info.toVariantList();
        updatedObject.insert(QStringLiteral("kdenliveInfo"), info);
        QJsonDocument updatedDoc(updatedObject);
        QFile jsonFile(fileName);
        ok = jsonFile.open(QFile::WriteOnly);
        if (ok) {
            ok = jsonFile.write(updatedDoc.toJson());
            jsonFile.close();
        }
    } else {
        ok = false;
        qDebug() << ":::: JSON IS NOT AN OBJECT. IS ARRAY: " << doc.isArray();
    }
    if (!ok) {
        KMessageBox::error(QApplication::activeWindow(), i18n("Could not save layout to file: %1", fileName));
    }

    // Update UI if saved successfully
    m_currentLayoutId = layout.internalId;
    if (m_layoutCollection.hasLayout(layout.internalId)) {
        LayoutInfo currentLayout = m_layoutCollection.getLayout(layout.internalId);
        if (pCore->isVertical()) {
            currentLayout.verticalPath = fileName;
        } else {
            currentLayout.path = fileName;
        }
        m_layoutCollection.addLayout(currentLayout);
    } else {
        m_layoutCollection.addLayout(layout);
    }
    m_layoutSwitcher->setCurrentLayout(m_currentLayoutId);
    initializeLayouts();
}

void LayoutManagement::slotManageLayouts()
{
    QString currentLayoutId = m_layoutSwitcher->currentLayout();

    // Use the new dialog
    LayoutManagerDialog dlg(pCore->window(), m_layoutCollection, currentLayoutId);
    dlg.exec();
    dlg.updateOrder();
    initializeLayouts();
}

void LayoutManagement::slotUpdatePalette()
{
    MainWindow *main = pCore->window();
    QPalette pal = m_switcherContainer->palette();
    KColorScheme scheme(main->palette().currentColorGroup(), KColorScheme::Button);
    QColor bg = scheme.background(KColorScheme::AlternateBackground).color();
    pal.setColor(QPalette::Active, QPalette::Button, bg);
    m_switcherContainer->setPalette(pal);

    pal = m_autosaveContainer->palette();
    // Create similar but more muted version of the same color
    QColor mutedBg = bg;
    mutedBg.setHsv(mutedBg.hue(), qMax(0, mutedBg.saturation() - 30), qMin(255, mutedBg.value() + 20));
    pal.setColor(QPalette::Active, QPalette::Button, mutedBg);
    m_autosaveContainer->setPalette(pal);

    updateAutosaveIcon();
}

void LayoutManagement::updateAutosaveIcon()
{
    MainWindow *main = pCore->window();
    qreal dpr = main->devicePixelRatioF();
    int iconSize = main->style()->pixelMetric(QStyle::PM_SmallIconSize);

    QSize iconPxSize = QSize(iconSize, iconSize) * dpr;
    QIcon icon = QIcon::fromTheme(QStringLiteral("document-save"));
    QPixmap iconPixmap = icon.pixmap(iconPxSize);
    iconPixmap.setDevicePixelRatio(dpr);

    m_autosaveLabel->setPixmap(iconPixmap);
}
