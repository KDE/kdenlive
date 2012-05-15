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


/**
 * @class EffectSystemItem
 * @brief Base class for everything in the effect system.
 * 
 * It should be used by every such class to be able to set up view connections
 * that are required to pass information up and down the different levels (from
 * parameter to the device and vice versa)
 */

class KDE_EXPORT EffectSystemItem : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Connects the parent item's viewUpdateRequired functions to the checkViewState slots.
     * @param parent parent item
     * 
     */
    EffectSystemItem(EffectSystemItem* parent = 0);
    virtual ~EffectSystemItem();

    /**
     * @brief Returns a pointer to the item's viewHandler
     */
    MultiViewHandler *viewHandler();

public slots:
    /**
     * @brief Calls the checkViewState functions.
     */
    void checkViewsState();

    /**
     * @brief Should create or destroy the properties view if necessary.
     * Possible child items also need to be notified from within this function. This can be done
     * either through the propertiesViewUpdateRequired signal or, when multiple children need to
     * be notified in the correct order, by using orderedChildViewUpdate.
     */
    virtual void checkPropertiesViewState() = 0;
    /**
     * @brief Should create or destroy the timeline view if necessary.
     * @see checkPropertiesViewState
     */
    virtual void checkTimelineViewState() = 0;
    /**
     * @brief Should create or destroy the monitor view if necessary.
     * @see checkPropertiesViewState
     */
    virtual void checkMonitorViewState() = 0;

protected:
    /**
     * @brief Calls the checkViewState functions of the children available through the iterator.
     * @param type the type of the view to be checked/updated
     * @param childIterator entry point iterator to iterate over children
     * @param end iterator to determine end of list
     */
    template <class Iterator>
    void orderedChildViewUpdate(MultiViewHandler::EffectViewTypes type, Iterator childIterator, Iterator end);

    /** The item's view handler. Must not be deleted in a subclass! */
    MultiViewHandler *m_viewHandler;

signals:
    /**
     * Used when the properties view was created or destroyed.
     */
    void propertiesViewUpdateRequired();
    /**
     * Used when the timeline view was created or destroyed.
     */
    void timelineViewUpdateRequired();
    /**
     * Used when the monitor view was created or destroyed.
     */
    void monitorViewUpdateRequired();

private:
    EffectSystemItem *m_parent;
};

template <class Iterator>
void EffectSystemItem::orderedChildViewUpdate(MultiViewHandler::EffectViewTypes type, Iterator childIterator, Iterator end)
{
    while(childIterator != end) {
        switch (type) {
            case MultiViewHandler::propertiesView:
                (*childIterator)->checkPropertiesViewState();
                break;
            case MultiViewHandler::timelineView:
                (*childIterator)->checkTimelineViewState();
                break;
            case MultiViewHandler::monitorView:
                (*childIterator)->checkMonitorViewState();
                break;
        }
        ++childIterator;
    }
}


#endif
