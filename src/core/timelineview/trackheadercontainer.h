/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TRACKHEADERCONTAINER_H
#define TRACKHEADERCONTAINER_H

#include <QScrollArea>

class Timeline;
class TrackHeaderWidget;


class TrackHeaderContainer : public QScrollArea
{
    Q_OBJECT

public:
    explicit TrackHeaderContainer(QWidget* parent = 0);

    void setTimeline(Timeline *timeline);

public slots:
    // parameters don't suit function name
    void adjustHeight(int min, int max);

private:
    QWidget *m_container;
};

#endif
