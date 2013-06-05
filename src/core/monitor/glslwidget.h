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

#ifndef GLSLWIDGET_H
#define GLSLWIDGET_H

#include <QSemaphore>
#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLFramebufferObject>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include "mltcontroller.h"

class ProducerWrapper;

class GLSLWidget : public QGLWidget, public MltController
{
    Q_OBJECT

public:
    GLSLWidget(Mlt::Profile *profile, QWidget *parent = 0);
    ~GLSLWidget();

    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    void startGlsl();
    void stopGlsl();
    int open(ProducerWrapper*, bool isMulti = false, bool isLive = false);
    void closeConsumer();
    void activate();
    int reOpen(bool isMulti = false);
    int reconfigure(bool isMulti);
    QWidget* displayWidget() { return this; }
    QWidget* videoWidget() { return this; }
    QSemaphore showFrameSemaphore;
    void renderImage(const QString &, ProducerWrapper *, QList<int> positions, int width = -1, int height = -1);
    void refreshConsumer();

public slots:
    void showFrame(Mlt::QFrame);
    void slotGetThumb(ProducerWrapper *producer, int position);

signals:
    /** This method will be called each time a new frame is available.
     * @param frame a Mlt::QFrame from which to get a QImage
     */
    void frameReceived(Mlt::QFrame frame);
    void dragStarted();
    void seekTo(int x);
    void positionChanged(int pos);
    void gpuNotSupported();
    void started();
    void imageRendered(const QString &id, int position, QImage image);
    void producerChanged();
    void gotThumb(int, QImage);
    void requestPlayPause();

private:
    int x, y, w, h;
    int m_image_width, m_image_height;
    GLuint m_texture[1];
    QPoint m_dragStart;
    Mlt::Filter* m_glslManager;
    QGLFramebufferObject* m_fbo;
    QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_isInitialized;
    Mlt::Event* m_threadStartEvent;
    Mlt::Event* m_threadStopEvent;
    mlt_image_format m_image_format;
    bool m_showPlay;
    QGLWidget* m_renderContext;
    QString m_overlay;
    Qt::WindowFlags m_baseFlags;
    void switchFullScreen();


protected:
    void initializeGL();
    void resizeGL(int width, int height);
    void resizeEvent(QResizeEvent* event);
    void showEvent(QShowEvent* event);
    void paintGL();
    void mousePressEvent(QMouseEvent *);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mouseReleaseEvent(QMouseEvent* event);
    void wheelEvent(QWheelEvent* event);
    void mouseMoveEvent(QMouseEvent *);
    void enterEvent( QEvent * event );
    void leaveEvent( QEvent * event );
    static void on_frame_show(mlt_consumer, void* self, mlt_frame frame);
};


#endif
