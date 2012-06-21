/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTPROJECTPART_H
#define ABSTRACTPROJECTPART_H

#include <QObject>
#include <QDomElement>
#include <kdemacros.h>


class KDE_EXPORT AbstractProjectPart : public QObject
{
    Q_OBJECT

public:
    explicit AbstractProjectPart(const QString &name, QObject *parent = 0);

    QString name() const;

    virtual QDomElement save() const = 0;
    virtual void load(const QDomElement &element) = 0;

    void setModified();

signals:
    void modified();

private:
    QString m_name;
};

#endif
