/*
    SPDX-FileCopyrightText: 2016 Zhigalin Alexander <alexander@zhigalin.tk>

    SPDX-License-Identifier: LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt includes

#include "thememanager.h"
#include "core.h"

#include <QFileInfo>
#include <QMenu>
#include <QModelIndex>
#include <QStringList>

#include <KActionMenu>
#include <KColorSchemeMenu>
#include <KColorSchemeModel>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

ThemeManager::ThemeManager(QObject *parent)
    : KColorSchemeManager(parent)
{
    setAutosaveChanges(false);
    const auto schemePath(loadCurrentPath());

    // KColorSchemeManager includes a system color scheme option that reacts to system scheme changes.
    // This scheme will be activated if we pass an empty string to KColorSchemeManager (if "scheme" is empty)
    QString scheme = loadScheme(schemePath);
    m_menu = KColorSchemeMenu::createMenu(this, this);
    connect(m_menu->menu(), &QMenu::triggered, this, &ThemeManager::schemeActionTriggered);
    if (!scheme.isEmpty()) {
        // We are not using the default color theme, ensure icons have correct color
        Q_EMIT pCore->updatePalette();
    }
}

QString ThemeManager::loadScheme(const QString &schemeName)
{
    QString scheme;
    for (int i = 0; i < model()->rowCount(); ++i) {
        QModelIndex index = model()->index(i, 0);
        if ((schemeName.isEmpty() && index.data(KColorSchemeModel::PathRole).toString().isEmpty()) ||
            index.data(KColorSchemeModel::PathRole).toString().endsWith(schemeName)) {
            scheme = index.data(Qt::DisplayRole).toString();
            break;
        }
    }
    const QModelIndex ix = indexForScheme(scheme);
    activateScheme(ix);
    return scheme;
}

void ThemeManager::switchDarkPalette(bool dark)
{
    const QString schemeFileName = dark ? QStringLiteral("BreezeDark.colors") : QString();
    for (int i = 0; i < model()->rowCount(); ++i) {
        QModelIndex index = model()->index(i, 0);
        if ((schemeFileName.isEmpty() && index.data(KColorSchemeModel::PathRole).toString().isEmpty()) ||
            index.data(KColorSchemeModel::PathRole).toString().endsWith(schemeFileName)) {
            const QString path = model()->data(index, KColorSchemeModel::PathRole).toString();
            auto actions = m_menu->menu()->actions();
            for (auto &a : actions) {
                qDebug() << "::: CHECKOING ACTION: " << a->text() << " = " << a->data().toString();
                if (a->data().toString() == path) {
                    qDebug() << ":::: MATCH!!";
                    a->trigger();
                    break;
                }
            }
            break;
        }
    }
}

QString ThemeManager::loadCurrentPath() const
{
    KConfigGroup cg(KSharedConfig::openConfig(), "UiSettings");
    return cg.readEntry("ColorSchemePath");
}

void ThemeManager::saveCurrentScheme(const QString &path)
{
    KConfigGroup cg(KSharedConfig::openConfig(), "UiSettings");
    cg.writeEntry("ColorSchemePath", path);
    cg.sync();
}

void ThemeManager::schemeActionTriggered(QAction *action)
{
    QModelIndex schemeIndex = indexForScheme(KLocalizedString::removeAcceleratorMarker(action->text()));
    const QString path = model()->data(schemeIndex, KColorSchemeModel::PathRole).toString();
    saveCurrentScheme(QFileInfo(path).fileName());
    Q_EMIT themeChanged(path);
}
