/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef UNDOVIEW_H
#define UNDOVIEW_H

#include <QUndoView>

class Project;


class UndoView : public QUndoView
{
    Q_OBJECT

public:
    explicit UndoView(QWidget* parent = 0);

public slots:
    void setProject(Project *project);
};

#endif
