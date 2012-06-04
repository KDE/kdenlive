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

class Timeline;
class AbstractProjectClip;
class AbstractProjectItem;
class ProjectFolder;
class QDomElement;
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
    Timeline *timeline();
    AbstractProjectClip *clip(int id);

    ProjectFolder *items();

    Mlt::Profile *profile();

    void addItem(AbstractProjectItem *item);

signals:
    void itemAdded(AbstractProjectItem *item);

private:
    void loadClips(const QDomElement &description);
    void loadTimeline(const QString &content);

    KUrl m_url;
    ProjectFolder *m_items;
    Timeline *m_timeline;
    
};

#endif
