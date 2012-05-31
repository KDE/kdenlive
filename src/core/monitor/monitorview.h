/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MONITORVIEW_H
#define MONITORVIEW_H

#include <QWidget>
#include <kdemacros.h>

class MonitorGraphicsScene;
class MonitorModel;
class PositionBar;
class QGraphicsView;


class KDE_EXPORT MonitorView : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorView(QWidget* parent = 0);
    virtual ~MonitorView();

    void setModel(MonitorModel *model);
    MonitorModel *model();

public slots:
    void togglePlaybackState();

private:
    QGraphicsView *m_videoView;
    MonitorGraphicsScene *m_videoScene;
    MonitorModel *m_model;
    PositionBar *m_positionBar;
};

#endif
