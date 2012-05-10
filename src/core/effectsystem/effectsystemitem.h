 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef EFFECTSYSTEMITEM_H
#define EFFECTSYSTEMITEM_H

#include "multiviewhandler.h"
#include <QObject>
#include <kdemacros.h>
#include <KDebug>


class KDE_EXPORT EffectSystemItem : public QObject
{
    Q_OBJECT

public:
    EffectSystemItem(EffectSystemItem* parent = 0);
    virtual ~EffectSystemItem();

    MultiViewHandler *getViewHandler();

public slots:
    void checkViewsState();

    virtual void checkPropertiesViewState() = 0;
    virtual void checkTimelineViewState() = 0;
    virtual void checkMonitorViewState() = 0;

protected:
    template <class Iterator>
    void orderedChildViewUpdate(EffectViewTypes type, Iterator childIterator, Iterator end);

    MultiViewHandler *m_viewHandler;

signals:
    virtual void propertiesViewUpdateRequired();
    virtual void timelineViewUpdateRequired();
    virtual void monitorViewUpdateRequired();

private:
    EffectSystemItem *m_parent;
};

template <class Iterator>
void EffectSystemItem::orderedChildViewUpdate(EffectViewTypes type, Iterator childIterator, Iterator end)
{
    while(childIterator != end) {
        switch (type) {
            case EffectPropertiesView:
                (*childIterator)->checkPropertiesViewState();
                break;
            case EffectTimelineView:
                (*childIterator)->checkTimelineViewState();
                break;
            case EffectMonitorView:
                (*childIterator)->checkMonitorViewState();
                break;
        }
        ++childIterator;
    }
}


#endif
