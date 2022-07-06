/*
    SPDX-FileCopyrightText: 2016 Zhigalin Alexander <alexander@zhigalin.tk>

    SPDX-License-Identifier: LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

// Qt includes

#include <QAction>
#include <QtGlobal>

#include <kconfigwidgets_version.h>


// KDE includes
#include <KColorSchemeManager>

class ThemeManager : public KColorSchemeManager
{
    Q_OBJECT
public:
    ThemeManager(QObject *parent);
    KActionMenu *menu() { return m_menu; };

private Q_SLOTS:
    void slotSchemeChanged(const QString &path);

private:
    KActionMenu *m_menu;

    QString loadCurrentScheme() const;
    QString loadCurrentPath() const;
    void saveCurrentScheme(const QString & path);
    QString currentSchemeName() const;

signals:
    void themeChanged(const QString &name);
};
