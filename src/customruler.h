#ifndef CUSTOMRULER_H
#define CUSTOMRULER_H

#include <KRuler>

#include "customtrackview.h"
#include "timecode.h"

enum RULER_MOVE { RULER_CURSOR = 0, RULER_START = 1, RULER_MIDDLE = 2, RULER_END = 3 };

class CustomRuler : public KRuler {
    Q_OBJECT

public:
    CustomRuler(Timecode tc, CustomTrackView *parent);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    void setPixelPerMark(double rate);
    static const int comboScale[];
    int outPoint() const;
    int inPoint() const;
    void setDuration(int d);

protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

private:
    int m_cursorPosition;
    Timecode m_timecode;
    CustomTrackView *m_view;
    int m_zoneStart;
    int m_zoneEnd;
    int m_duration;
    RULER_MOVE m_moveCursor;

public slots:
    void slotMoveRuler(int newPos);
    void slotCursorMoved(int oldpos, int newpos);

};

#endif
