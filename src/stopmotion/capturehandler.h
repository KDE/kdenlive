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

#ifndef __CAPTUREHANDLER_H__
#define __CAPTUREHANDLER_H__

#include <QWidget>
#include <QObject>
#include <QLayout>

class CaptureHandler : public QObject
{
    Q_OBJECT
public:
    CaptureHandler(QVBoxLayout *lay, QWidget *parent = 0);
    ~CaptureHandler();
    virtual void startPreview(int deviceId, int captureMode, bool audio = true) = 0;
    virtual void stopPreview() = 0;
    virtual void startCapture(const QString &path) = 0;
    virtual void stopCapture();
    virtual void captureFrame(const QString &fname) = 0;
    virtual void showOverlay(QImage img, bool transparent = true) = 0;
    virtual void hideOverlay() = 0;
    virtual void hidePreview(bool hide) = 0;
    virtual QString getDeviceName(QString input, int *width, int *height) = 0;
    static void yuv2rgb(unsigned char *yuv_buffer, unsigned char *rgb_buffer, int width, int height);

protected:
    QVBoxLayout *m_layout;
    QWidget *m_parent;

signals:
    void gotTimeCode(ulong);
    void gotMessage(const QString &);
    void frameSaved(const QString);
};


#endif
