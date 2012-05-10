/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MULTIUIHANDLER_H
#define MULTIUIHANDLER_H

#include <QObject>
#include <QHash>
#include <kdemacros.h>

enum EffectViewTypes { EffectPropertiesView, EffectTimelineView, EffectMonitorView };


class KDE_EXPORT MultiViewHandler : public QObject, private QHash<int, QObject*>
{
    Q_OBJECT

public:
    MultiViewHandler(MultiViewHandler* parent = 0);

    void setView(EffectViewTypes type, QObject *view);
    QObject *getView(EffectViewTypes type);
    bool hasView(EffectViewTypes type) const;
    void deleteView(EffectViewTypes type);
    QObject *popView(EffectViewTypes type);
    QObject *getParentView(EffectViewTypes type);

private:
    MultiViewHandler *m_parent;
};

#endif
