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

#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QSemaphore>
#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLShaderProgram>
#include <QtOpenGL/QGLFramebufferObject>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include "mltcontroller.h"

class ProducerWrapper;

class GLWidget : public QGLWidget, public MltController
{
    Q_OBJECT

public:
    GLWidget(Mlt::Profile *profile, QWidget *parent = 0);
    ~GLWidget();

    QSize minimumSizeHint() const;
    QSize sizeHint() const;
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
    void slotSetZone(const QPoint &zone);
    void slotSetMarks(QMap <int,QString> marks);

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
    void imageRendered(const QString &id, int position, const QImage &image);
    void producerChanged();
    void gotThumb(int, const QImage&);
    void stateChanged();

private:
    int x, y, w, h;
    int m_image_width, m_image_height;
    GLuint m_texture[3];
    QGLShaderProgram* m_shader;
    QPoint m_dragStart;
    QGLFramebufferObject* m_fbo;
    QGLFramebufferObject* m_fbo2;
    QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_isInitialized;
    mlt_image_format m_image_format;
    bool m_showPlay;
    QString m_overlayText;
    QColor m_overlayColor;
    Qt::WindowFlags m_baseFlags;
    QRectF m_overlayZone;
    GLuint cubeTexture[1];
    void resizeViewPort();
    void switchFullScreen();


protected:
    void initializeGL();
    void resizeGL(int width, int height);
    void resizeEvent(QResizeEvent* event);
    void showEvent(QShowEvent* event);
    //void paintGL();
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mouseReleaseEvent(QMouseEvent* event);
    void wheelEvent(QWheelEvent* event);
    void mouseMoveEvent(QMouseEvent *);
    void enterEvent( QEvent * event );
    void leaveEvent( QEvent * event );
    void createShader();
    void destroyShader();

    static void on_frame_show(mlt_consumer, void* self, mlt_frame frame);
};


#endif
