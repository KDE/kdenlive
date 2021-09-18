/*
    SPDX-FileCopyrightText: 2009 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SAMPLEPLUGIN_H
#define SAMPLEPLUGIN_H

#include <QObject>
#include <QStringList>


#include "interfaces.h"

class SamplePlugin : public QObject, public ClipGenerator
{
    Q_OBJECT
    Q_INTERFACES(ClipGenerator)

public:
    QStringList generators(const QStringList &producers = QStringList()) const;
    QUrl generatedClip(const QString &renderer, const QString &generator, const QUrl &projectFolder, const QStringList &lumaNames, const QStringList &lumaFiles,
                       const double fps, const int width, const int height);
};

#endif
