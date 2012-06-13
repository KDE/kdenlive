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
class Project;
class QDomElement;


class KDE_EXPORT AbstractProjectItem : public QObject, public QList<AbstractProjectItem *>
{
    Q_OBJECT

public:
    AbstractProjectItem(AbstractProjectItem *parent = 0);
    AbstractProjectItem(const QDomElement &description, AbstractProjectItem* parent = 0);
    virtual ~AbstractProjectItem();

    bool operator==(const AbstractProjectItem *projectItem) const;

    AbstractProjectItem *parent() const;
    virtual void addChild(AbstractProjectItem *child);
    virtual void childDeleted(AbstractProjectItem *child);
    virtual Project *project();
    int index() const;

    virtual AbstractProjectClip *clip(int id) = 0;

    enum DataType {
        DataName = 0,
        DataDescription,
        DataDate
    };

    virtual QVariant data(DataType type) const;
    virtual int supportedDataCount() const;

    QString name() const;
    virtual void setName(const QString &name);
    QString description() const;
    virtual void setDescription(const QString &description);

    virtual void setCurrent(bool current);
//     virtual bool isSelected();

signals:
    void childAdded(AbstractProjectItem *child);
    void aboutToRemoveChild(AbstractProjectItem *child);

protected:
    AbstractProjectItem *m_parent;
    QString m_name;
    QString m_description;
};

#endif
