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
#include <KColorSchemeModel>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

ThemeManager::ThemeManager(QObject *parent)
    : KColorSchemeManager(parent)
{
#if KCONFIGWIDGETS_VERSION >= QT_VERSION_CHECK(5, 89, 0)
    setAutosaveChanges(false);
#endif
    const auto schemePath(loadCurrentPath());

    // KColorSchemeManager includes a system color scheme option that reacts to system scheme changes.
    // This scheme will be activated if we pass an empty string to KColorSchemeManager (if "scheme" is empty)
    QString scheme;

    if (!schemePath.isEmpty()) {
        for (int i = 1; i < model()->rowCount(); ++i) {
            QModelIndex index = model()->index(i, 0);
#if KCONFIGWIDGETS_VERSION >= QT_VERSION_CHECK(5, 94, 0)
            if (index.data(KColorSchemeModel::PathRole).toString().endsWith(schemePath)) {
#else
            if (index.data(Qt::UserRole).toString().endsWith(schemePath)) {
#endif
                scheme = index.data(Qt::DisplayRole).toString();
            }
        }
    }

    m_menu = createSchemeSelectionMenu(scheme, this);

    connect(m_menu->menu(), &QMenu::triggered, this, [this](QAction *action) {
        QModelIndex schemeIndex = indexForScheme(KLocalizedString::removeAcceleratorMarker(action->text()));
        const QString path = model()->data(schemeIndex, Qt::UserRole).toString();
        slotSchemeChanged(path);
    });

    activateScheme(indexForScheme(scheme));
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
