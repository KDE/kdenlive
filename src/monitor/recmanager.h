/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

/*!
* @class RecManager
* @brief All recording specific features are gathered here
* @author Jean-Baptiste Mardelle
*/

#ifndef RECMANAGER_H
#define RECMANAGER_H

#include "definitions.h"

#include <QUrl>
#include <QProcess>

class Monitor;
class QAction;
class QToolBar;
class QComboBox;


class RecManager : public QObject
{
    Q_OBJECT

enum CaptureDevice {
    Firewire = 0,
    Video4Linux = 1,
    ScreenGrab = 2,
    BlackMagic = 3
};

public:
    explicit RecManager(int iconSize, Monitor *parent = 0);
    ~RecManager();
    QToolBar *toolbar() const;
    void stopCapture();
    QAction *switchAction() const;

private:
    Monitor *m_monitor;
    QAction *m_switchRec;
    QString m_captureFolder;
    QUrl m_captureFile;
    QString m_recError;
    QProcess *m_captureProcess;
    QAction *m_recAction;
    QAction *m_playAction;
    QToolBar *m_recToolbar;
    QComboBox *m_screenCombo;
    QComboBox *m_device_selector;

private slots:
    void slotRecord(bool record);
    void slotPreview(bool record);
    void slotProcessStatus(QProcess::ProcessState status);
    void slotReadProcessInfo();
    void showRecConfig();
    void slotVideoDeviceChanged(int ix);

signals:
    void addClipToProject(QUrl);
    void warningMessage(QString);
};

#endif
