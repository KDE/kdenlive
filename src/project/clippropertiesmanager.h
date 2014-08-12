/*
Copyright (C) 2012  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef CLIPPROPERTIESMANAGER_H
#define CLIPPROPERTIESMANAGER_H

#include <QObject>
#include <QMap>

class DocClipBase;
class ProjectList;

/**
 * @class ClipPropertiesManager
 * @brief Handles the clip properties dialog.
 */

class ClipPropertiesManager : public QObject
{
    Q_OBJECT
public:
    explicit ClipPropertiesManager(ProjectList *projectList);

    void showClipPropertiesDialog(DocClipBase *clip);
    void showClipPropertiesDialog(const QList<DocClipBase *> &cliplist, const QMap<QString, QString> &commonproperties);

private slots:
    /** @brief Apply new properties to a clip */
    void slotApplyNewClipProperties(const QString &id, const QMap <QString, QString> &properties,
                                    const QMap <QString, QString> &newProperties, bool refresh, bool reload);

private:
    ProjectList *m_projectList;
};

#endif
