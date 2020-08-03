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
#include <locale.h>

auto LocaleHandling::setLocale(const QString &lcName) -> QString
{
    QString newLocale;
    QList<QString> localesToTest;
    localesToTest << lcName << lcName + ".utf-8" << lcName + ".UTF-8" << lcName + ".utf8" << lcName + ".UTF8";
    for (const auto &locale : qAsConst(localesToTest)) {
#ifdef Q_OS_FREEBSD
        auto *result = setlocale(LC_NUMERIC, locale.toStdString().c_str());
#else
        auto *result = std::setlocale(LC_NUMERIC, locale.toStdString().c_str());
#endif
        if (result != nullptr) {
            ::qputenv("LC_NUMERIC", locale.toStdString().c_str());
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
    setlocale(LC_NUMERIC, "C");
#else
    std::setlocale(LC_NUMERIC, "C");
#endif
    ::qputenv("LC_NUMERIC", "C");
    qDebug() << "LC_NUMERIC reset to C";
}

QPair<QLocale, LocaleHandling::MatchType> LocaleHandling::getQLocaleForDecimalPoint(const QString &requestedLocale, const QString &decimalPoint)
{
    // Parse installed locales to find one matching
    const QList<QLocale> list = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale().script(), QLocale::AnyCountry);

    QLocale locale; // Best matching locale
    MatchType matchType = MatchType::NoMatch;

    for (const QLocale &loc : list) {
        if (loc.name().startsWith(requestedLocale)) {
            if (loc.decimalPoint() == decimalPoint) {
                locale = loc;
                matchType = MatchType::Exact;
            }
        }
    }

    if (matchType == MatchType::NoMatch) {
        for (const QLocale &loc : list) {
            if (loc.decimalPoint() == decimalPoint) {
                locale = loc;
                matchType = MatchType::DecimalOnly;
            }
        }
    }

    return QPair<QLocale, LocaleHandling::MatchType>(locale, matchType);
}
