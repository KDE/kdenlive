/***************************************************************************
 *   Copyright (C) 2010 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef __V4LCAPTUREHANDLER_H__
#define __V4LCAPTUREHANDLER_H__

#include "../stopmotion/capturehandler.h"
#include "src.h"

#include <QWidget>
#include <QObject>
#include <QLayout>
#include <QLabel>

class MyDisplay;

class V4lCaptureHandler : public CaptureHandler
{
    Q_OBJECT
public:
    V4lCaptureHandler(QVBoxLayout *lay, QWidget *parent = 0);
    ~V4lCaptureHandler();
    void startPreview(int deviceId, int captureMode, bool audio = true);
    void stopPreview();
    void startCapture(const QString &path);
    void stopCapture();
    void captureFrame(const QString &fname);
    void showOverlay(QImage img, bool transparent = true);
    void hideOverlay();
    void hidePreview(bool hide);
    QString getDeviceName(QString input);

private:
    bool m_update;
    MyDisplay *m_display;
    QString m_captureFramePath;
    QImage m_overlayImage;

private slots:
    void slotUpdate();
};


#endif
