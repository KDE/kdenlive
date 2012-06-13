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
class AbstractProjectClip;
class AbstractProjectItem;
class ProjectFolder;
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
    ProjectFolder *items();
    MonitorModel *binMonitor();
    MonitorModel *timelineMonitor();
    Mlt::Profile *profile();
    TimecodeFormatter *timecodeFormatter();

    AbstractProjectClip *clip(int id);

    void itemsChange();

    QUndoStack *undoStack();

signals:
    void itemsChanged();

private:
    void openFile();
    void openNew();
    void loadClips(const QDomElement &description);
    void loadTimeline(const QString &content);

    KUrl m_url;
    ProjectFolder *m_items;
    Timeline *m_timeline;
    MonitorModel *m_binMonitor;
    TimecodeFormatter *m_timecodeFormatter;

    QUndoStack *m_undoStack;
};

#endif
