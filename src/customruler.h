#ifndef CUSTOMRULER_H
#define CUSTOMRULER_H

#include <QWidget>

#include "customtrackview.h"
#include "timecode.h"

enum RULER_MOVE { RULER_CURSOR = 0, RULER_START = 1, RULER_MIDDLE = 2, RULER_END = 3 };

class CustomRuler : public QWidget {
    Q_OBJECT

public:
    CustomRuler(Timecode tc, CustomTrackView *parent);
    void setPixelPerMark(double rate);
    static const int comboScale[];
    int outPoint() const;
    int inPoint() const;
    void setDuration(int d);
    void setZone(QPoint p);
    int offset() const;

protected:
    virtual void paintEvent(QPaintEvent * /*e*/);
    virtual void wheelEvent(QWheelEvent * e);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);

private:
    int m_cursorPosition;
    Timecode m_timecode;
    CustomTrackView *m_view;
    int m_zoneStart;
    int m_zoneEnd;
    int m_duration;
    double m_textSpacing;
    double m_factor;
    double m_scale;
    int m_offset;
    RULER_MOVE m_moveCursor;
    QMenu *m_contextMenu;

public slots:
    void slotMoveRuler(int newPos);
    void slotCursorMoved(int oldpos, int newpos);

signals:
    void zoneMoved(int, int);
};

#endif
