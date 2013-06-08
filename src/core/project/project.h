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
#include <QDomDocument>
#include "kdenlivecore_export.h"

class TimecodeFormatter;
class MonitorView;
class MonitorManager;
class Timeline;
class BinModel;
class QDomElement;
class QDomDocument;
class QUndoStack;
class MltController;
namespace Mlt
{
    class Profile;
}


/**
 * @class Project
 * @brief ...
 */


class KDENLIVECORE_EXPORT Project : public QObject
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
    
    /** @brief Returns the display ratio for this project file, useful for creating thumbs. */
    double displayRatio() const;
    
    /** @brief Returns a short displayable caption describing the project in the format: filename / profile name. */
    QString caption();
    
    /** @brief Returns a string with an available clip id. */
    QString getFreeId();

    /** @brief Returns a pointer to the timeline. */
    Timeline *timeline();
    /** @brief Returns a pointer to the bin model. */
    BinModel *bin();
    /** @brief Returns a pointer to the bin monitor model. */
    MonitorView *binMonitor();
    /** @brief Returns a pointer to the timeline monitor model. */
    MonitorView *timelineMonitor();
    MonitorManager *monitorManager();
    /** @brief Returns a pointer to the used profile. */
    Mlt::Profile *profile() const;
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

    /** @brief Returns a DomDocument that describes this project. */
    QDomDocument toXml() const;
    
    /** @brief Returns current project's folder that will be used to store various data (thumbnails, etc). */
    const KUrl &projectFolder() const;

public slots:
    /** @brief Saves to the current project file. */
    void save();
    /** @brief Saves to a new project file. */
    void saveAs();

private:
    void openFile();
    void openNew();
    void loadTimeline(const QString &content);
    void loadParts(const QDomElement &element = QDomElement());
    QList<QDomElement> saveParts(QDomDocument &document) const;
    void loadSettings(const QDomElement &kdenliveDoc);
    QDomElement saveSettings(QDomDocument &document) const;
    QDomElement convertMltPlaylist(QDomDocument &document);
    QString getXmlProperty(const QDomElement &producer, const QString &propertyName);
    void updateClipCounter(const QDomNodeList clips);
    bool upgradeDocument(QDomElement &kdenliveDoc);

    KUrl m_url;
    KUrl m_projectFolder;
    BinModel *m_bin;
    Timeline *m_timeline;
    TimecodeFormatter *m_timecodeFormatter;
    QHash<QString, QString> m_settings;
    int m_idCounter;

    QUndoStack *m_undoStack;
};

#endif
