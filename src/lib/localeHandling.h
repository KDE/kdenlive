/*
SPDX-FileCopyrightText: 2020 Simon A. Eugster <simon.eu@gmail.com>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QtCore/QLocale>
#include <QtCore/QString>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
#   define MLT_LC_CATEGORY LC_NUMERIC
#   define MLT_LC_NAME     "LC_NUMERIC"
#else
#   define MLT_LC_CATEGORY LC_ALL
#   define MLT_LC_NAME     "LC_ALL"
#endif

class LocaleHandling
{
public:

    enum class MatchType { Exact = 0, DecimalOnly = 1, NoMatch = 2 };

    /**
     * Set LC_ALL to the desired locale.
     * The function also tries variants of .utf-8 appendixes if setting the plain locale fails.
     * @return The locale which was set, or an empty string if no locale could be set (e.g. not installed on the system).
     */
    static QString setLocale(const QString &lcName);

    static QPair<QLocale, LocaleHandling::MatchType> getQLocaleForDecimalPoint(const QString &requestedLocale, const QString &decimalPoint);

    /**
     * Reset LC_ALL to "C".
     * This is used for MLT to ensure that numbers are always serialised the same way
     * regardless of the user's locale; many locales use , or another character as decimal point.
     */
    static void resetLocale();
    static void resetAllLocale();
};
