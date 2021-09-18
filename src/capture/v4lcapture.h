/*
    SPDX-FileCopyrightText: 2010 Jean-Baptiste Mardelle (jb@kdenlive.org)

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef V4LCAPTURE_H
#define V4LCAPTURE_H
//#include "src.h"

#include <QStringList>

class V4lCaptureHandler
{

public:
    V4lCaptureHandler();
    static QStringList getDeviceName(const QString &input);
};

#endif
