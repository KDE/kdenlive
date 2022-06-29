/*
    SPDX-FileCopyrightText: 2016 Zhigalin Alexander <alexander@zhigalin.tk>

    SPDX-License-Identifier: LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt includes

#include "thememanager.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"

#include <QFileInfo>
#include <QMenu>
#include <QModelIndex>
#include <QStringList>

#include <KActionMenu>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

ThemeManager::ThemeManager(QObject *parent)
    : QAction(parent)
{
    auto manager = new KColorSchemeManager(parent);

    const auto schemePath(loadCurrentPath());
    auto selectionMenu = manager->createSchemeSelectionMenu(QString(), this);
    auto themesMenu = selectionMenu->menu();
    QString scheme;
    // Check for duplicates
    QList<QAction *> actions = themesMenu->actions();
    QStringList existing;
    QList<QAction *> duplicates;
    for (QAction *ac : qAsConst(actions)) {
        if (existing.contains(ac->text())) {
            duplicates << ac;
        } else {
            existing << ac->text();
            if (schemePath.isEmpty()) {
                if (ac->data().toString().endsWith(QLatin1String("BreezeDark.colors"))) {
                    themesMenu->setActiveAction(ac);
                    scheme = ac->text();
                }
            } else if (ac->data().toString().endsWith(schemePath)) {
                themesMenu->setActiveAction(ac);
                scheme = ac->text();
            }
        }
    }
    for (QAction *ac : qAsConst(duplicates)) {
        themesMenu->removeAction(ac);
    }
    qDeleteAll(duplicates);

    // KColorSchemeManager includes a system color scheme option that reacts to system scheme changes.
    // This scheme will be activated if we pass an empty string to KColorSchemeManager (if "scheme" is empty)

    connect(themesMenu, &QMenu::triggered, this, [this, manager](QAction *action) {
        QModelIndex schemeIndex = manager->indexForScheme(KLocalizedString::removeAcceleratorMarker(action->text()));
        const QString path = manager->model()->data(schemeIndex, Qt::UserRole).toString();
        slotSchemeChanged(path);
    });

    manager->activateScheme(manager->indexForScheme(scheme));

    setMenu(themesMenu);
    menu()->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-color")));
    menu()->setTitle(i18n("&Color Theme"));
}

QString ThemeManager::loadCurrentPath() const
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup cg(config, "UiSettings");
    return cg.readEntry("ColorSchemePath");
}

void ThemeManager::saveCurrentScheme(const QString &path)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup cg(config, "UiSettings");
    cg.writeEntry("ColorSchemePath", path);
    cg.sync();
}

void ThemeManager::slotSchemeChanged(const QString &path)
{
    saveCurrentScheme(QFileInfo(path).fileName());
    emit themeChanged(path);
}
