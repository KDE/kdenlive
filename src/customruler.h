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
    void setPixelPerMark(double rate);
    static const int comboScale[];
    int outPoint() const;
    int inPoint() const;
    void setDuration(int d);

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
    RULER_MOVE m_moveCursor;
	QMenu *m_contextMenu;

public slots:
    void slotMoveRuler(int newPos);
    void slotCursorMoved(int oldpos, int newpos);

private slots:
	void slotAddGuide();

};

#endif
