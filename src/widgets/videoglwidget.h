/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *               2014 by Jean-Nicolas Artaud (jeannicolasartaud@gmail.com) *
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

#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QGLWidget>
#include <QGLFramebufferObject>

namespace Mlt {
class Frame;
}

class VideoGLWidget : public QGLWidget
{
    Q_OBJECT

public:
    explicit VideoGLWidget(QWidget *parent = 0, QGLWidget *share = 0);
    ~VideoGLWidget();
    void activateMonitor();
    void checkOverlay(const QString &overlay);
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    void setImageAspectRatio(double ratio);
    void setBackgroundColor(const QColor &color) {
        m_backgroundColor = color;
    }
    bool sendFrameForAnalysis;
    bool analyseAudio;

public slots:
    void showImage(const QImage &image);
    void showImage(Mlt::Frame*, GLuint, const QString overlay);

protected:
    void initializeGL();
    void resizeGL(int width, int height);
    void resizeEvent(QResizeEvent* event);
    void paintGL();
    void mouseDoubleClickEvent(QMouseEvent * event);

private:
    int x, y, w, h;
    int m_image_width, m_image_height;
    GLuint m_texture;
    Mlt::Frame *m_frame;
    GLuint m_frame_texture;
    double m_display_ratio;
    QColor m_backgroundColor;
    Qt::WindowFlags m_baseFlags;
    QGLFramebufferObject *m_fbo;
    QString m_overlay;
    
signals:
    void frameUpdated(QImage);
    /** @brief This signal contains the audio of the current frame. */
    void audioSamplesSignal(QVector<int16_t>&,int,int,int);
};

#endif
