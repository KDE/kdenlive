/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PRODUCERWRAPPER_H
#define PRODUCERWRAPPER_H

#include <mlt++/Mlt.h>
#include <kdemacros.h>

class KUrl;
class QPixmap;


class KDE_EXPORT ProducerWrapper : public Mlt::Producer
{
public:
    ProducerWrapper(Mlt::Producer *producer);
    ProducerWrapper(Mlt::Profile &profile, const KUrl &url);
    virtual ~ProducerWrapper();

    void setProperty(const QString &name, const QString &value);
    QString property(const QString &name);

    QPixmap *pixmap(int position = 0, int width = 0, int height = 0);
};

#endif
