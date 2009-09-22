/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef RECMONITOR_H
#define RECMONITOR_H

#include <QToolBar>
#include <QTimer>
#include <QProcess>
#include <QImage>

#include <KIcon>
#include <KAction>
#include <KRestrictedLine>
#include <KDateTime>
#include <kdeversion.h>

#if KDE_IS_VERSION(4,2,0)
#include <kcapacitybar.h>
#endif

#include "ui_recmonitor_ui.h"

class RecMonitor : public QWidget
{
    Q_OBJECT

public:
    explicit RecMonitor(QString name, QWidget *parent = 0);
    virtual ~RecMonitor();

    QString name() const;

    enum CAPTUREDEVICE {FIREWIRE = 0, VIDEO4LINUX = 1, SCREENGRAB = 2};

protected:
    virtual void mousePressEvent(QMouseEvent * event);

private:
    Ui::RecMonitor_UI m_ui;
    QString m_name;
    bool m_isActive;
    KDateTime m_captureTime;

#if KDE_IS_VERSION(4,2,0)
    KCapacityBar *m_freeSpace;
#endif

    KUrl m_captureFile;
    KIcon m_playIcon;
    KIcon m_pauseIcon;

    QProcess *m_captureProcess;
    QProcess *m_displayProcess;
    bool m_isCapturing;
    /** did the user capture something ? */
    bool m_didCapture;
    bool m_isPlaying;
    QStringList m_captureArgs;
    QStringList m_displayArgs;
    QAction *m_recAction;
    QAction *m_playAction;
    QAction *m_fwdAction;
    QAction *m_rewAction;
    QAction *m_stopAction;
    QAction *m_discAction;
    void checkDeviceAvailability();
    QPixmap mergeSideBySide(const QPixmap& pix, const QString txt);
    void manageCapturedFiles();

#if KDE_IS_VERSION(4,2,0)
    void updatedFreeSpace();
#endif

private slots:
    void slotStartCapture(bool play = true);
    void slotStopCapture();
    void slotRecord();
    void slotProcessStatus(QProcess::ProcessState status);
    void slotVideoDeviceChanged(int ix);
    void slotRewind();
    void slotForward();
    void slotDisconnect();
    //void slotStartGrab(const QRect &rect);
    void slotConfigure();

public slots:
    void refreshRecMonitor(bool visible);
    void stop();
    void start();
    void activateRecMonitor();
    void slotPlay();
    void slotUpdateCaptureFolder();

signals:
    void renderPosition(int);
    void durationChanged(int);
    void addProjectClip(KUrl);
    void showConfigDialog(int, int);
};

#endif
