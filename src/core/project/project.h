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

class ClipPluginManager;
class Timeline;
class AbstractProjectClip;
class QDomNodeList;


class KDE_EXPORT Project : public QObject
{
    Q_OBJECT

public:
    Project(const KUrl &url, ClipPluginManager *clipPluginManager, QObject* parent = 0);
    Project(QObject *parent = 0);
    virtual ~Project();

    KUrl url() const;
    Timeline *timeline();
    AbstractProjectClip *clip(int id);

private:
    void loadClips(const QDomNodeList &clipEntries, ClipPluginManager *clipPluginManager);
    void loadTimeline(const QString &content);

    KUrl m_url;
    QHash <int, AbstractProjectClip *> m_clips;
    Timeline *m_timeline;
    
};

#endif
