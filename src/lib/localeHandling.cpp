/*
Copyright (C) 2020 Simon A. Eugster <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "localeHandling.h"
#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtGlobal>
#include <clocale>

auto LocaleHandling::setLocale(const QString &lcName) -> QString
{
    QString newLocale;
    QList<QString> localesToTest;
    localesToTest << lcName << lcName + ".utf-8" << lcName + ".UTF-8" << lcName + ".utf8" << lcName + ".UTF8";
    for (const auto &locale : qAsConst(localesToTest)) {
#ifdef Q_OS_FREEBSD
        auto *result = setlocale(MLT_LC_CATEGORY, locale.toStdString().c_str());
#else
        auto *result = std::setlocale(MLT_LC_CATEGORY, locale.toStdString().c_str());
#endif
        if (result != nullptr) {
            ::qputenv(MLT_LC_NAME, locale.toStdString().c_str());
            newLocale = locale;
            break;
        }
    }
    if (newLocale.isEmpty()) {
        resetLocale();
    }
    return newLocale;
}

void LocaleHandling::resetLocale()
{
#ifdef Q_OS_FREEBSD
    setlocale(MLT_LC_CATEGORY, "C");
#else
    std::setlocale(MLT_LC_CATEGORY, "C");
#endif
    ::qputenv(MLT_LC_NAME, "C");
}

void LocaleHandling::resetAllLocale()
{
#ifdef Q_OS_FREEBSD
    setlocale(LC_ALL, "C.UTF-8");
#else
    std::setlocale(LC_ALL, "C.UTF-8");
#endif
    ::qputenv("LC_ALL", "C.UTF-8");
}

QPair<QLocale, LocaleHandling::MatchType> LocaleHandling::getQLocaleForDecimalPoint(const QString &requestedLocale, const QString &decimalPoint)
{
    QLocale locale; // Best matching locale
    MatchType matchType = MatchType::NoMatch;

    // Parse installed locales to find one matching. Check matching language first
    QList<QLocale> list = QLocale::matchingLocales(QLocale().language(), QLocale().script(), QLocale::AnyCountry);
    for (const QLocale &loc : qAsConst(list)) {
        if (loc.decimalPoint() == decimalPoint) {
            locale = loc;
            matchType = MatchType::Exact;
            break;
        }
    }

    if (matchType == MatchType::NoMatch) {
        // Parse installed locales to find one matching. Check in all languages
        list = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale().script(), QLocale::AnyCountry);
        for (const QLocale &loc : qAsConst(list)) {
            if (loc.decimalPoint() == decimalPoint) {
                locale = loc;
                matchType = MatchType::DecimalOnly;
                break;
            }
        }
    }
    if (matchType == MatchType::NoMatch && requestedLocale == QLatin1String("C")) {
        locale = QLocale::c();
        matchType = MatchType::DecimalOnly;
    }
    return QPair<QLocale, LocaleHandling::MatchType>(locale, matchType);
}
