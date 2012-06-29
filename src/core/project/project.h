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

class TimecodeFormatter;
class MonitorModel;
class Timeline;
class BinModel;
class QDomElement;
class QUndoStack;
namespace Mlt
{
    class Profile;
}


/**
 * @class Project
 * @brief ...
 */


class KDE_EXPORT Project : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Loads the project from the supplied url.
     * @param url url of file to open/load
     */
    Project(const KUrl &url, QObject* parent = 0);
    /** @brief Creates a new project. */
    Project(QObject *parent = 0);
    virtual ~Project();

    /** @brief Returns the url of the project file. */
    KUrl url() const;
    /** @brief Returns a short displayable caption describing the project in the format: filename / profile name. */
    QString caption();

    /** @brief Returns a pointer to the timeline. */
    Timeline *timeline();
    /** @brief Returns a pointer to the bin model. */
    BinModel *bin();
    /** @brief Returns a pointer to the bin monitor model. */
    MonitorModel *binMonitor();
    /** @brief Returns a pointer to the timeline monitor model. */
    MonitorModel *timelineMonitor();
    /** @brief Returns a pointer to the used profile. */
    Mlt::Profile *profile();
    /** @brief Returns a pointer to the used timecode formatter. */
    TimecodeFormatter *timecodeFormatter();

    /** @brief Returns a pointer to the project's undo stack. */
    QUndoStack *undoStack();

    /** @brief Retrieves a setting.
     * @param name name of the setting to get
     * 
     * Settings can be used to store simple data in the project. For more complex data or something
     * that requires better separation subclass AbstractProjectPart.
     */
    QString setting(const QString &name) const;
    /**
     * @brief Sets a new settings value.
     * @param name name of value (does not have to exist already)
     * @param value new value
     * 
     * @see setting
     */
    void setSetting(const QString &name, const QString &value);

private:
    void openFile();
    void openNew();
    void loadTimeline(const QString &content);
    void loadParts(const QDomElement &element);
    void loadSettings(const QDomElement &settings);

    KUrl m_url;
    BinModel *m_bin;
    Timeline *m_timeline;
    TimecodeFormatter *m_timecodeFormatter;
    QHash<QString, QString> m_settings;

    QUndoStack *m_undoStack;
};

#endif
