/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PROJECTMANAGERVIEW_H
#define PROJECTMANAGERVIEW_H

#include <QObject>

class MainWindow;
class ProjectManager;
class KAction;

/**
 * @class ProjectManagerView
 * @brief Manages Project related actions. No widget itself.
 */

class ProjectManagerView : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManagerView(MainWindow *parent = 0);
    virtual ~ProjectManagerView();

private slots:
    void revertFile();
//     bool closeUnsavedDocument();

private:
    ProjectManager *m_model;
};


#endif
