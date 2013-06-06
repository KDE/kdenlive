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

#ifndef SCENEWIDGET_H
#define SCENEWIDGET_H

#include <QSemaphore>
#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLShaderProgram>
#include <QtOpenGL/QGLFramebufferObject>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QGraphicsView>
#include "mltcontroller.h"

class ProducerWrapper;
class MonitorGraphicsScene;


class MyGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    MyGraphicsView(QWidget *parent = 0);
    MyGraphicsView(QGraphicsScene *scene, QWidget *parent = 0);
    
protected:
    void resizeEvent(QResizeEvent* event);
    void showEvent(QShowEvent* event);
    void wheelEvent(QWheelEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent* event);

private:
    Qt::WindowFlags m_baseFlags;
    void switchFullScreen();
    
protected slots:
    virtual void setupViewport(QWidget *viewport)
    {
        QGLWidget *glWidget = dynamic_cast<QGLWidget*>(viewport);
        if (glWidget)
            glWidget->updateGL();
    }

signals:
    void gotWheelEvent(QWheelEvent*);
    void switchPlay();
    void seekTo(double);
};

class SceneWidget : public QGLWidget, public MltController
{
    Q_OBJECT

public:
    SceneWidget(Mlt::Profile *profile, QWidget *parent = 0);
    ~SceneWidget();

    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    /** @brief Open requested producer and get ready for playback. */
    int open(ProducerWrapper*, bool isMulti = false, bool isLive = false);
    /** @brief Configure the MLT consumer. */
    int reconfigure(bool isMulti);
    /** @brief Rebuild consumer if it was closed. */
    int reOpen(bool isMulti = false);
    /** @brief Returns the widget responsible for this monitor's display. */
    QWidget* displayWidget() { return m_view; }
    /** @brief Returns this widget. */
    QWidget* videoWidget() { return this; }
        /** @brief Get thumbnails from a producer.
	 *  @param id the Kdenlive id for the clip item (used to update correct item in bin
	 *  @param producer the producer
	 *  @param positions The list of frame positions where we want thumbnails
	 *  @param width Wanted width for the thumbnail (will be 80 * dar if not specified)
	 *  @param height Wanted height for the thumbnail (will be 80 if not specified)
	 */
    void renderImage(const QString &id, ProducerWrapper *producer, QList<int> positions, int width = -1, int height = -1);

public slots:
    /** @brief This will trigger the display of the frame. */
    void showFrame(Mlt::QFrame);
    /** @brief This will request a thumbnail from the active producer (to display in tooltip). */
    void slotGetThumb(ProducerWrapper *prod, int position);

private slots:
    /** @brief Trigger seek on wheel event. */
    void slotWheelEvent(QWheelEvent* event);
    /** @brief Toggle play / pause and inform monitor view. */
    void slotSwitchPlay();
    /** @brief Seek to a given percentage of the clip. */
    void slotSeekTo(double percentage);

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
    Mlt::Filter* m_glslManager;
    QGLWidget* m_renderContext;
    QGLFramebufferObject* m_fbo;
    QMutex m_mutex;
    QWaitCondition m_condition;
    Mlt::Event* m_threadStartEvent;
    Mlt::Event* m_threadStopEvent;
    mlt_image_format m_image_format;
    bool m_showPlay;
    MyGraphicsView *m_view;
    MonitorGraphicsScene *m_videoScene;

protected:
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent *);
    void enterEvent( QEvent * event );
    void leaveEvent( QEvent * event );
    void createShader();
    void destroyShader();
    void initializeGL();
    void renderImage(const QString &, ProducerWrapper *, int position, int width, int height);
    static void on_frame_show(mlt_consumer, void* self, mlt_frame frame);
};


#endif
