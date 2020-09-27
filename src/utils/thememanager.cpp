/*************************************************************************************
 * This file is part of KDevPlatform                                                 *
 * Copyright 2016 Zhigalin Alexander <alexander@zhigalin.tk>                         *
 *                                                                                   *
 * This library is free software; you can redistribute it and/or                     *
 * modify it under the terms of the GNU Lesser General Public                        *
 * License as published by the Free Software Foundation; either                      *
 * version 2.1 of the License, or (at your option) version 3, or any                 *
 * later version accepted by the membership of KDE e.V. (or its                      *
 * successor approved by the membership of KDE e.V.), which shall                    *
 * act as a proxy defined in Section 6 of version 3 of the license.                  *
 *                                                                                   *
 * This library is distributed in the hope that it will be useful,                   *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                    *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU                 *
 * Lesser General Public License for more details.                                   *
 *                                                                                   *
 * You should have received a copy of the GNU Lesser General Public                  *
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.     *
 *************************************************************************************/

// Qt includes

#include "thememanager.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"

#include <QAction>
#include <QMenu>
#include <QModelIndex>
#include <QStandardPaths>
#include <QStringList>

#include <KColorSchemeManager>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KActionMenu>

ThemeManager::ThemeManager(QObject *parent)
    : QAction(parent)
{
    auto manager = new KColorSchemeManager(parent);

    const auto scheme(currentSchemeName());

    auto selectionMenu = manager->createSchemeSelectionMenu(scheme, this);
    QMenu *themesMenu = selectionMenu->menu();
    // Check for duplicates
    QList<QAction *> actions = themesMenu->actions();
    QStringList existing;
    QList<QAction *> duplicates;
    for (QAction *ac : qAsConst(actions)) {
        if (existing.contains(ac->text())) {
            duplicates << ac;
        } else {
            existing << ac->text();
        }
    }
    for (QAction *ac : qAsConst(duplicates)) {
        themesMenu->removeAction(ac);
    }
    qDeleteAll(duplicates);

    connect(themesMenu, &QMenu::triggered, this, [this, manager](QAction *action) {
        QModelIndex schemeIndex = manager->indexForScheme(KLocalizedString::removeAcceleratorMarker(action->text()));
        const QString path = manager->model()->data(schemeIndex, Qt::UserRole).toString();
        slotSchemeChanged(action, path);
    });

    manager->activateScheme(manager->indexForScheme(scheme));

    setMenu(themesMenu);
    menu()->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-color")));
    menu()->setTitle(i18n("&Color Theme"));
}

QString ThemeManager::loadCurrentScheme() const
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup cg(config, "UiSettings");
#if KCONFIGWIDGETS_VERSION >= QT_VERSION_CHECK(5, 67, 0)
    // Since 5.67 KColorSchemeManager includes a system color scheme option that reacts to system
    // scheme changes. This scheme will be activated if we pass an empty string to KColorSchemeManager
    // So no need anymore to read the current global scheme ourselves if no custom one is configured.
    return cg.readEntry("ColorScheme");
#else
     return cg.readEntry("ColorScheme", currentDesktopDefaultScheme());
#endif
}

void ThemeManager::saveCurrentScheme(const QString &name)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup cg(config, "UiSettings");
    cg.writeEntry("ColorScheme", name);
    cg.sync();
}

#if KCONFIGWIDGETS_VERSION < QT_VERSION_CHECK(5, 67, 0)
QString ThemeManager::currentDesktopDefaultScheme() const
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QLatin1String("kdeglobals"));
    KConfigGroup group(config, "General");
    return group.readEntry("ColorScheme", QStringLiteral("Breeze"));
}
#endif

QString ThemeManager::currentSchemeName() const
{
    if (!menu()) return loadCurrentScheme();

    QAction *const action = menu()->activeAction();

    if (action) return KLocalizedString::removeAcceleratorMarker(action->text());
#if KCONFIGWIDGETS_VERSION >= QT_VERSION_CHECK(5, 67, 0)
    // See above
    return QString();
#else
    return currentDesktopDefaultScheme();
#endif
}

void ThemeManager::slotSchemeChanged(QAction *triggeredAction, const QString &path)
{
    saveCurrentScheme(KLocalizedString::removeAcceleratorMarker(triggeredAction->text()));
    emit themeChanged(path);
}
