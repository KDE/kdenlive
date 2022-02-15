/*
    SPDX-FileCopyrightText: 2016 Zhigalin Alexander <alexander@zhigalin.tk>

    SPDX-License-Identifier: LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

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
    void slotSchemeChanged(const QString &path);

private:
    QString loadCurrentScheme() const;
    QString loadCurrentPath() const;
    void saveCurrentScheme(const QString & path);
#if KCONFIGWIDGETS_VERSION < QT_VERSION_CHECK(5, 67, 0)
    QString currentDesktopDefaultScheme() const;
#endif

signals:
    void themeChanged(const QString &name);
};

#endif /* THEMEMANAGER_H */
