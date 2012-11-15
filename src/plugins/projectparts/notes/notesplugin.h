/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef NOTESPLUGIN_H
#define NOTESPLUGIN_H

#include "core/project/abstractprojectpart.h"
#include <QVariantList>

class NotesWidget;


class NotesPlugin : public AbstractProjectPart
{
    Q_OBJECT

public:
    explicit NotesPlugin(QObject* parent, const QVariantList &args);

    void save(QDomDocument &document, QDomElement &element) const;
    void load(const QDomElement &element);

private slots:
    void insertTimecode();
    void seekProject(int position);

private:
    NotesWidget *m_widget;
};

#endif
