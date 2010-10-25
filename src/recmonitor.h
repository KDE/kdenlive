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

/**
* @class RecMonitor
* @brief Records video via dvgrab, video4linux and recordmydesktop
* @author Jean-Baptiste Mardelle
*/

#ifndef RECMONITOR_H
#define RECMONITOR_H

#include "blackmagic/capture.h"
#include "ui_recmonitor_ui.h"

#include <QToolBar>
#include <QTimer>
#include <QProcess>
#include <QImage>

#include <KIcon>
#include <KAction>
#include <KRestrictedLine>
#include <KDateTime>
#include <kdeversion.h>
#include <KComboBox>

#if KDE_IS_VERSION(4,2,0)
#include <kcapacitybar.h>
#endif

class RecMonitor : public QWidget, public Ui::RecMonitor_UI
{
    Q_OBJECT

public:
    explicit RecMonitor(QString name, QWidget *parent = 0);
    virtual ~RecMonitor();

    QString name() const;

    enum CAPTUREDEVICE {FIREWIRE = 0, VIDEO4LINUX = 1, SCREENGRAB = 2, BLACKMAGIC = 3};

protected:
    virtual void mousePressEvent(QMouseEvent * event);

private:
    QString m_name;
    bool m_isActive;
    KDateTime m_captureTime;
    /** @brief Provide feedback about dvgrab operations */
    QLabel m_dvinfo;
    
    /** @brief Keeps a brief (max ten items) history of warning or error messages
     * 	(currently only used for BLACKMAGIC). */
    KComboBox m_logger;
    QString m_capturePath;

#if KDE_IS_VERSION(4,2,0)
    KCapacityBar *m_freeSpace;
    QTimer m_spaceTimer;
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
    CaptureHandler *m_bmCapture;
    /** @brief Indicates whether we are currently capturing from BLACKMAGIC. */
    bool m_blackmagicCapturing;
    void createBlackmagicDevice();

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
    void slotReadDvgrabInfo();
    void slotUpdateFreeSpace();
    void slotGotBlackmagicFrameNumber(ulong ix);
    void slotGotBlackmagicMessage(const QString &message);

public slots:
    void refreshRecMonitor(bool visible);
    void stop();
    void start();
    void activateRecMonitor();
    void slotPlay();
    void slotUpdateCaptureFolder(const QString currentProjectFolder);

signals:
    void renderPosition(int);
    void durationChanged(int);
    void addProjectClip(KUrl);
    void showConfigDialog(int, int);
};

#endif
