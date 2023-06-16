/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QString>
#include <QStringList>

class QStringUtils
{
public:
    /** @returns a name based on @param name that is unique in @param names.
     *  This is achived by adding a number as suffix to the @param name in case name
     *  does already exist in @param names.
     */
    static QString getUniqueName(const QStringList &names, const QString &name);
    /** Append @param appendix to @param filename before the file extension.
     *  The part of the string after the last dot will be treated as file extension.
     *  Hence it does not work with extensions like .tar.gz
     */
    static QString appendToFilename(const QString &filename, const QString &appendix);
};
