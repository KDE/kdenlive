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


/**
 * @class MultiViewHandler
 * @brief Stores the multiple views an item in the effect system has.
 */

class KDE_EXPORT MultiViewHandler : public QObject, private QHash<int, QObject*>
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a view handler which is a child of @p parent.
     * @param parent parent of this handler
     */
    MultiViewHandler(MultiViewHandler* parent = 0);

    /**
     * @brief Sets the view of @p type to @p view.
     * @param type type of the view to set
     * @param view pointer to the view
     * A possible exsiting view for this type needs to be deleted first to avoid a leak.
     */
    void setView(EffectViewTypes type, QObject *view);

    /**
     * @brief Returns a pointer to the requested view or NULL if it does not exist.
     * @param type type of the view to return
     */
    QObject *view(EffectViewTypes type) const;

    /**
     * @brief Returns true if the view exists; otherwise false.
     * @param type type of the view to check on
     */
    bool hasView(EffectViewTypes type) const;

    /**
     * @brief Deletes the view.
     * @param type type of the view to delete
     */
    void deleteView(EffectViewTypes type);

    /**
     * @brief Removes the view from the handler and returns it.
     * @param type type of the view to pop
     * Popping a view means that for other items that use it as an indicator for the view state the
     * view was destroyed.
     */
    QObject *popView(EffectViewTypes type);

    /**
     * @brief Returns the parent's view or NULL if this handler has no parent or the parent handler does not have the view.
     * @param type type of the view to return
     */
    QObject *parentView(EffectViewTypes type);

private:
    MultiViewHandler *m_parent;
};

#endif
