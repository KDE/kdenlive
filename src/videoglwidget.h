
#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QGLWidget>

class VideoGLWidget : public QGLWidget
{
    Q_OBJECT

public:
    explicit VideoGLWidget(QWidget *parent = 0);
    ~VideoGLWidget();
    void activateMonitor();
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    void setImageAspectRatio(double ratio);
    void setBackgroundColor(const QColor &color) {
        m_backgroundColor = color;
    }

public slots:
    void showImage(const QImage &image);

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
    double m_display_ratio;
    QColor m_backgroundColor;
    Qt::WindowFlags m_baseFlags;
};

#endif
