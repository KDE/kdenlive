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

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

// Qt includes

#include <QAction>
#include <QtGlobal>

#include <kconfigwidgets_version.h>


// KDE includes
#include <KColorSchemeManager>

class ThemeManager : public QAction
{
    Q_OBJECT
public:
    ThemeManager(QObject *parent);
    QString currentSchemeName() const;

private Q_SLOTS:
    void slotSchemeChanged(QAction *triggeredAction, const QString &path);

private:
    QString loadCurrentScheme() const;
    void saveCurrentScheme(const QString &name);
#if KCONFIGWIDGETS_VERSION < QT_VERSION_CHECK(5, 67, 0)
    QString currentDesktopDefaultScheme() const;
#endif

signals:
    void themeChanged(const QString &name);
};

#endif /* THEMEMANAGER_H */
