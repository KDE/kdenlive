/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MARKERSWIDGET_H
#define MARKERSWIDGET_H

#include <QWidget>
#include <effectsystem/effectswidget.h>

class KAction;
class QListWidget;
class KToolBar;

/**
 * @class MarkersWidget
 * @brief The timeline widgets contains the sub widgets (track headers, position bar, actual timeline view) and establishes the connections between them.
 */


class MarkersWidget : public ToolPanel
{
    Q_OBJECT

public:
    /** @brief Creates the sub widgets and a tool manager. */
    explicit MarkersWidget(KToolBar *toolbar, QWidget* parent = 0);
    virtual ~MarkersWidget();
    void fillToolBar();
    void setMarkers(const QList <int> &markers);

private slots:
    /** @brief Creates a new timeline scene and triggers the creation of new track headers. */
    void setProject();
    void slotActivateMarker(int ix);
    void slotRemoveMarker();
    
private:
    QListWidget *m_list;
    KAction *m_addAction;
    KAction *m_removeAction;
    KAction *m_editAction;
    KToolBar *m_toolbar;

signals:
    void addMarker();
    void removeMarker(int);
    void seek(int);
};

#endif
