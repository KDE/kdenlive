/*
 * Copyright (c) 2011 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SDLWIDGET_H
#define SDLWIDGET_H

#include <QWidget>
#include <QFrame>
#include "mltcontroller.h"


class VideoContainer : public QFrame
{
    Q_OBJECT
public:
    VideoContainer(QWidget *parent = 0);
    void switchFullScreen();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent *event);
    virtual void wheelEvent(QWheelEvent * event);

private:
    Qt::WindowFlags m_baseFlags;
    
signals:
    void switchPlay();
    void seekTo(double);
    void wheel(int);
};

class VideoSurface : public QWidget
{
    Q_OBJECT
public:
    VideoSurface(QWidget *parent = 0);
    
signals:
    void refreshMonitor();

protected:
    virtual void paintEvent ( QPaintEvent * event );
};

class SDLWidget : public QWidget, public MltController
{
    Q_OBJECT
public:
    explicit SDLWidget(Mlt::Profile *profile, QWidget *parent = 0);
    int open(ProducerWrapper*, bool isMulti = false, bool isLive = false);
    void pause();
    void reStart();
    void seek(int position);
    int reconfigure(bool isMulti);
    QWidget* displayWidget() { return this; }
    QWidget* videoWidget() { return this; }
    void renderImage(const QString &, ProducerWrapper *, QList <int> position, int width = -1, int height = -1);

signals:
    /** This method will be called each time a new frame is available.
     * @param frame a Mlt::QFrame from which to get a QImage
     * @param position the frame number of this frame representing time
     */
    void frameReceived(Mlt::QFrame);
    void dragStarted();
    void seekTo(int x);
    void imageRendered(const QString &id, int position, const QImage &image);
    void started();
    void producerChanged();
    void gotThumb(int, QImage);
    void requestPlayPause();

public slots:
    void slotGetThumb(ProducerWrapper *producer, int position);
    void reStart2();

private slots:    
    void slotSeek(double ratio);
    void slotWheel(int factor);

private:
    QPoint m_dragStart;
    VideoContainer *m_videoBox;
    VideoSurface *m_videoSurface;
    int m_winId;
    void createVideoSurface();

protected:
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    static void on_frame_show(mlt_consumer, void* self, mlt_frame frame);

};


#endif // SDLWIDGET_H
