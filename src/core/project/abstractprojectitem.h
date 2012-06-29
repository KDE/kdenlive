/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTPROJECTITEM_H
#define ABSTRACTPROJECTITEM_H

#include <QObject>
#include <kdemacros.h>

class AbstractProjectClip;
class BinModel;
class QDomElement;


/**
 * @class AbstractProjectItem
 * @brief Base class for all project items (clips, folders, ...).
 * 
 * Project items are stored in a tree like structure ...
 */


class KDE_EXPORT AbstractProjectItem : public QObject, public QList<AbstractProjectItem *>
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent parent this item should be added to
     */
    AbstractProjectItem(AbstractProjectItem *parent = 0);
    /** 
     * @brief Creates a project item upon project load.
     * @param description element for this item.
     * @param parent parent this item should be added to
     *
     * We try to read the attributes "name" and "description" 
     */
    AbstractProjectItem(const QDomElement &description, AbstractProjectItem* parent = 0);
    virtual ~AbstractProjectItem();

    bool operator==(const AbstractProjectItem *projectItem) const;

    /** @brief Returns a pointer to the parent item (or NULL). */
    AbstractProjectItem *parent() const;
    /** @brief Removes the item from its current parent and adds it as a child to @param parent. */
    virtual void setParent(AbstractProjectItem *parent);

    /**
     * @brief Adds a new child item and notifies the bin model about it (before and after). 
     * @param child project item which should be added as a child
     * 
     * This function is called by setParent.
     */
    virtual void addChild(AbstractProjectItem *child);

    /**
     * @brief Removes a child item and notifies the bin model about it (before and after).
     * @param child project which sould be removed from the child list
     * 
     * This function is called when a child's parent is changed through setParent
     */
    virtual void removeChild(AbstractProjectItem *child);

    /** @brief Returns a pointer to the bin model this item is child of (through its parent item). */
    virtual BinModel *bin();

    /** @brief Returns the index this item has in its parent's child list. */
    int index() const;

    /** @brief Used to search for a clip with a specific id. */
    virtual AbstractProjectClip *clip(int id) = 0;

    enum DataType {
        DataName = 0,
        DataDescription,
        DataDate
    };

    /** @brief Returns the data that describes this item.
     * @param type type of data to return
     *
     * This function is necessary for interaction with ProjectItemModel.
     */
    virtual QVariant data(DataType type) const;

    /** 
     * @brief Returns the amount of different types of data this item supports.
     * 
     * This base class supports only DataName and DataDescription, so the return value is always 2.
     * This function is necessary for interaction with ProjectItemModel.
     */
    virtual int supportedDataCount() const;

    /** @brief Returns the (displayable) name of this item. */
    QString name() const;
    /** @brief Sets a new (displayable) name. */
    virtual void setName(const QString &name);

    /** @brief Returns the (displayable) description of this item. */
    QString description() const;
    /** @brief Sets a new description. */
    virtual void setDescription(const QString &description);

    /** @brief Flags this item as being current (or not) and notifies the bin model about it. */
    virtual void setCurrent(bool current);
//     virtual bool isSelected();

signals:
    void childAdded(AbstractProjectItem *child);
    void aboutToRemoveChild(AbstractProjectItem *child);

protected:
    AbstractProjectItem *m_parent;
    QString m_name;
    QString m_description;

private:
    bool m_isCurrent;
};

#endif
