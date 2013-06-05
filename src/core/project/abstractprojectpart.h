/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTPROJECTPART_H
#define ABSTRACTPROJECTPART_H

#include <QObject>
#include <QDomElement>
#include <kdemacros.h>


/**
 * @class AbstractProjectPart
 * @brief Subclassing AbstractProjectPart allows to store extra information in the project.
 * 
 * For very simple information that does not need a complete separation the project's setting
 * functionality should be used instead.
 * 
 * Objects are registered to the project manager and the individual projects only call load and
 * save but don't instantiate the objects themself.
 */


class KDE_EXPORT AbstractProjectPart : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Registers the part to the project manager. 
     * @param name name of the element this part stores its information in
     */
    explicit AbstractProjectPart(const QString &name, QObject *parent = 0);

    /** @brief Returns the (tag) name of the element this part stores its information in. */
    QString name() const;

    /** @brief Called upon opening/creating a document. */
    virtual void init() = 0;
    /** @brief Called by the project to request the current data. */
    virtual void save(QDomDocument &document, QDomElement &element) const = 0;
    /** @brief Called upon opening a project when data of the part exists. */
    virtual void load(const QDomElement &element) = 0;

    /** 
     * @brief Should be called when the data of this part has changed.
     * 
     * emits modified to notify the project.
     */
    void setModified();

signals:
    void modified();

private:
    QString m_name;
};

#endif
