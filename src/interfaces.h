/*
    SPDX-FileCopyrightText: 2009 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef INTERFACES_H
#define INTERFACES_H

#include <QStringList>

class ClipGenerator
{
public:
    virtual ~ClipGenerator() {}

    virtual QStringList generators(const QStringList &producers = QStringList()) const = 0;
    virtual QUrl generatedClip(const QString &renderer, const QString &generator, const QUrl &projectFolder, const QStringList &lumaNames,
                               const QStringList &lumaFiles, const double fps, const int width, const int height) = 0;
};

Q_DECLARE_INTERFACE(ClipGenerator, "com.kdenlive.ClipGenerator.ClipGeneratorInterface/1.0")

#endif
