/***************************************************************************
 *   SPDX-FileCopyrightText: 2010 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 ***************************************************************************/

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
