/*
Copyright (C) 2020 Simon A. Eugster <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef KDENLIVE_LOCALEHANDLING_H
#define KDENLIVE_LOCALEHANDLING_H

#include <QtCore/QLocale>
#include <QtCore/QString>
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
};

#endif // KDENLIVE_LOCALEHANDLING_H
