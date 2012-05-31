/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <KUrl>
#include <kdemacros.h>

class ClipPluginManager;
class Project;
class Bin;
class EffectRepository;
class MonitorView;


class KDE_EXPORT MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &MltPath = QString(),
                        const KUrl &Url = KUrl(), const QString & clipsToLoad = QString(), QWidget* parent = 0);
    virtual ~MainWindow();

    void addDock();

private:
    void initLocale();
    void loadDocks();

    EffectRepository *m_effectRepository;
    ClipPluginManager *m_clipPluginManager;
    Project *m_project;
    Bin *m_bin;
    MonitorView *m_monitor;
};

#endif
