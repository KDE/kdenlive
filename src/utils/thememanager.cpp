/* ============================================================
 *
 * Code borrowed from digiKam project
 * http://www.digikam.org
 *
 * Date        : 2004-08-02
 * Description : theme manager
 *
 * Copyright (C) 2006-2013 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

// Qt includes

#include <QStringList>
#include <QFileInfo>
#include <QPalette>
#include <QColor>
#include <QAction>
#include <QActionGroup>
#include <QBitmap>
#include <QPainter>
#include <QPixmap>
#include "kdenlive_debug.h"
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QMenu>
// KDE includes

#include <kmessagebox.h>
#include <klocalizedstring.h>
#include <KActionMenu>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <KColorScheme>
#include <ktoolinvocation.h>
#include "thememanager.h"
#include "kdenlivesettings.h"

class ThemeManagerCreator
{
public:

    ThemeManager object;
};

Q_GLOBAL_STATIC(ThemeManagerCreator, creator)

// ---------------------------------------------------------------

class ThemeManager::Private
{
public:

    Private()
        : defaultThemeName(i18nc("default theme name", "Default")),
          themeMenuActionGroup(nullptr),
          themeMenuAction(nullptr)
    {
    }

    const QString          defaultThemeName;
    QMap<QString, QString> themeMap;            // map<theme name, theme config path>

    QActionGroup          *themeMenuActionGroup;
    KActionMenu           *themeMenuAction;
};

ThemeManager::ThemeManager()
    : d(new Private)
{
}

ThemeManager::~ThemeManager()
{
    delete d;
}

ThemeManager *ThemeManager::instance()
{
    return &creator->object;
}

QString ThemeManager::defaultThemeName() const
{
    return d->defaultThemeName;
}

QString ThemeManager::currentThemeName() const
{
    if (!d->themeMenuAction || !d->themeMenuActionGroup) {
        return defaultThemeName();
    }

    QAction *const action = d->themeMenuActionGroup->checkedAction();

    return (!action ? defaultThemeName()
            : action->text().remove('&'));
}

void ThemeManager::setCurrentTheme(const QString &filename)
{
    if (!d->themeMenuAction || !d->themeMenuActionGroup) {
        return;
    }
    QString name = d->themeMap.key(filename, d->defaultThemeName);
    const QList<QAction *> list = d->themeMenuActionGroup->actions();
    for (QAction *const action : list) {
        if (action->text().remove('&') == name) {
            action->setChecked(true);
            //slotChangePalette();
        }
    }
}

void ThemeManager::slotChangePalette()
{
    updateCurrentKDEdefaultThemePreview();

    QString theme(currentThemeName());

    if (theme == defaultThemeName() || theme.isEmpty()) {
        theme = currentKDEdefaultTheme();
    }

    QString filename = d->themeMap.value(theme);

    //qCDebug(KDENLIVE_LOG) << theme << " :: " << filename;

    emit signalThemeChanged(filename);
}

void ThemeManager::setThemeMenuAction(KActionMenu *const action)
{
    d->themeMenuAction = action;
    populateThemeMenu();
}

void ThemeManager::populateThemeMenu()
{
    if (!d->themeMenuAction) {
        return;
    }

    QString theme(currentThemeName());
    d->themeMenuAction->menu()->clear();
    delete d->themeMenuActionGroup;

    d->themeMenuActionGroup = new QActionGroup(d->themeMenuAction);

    connect(d->themeMenuActionGroup, &QActionGroup::triggered,
            this, &ThemeManager::slotChangePalette);

    QAction *action = new QAction(defaultThemeName(), d->themeMenuActionGroup);
    action->setCheckable(true);
    d->themeMenuAction->addAction(action);

    QStringList filters;
    filters << QStringLiteral("*.colors");
    QStringList schemeFiles;
    //const QStringList schemeFiles = KGlobal::dirs()->findAllResources("data", "color-schemes/*.colors", KStandardDirs::NoDuplicates);
    const QStringList colors = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes"), QStandardPaths::LocateDirectory);
    for (const QString &folder : colors) {
        QDir directory(folder);
        QStringList filesnames = directory.entryList(filters, QDir::Files);
        for (const QString &fname : filesnames) {
            schemeFiles << directory.absoluteFilePath(fname);
        }
    }
    QMap<QString, QAction *> actionMap;

    for (int i = 0; i < schemeFiles.size(); ++i) {
        const QString filename  = schemeFiles.at(i);
        const QFileInfo info(filename);
        KSharedConfigPtr config = KSharedConfig::openConfig(filename);
        QIcon icon = createSchemePreviewIcon(config);
        KConfigGroup group(config, "General");
        const QString name = group.readEntry("Name", info.baseName());
        QAction *ac = new QAction(name, d->themeMenuActionGroup);
        d->themeMap.insert(name, filename);
        ac->setIcon(icon);
        ac->setCheckable(true);
        actionMap.insert(name, ac);
    }

    // sort the list
    QStringList actionMapKeys = actionMap.keys();
    actionMapKeys.sort();

    foreach (const QString &name, actionMapKeys) {
        d->themeMenuAction->addAction(actionMap.value(name));
    }

    updateCurrentKDEdefaultThemePreview();
    setCurrentTheme(theme);
    if (!QStandardPaths::findExecutable(QStringLiteral("kcmshell5")).isEmpty()) {
        d->themeMenuAction->addSeparator();
        QAction *config = new QAction(i18n("Configuration..."), d->themeMenuAction);
        config->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme")));
        d->themeMenuAction->addAction(config);

        connect(config, &QAction::triggered,
                this, &ThemeManager::slotConfigColors);
    }
}

void ThemeManager::slotConfigColors()
{
    int ret = KToolInvocation::kdeinitExec(QStringLiteral("kcmshell5"), QStringList() << QStringLiteral("colors"));

    if (ret > 0) {
        KMessageBox::error(nullptr, i18n("Cannot start Colors Settings panel from KDE Control Center. "
                                   "Please check your system..."));
    }
}

void ThemeManager::updateCurrentKDEdefaultThemePreview()
{
    const QList<QAction *> list = d->themeMenuActionGroup->actions();

    for (QAction *const action : list) {
        if (action->text().remove('&') == defaultThemeName()) {
            KSharedConfigPtr config = KSharedConfig::openConfig(d->themeMap.value(currentKDEdefaultTheme()));
            QIcon icon = createSchemePreviewIcon(config);
            action->setIcon(icon);
        }
    }
}

QPixmap ThemeManager::createSchemePreviewIcon(const KSharedConfigPtr &config) const
{
    // code taken from kdebase/workspace/kcontrol/colors/colorscm.cpp
    const uchar bits1[] = { 0xff, 0xff, 0xff, 0x2c, 0x16, 0x0b };
    const uchar bits2[] = { 0x68, 0x34, 0x1a, 0xff, 0xff, 0xff };
    const QSize bitsSize(24, 2);
    const QBitmap b1    = QBitmap::fromData(bitsSize, bits1);
    const QBitmap b2    = QBitmap::fromData(bitsSize, bits2);

    QPixmap pixmap(23, 16);
    pixmap.fill(Qt::black); // FIXME use some color other than black for borders?

    KConfigGroup group(config, "WM");
    QPainter p(&pixmap);
    KColorScheme windowScheme(QPalette::Active, KColorScheme::Window, config);
    p.fillRect(1,  1, 7, 7, windowScheme.background());
    p.fillRect(2,  2, 5, 2, QBrush(windowScheme.foreground().color(), b1));

    KColorScheme buttonScheme(QPalette::Active, KColorScheme::Button, config);
    p.fillRect(8,  1, 7, 7, buttonScheme.background());
    p.fillRect(9,  2, 5, 2, QBrush(buttonScheme.foreground().color(), b1));

    p.fillRect(15,  1, 7, 7, group.readEntry("activeBackground", QColor(96, 148, 207)));
    p.fillRect(16,  2, 5, 2, QBrush(group.readEntry("activeForeground", QColor(255, 255, 255)), b1));

    KColorScheme viewScheme(QPalette::Active, KColorScheme::View, config);
    p.fillRect(1,  8, 7, 7, viewScheme.background());
    p.fillRect(2, 12, 5, 2, QBrush(viewScheme.foreground().color(), b2));

    KColorScheme selectionScheme(QPalette::Active, KColorScheme::Selection, config);
    p.fillRect(8,  8, 7, 7, selectionScheme.background());
    p.fillRect(9, 12, 5, 2, QBrush(selectionScheme.foreground().color(), b2));

    p.fillRect(15,  8, 7, 7, group.readEntry("inactiveBackground", QColor(224, 223, 222)));
    p.fillRect(16, 12, 5, 2, QBrush(group.readEntry("inactiveForeground", QColor(20, 19, 18)), b2));

    p.end();
    return pixmap;
}

QString ThemeManager::currentKDEdefaultTheme() const
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    KConfigGroup group(config, "General");
    return group.readEntry("ColorScheme");
}

void ThemeManager::slotSettingsChanged()
{
    populateThemeMenu();
    slotChangePalette();
}

void ThemeManager::initDarkTheme()
{
    QString defaultTheme = currentKDEdefaultTheme();
    if (defaultTheme.isEmpty() || defaultTheme == QLatin1String("Breeze")) {
        // Switch to breeze dark if available
        if (d->themeMap.contains(QStringLiteral("Breeze Dark"))) {
            KdenliveSettings::setColortheme(d->themeMap.value(QStringLiteral("Breeze Dark")));
            setCurrentTheme(KdenliveSettings::colortheme());
            slotChangePalette();
        }
    }
}
