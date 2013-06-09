/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MONITORMANAGER_H
#define MONITORMANAGER_H

#include <QObject>
#include <QHash>
#include <QMap>
#include "kdenlivecore_export.h"
#include "mltcontroller.h"

class MonitorView;
class Project;
class QSignalMapper;
class MltController;


enum MONITORID {KeepMonitor, AutoMonitor, ClipMonitor, ProjectMonitor, RecordMonitor};

/**
 * @class MonitorManager
 * @brief Takes care of assigning the monitor models to views.
 */


class KDENLIVECORE_EXPORT MonitorManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Creates monitor views. */
    explicit MonitorManager(QObject* parent = 0);

    /** @brief Request thumbnails for a clip
     *  @param id the clip's bin id
     *  @param positions the list of frames for which we want thumbnails
     */
    void requestThumbnails(const QString &id, QList <int> positions);
    /** @brief Activate requested monitor (clip, project)
     *  @param id the requested MONITORID (ClipMonitor, ProjectMonitor,..)
     */
    void requestActivation(MONITORID id);
    /** @brief Returns a list of supported DISPLAYMODES (MLTOPENGL, MLTSDL,...)
     */
    QList <DISPLAYMODE> supportedDisplayModes();
    /** @brief Is this DISPLAYMODE supported on the system
     *  @param mode the mode we want to test
     */
    bool isSupported(DISPLAYMODE mode) const;
    /** @brief Returns the display name of a DISPLAYMODE.
     */
    const QString getDisplayName(DISPLAYMODE mode) const;
    /** @brief Is this DISPLAYMODE available on the system. For example in a 2 monitors config, only one GLSL display is allowed
     *  @param mode the requested DISPLAYMODE
     */
    bool isAvailable(DISPLAYMODE mode);
    /** @brief Does this DISPLAYMODE allow multiple monitor view ?
     *  @param mode the requested DISPLAYMODE
     */
    static bool requiresUniqueDisplay(DISPLAYMODE mode);
    
private slots:
    void setProject(Project *project);
    //void onModelActivated(MONITORID id);
    /** @brief The controller for a monitor has changed (controller is tied to a DISPLAYMODE), so happens when switching mode
     *  @param id The monitor's id (ClipMonitor, ProjectMonitor)
     *  @param controller The new controller for this monitor.
     */
    void updateController(MONITORID id, MltController *controller);
    void slotManageZoneChange(MONITORID id,const QPoint &zone);

private:
    QHash<MONITORID, MonitorView*> m_monitors;
    //QSignalMapper *m_modelSignalMapper;
    QMap <QString, QList<int> > m_thumbRequests;
};

#endif
