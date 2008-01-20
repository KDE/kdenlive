#ifndef CUSTOMRULER_H
#define CUSTOMRULER_H

#include <KRuler>

#include <timecode.h>

class CustomRuler : public KRuler
{
  Q_OBJECT
  
  public:
    CustomRuler(Timecode tc, QWidget *parent=0);
    virtual void mousePressEvent ( QMouseEvent * event );
    virtual void mouseMoveEvent ( QMouseEvent * event );
    void setPixelPerMark (double rate);
    static const int comboScale[];
  protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

  private:
    int m_cursorPosition;
    Timecode m_timecode;

  public slots:
    void slotNewValue ( int _value, bool emitSignal = false );

  signals:
    void cursorMoved(int);
};

#endif
