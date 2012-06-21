/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef KDENLIVE_PROJECT_H
#define KDENLIVE_PROJECT_H

#include <KUrl>
#include <kdemacros.h>

class TimecodeFormatter;
class MonitorModel;
class Timeline;
class BinModel;
class QDomElement;
class QUndoStack;
namespace Mlt
{
    class Profile;
}


class KDE_EXPORT Project : public QObject
{
    Q_OBJECT

public:
    Project(const KUrl &url, QObject* parent = 0);
    Project(QObject *parent = 0);
    virtual ~Project();

    KUrl url() const;
    QString description();

    Timeline *timeline();
    BinModel *bin();
    MonitorModel *binMonitor();
    MonitorModel *timelineMonitor();
    Mlt::Profile *profile();
    TimecodeFormatter *timecodeFormatter();

    QUndoStack *undoStack();

private:
    void openFile();
    void openNew();
    void loadTimeline(const QString &content);
    void loadParts(const QDomElement &element);

    KUrl m_url;
    BinModel *m_bin;
    Timeline *m_timeline;
    TimecodeFormatter *m_timecodeFormatter;

    QUndoStack *m_undoStack;
};

#endif
