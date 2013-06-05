/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MLTCORE_H
#define MLTCORE_H

#include <QMap>
#include <QHash>
#include <QAction>
#include "../monitor/mltcontroller.h"

class QStringList;
class KPluginFactory;
class AbstractParameterDescription;

namespace Mlt {
    class Properties;
    class Repository;
}


/**
 * @class MltCore
 * @brief Initialize some core parameters
 */

class KDE_EXPORT MltCore
{
public:
    /**
     * @brief Constructs the repository.
     */
    MltCore();
    ~MltCore();

    /**
     * @brief Returns an empty parameter description as received from the factory of its plugin.
     * @param type type of the parameter for which the description should be received
     */
    AbstractParameterDescription *newParameterDescription(const QString &type);
    /**
     * @brief returns a list of available display mode for the monitors
     */
    QList <DISPLAYMODE> availableDisplayModes() const;
    /**
     * @brief returns the display name of requested DISPLAYMODE.
     * @param mode the mode for which we want a name
     */
    const QString getDisplayName(DISPLAYMODE mode) const;
    /**
     * @brief returns the global MLT repository
     */
    Mlt::Repository *repository();
    /**
     * @brief Apply a blacklist taken from a file to remove some names from a list
     * @param filename The name of the blacklist .txt file
     * @param list the list we want to check against the blacklist
     */
    static void applyBlacklist(const QString &filename, QStringList &list);
    /**
     * @brief returns the list of pluggable parameter types
     */
    const QHash <QString, KPluginFactory*> parameterPlugins() const;
    /**
     * @brief returns the list of all names contained in the properties list
     * @param properties the Mlt ptoperties container
     * @param names the list where properties names will be put
     */
    static void getNamesFromProperties(Mlt::Properties *properties, QStringList &names);

private:
    void initRepository();
    void checkConsumers();
    void loadParameterPlugins();
    Mlt::Repository *m_repository;
    /** key: parameter types supported by the parameter; value: factory to create parameter description */
    QHash <QString, KPluginFactory*> m_parameterPlugins;
    QList <DISPLAYMODE> m_availableDisplayModes;
};

#endif
