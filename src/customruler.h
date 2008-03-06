#ifndef CUSTOMRULER_H
#define CUSTOMRULER_H

#include <KRuler>

#include "customtrackview.h"
#include "timecode.h"

class CustomRuler : public KRuler {
    Q_OBJECT

public:
    CustomRuler(Timecode tc, CustomTrackView *parent);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    void setPixelPerMark(double rate);
    static const int comboScale[];
protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

private:
    int m_cursorPosition;
    Timecode m_timecode;
    CustomTrackView *m_view;

public slots:
    void slotMoveRuler(int newPos);
    void slotCursorMoved(int oldpos, int newpos);

};

#endif
