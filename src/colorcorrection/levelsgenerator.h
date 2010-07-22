/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef LEVELSGENERATOR_H
#define LEVELSGENERATOR_H

#include <QObject>

class QImage;
class QSize;

class LevelsGenerator : public QObject
{
public:
    LevelsGenerator();
    QImage calculateLevels(const QSize &paradeSize, const QImage &image, const int &components, const uint &accelFactor = 1) const;
    enum Components { ComponentY = 1<<0, ComponentR = 1<<1, ComponentG = 1<<2, ComponentB = 1<<3 };
};

#endif // LEVELSGENERATOR_H
