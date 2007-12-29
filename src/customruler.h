#ifndef CUSTOMRULER_H
#define CUSTOMRULER_H

#include <KRuler>

class CustomRuler : public KRuler
{
  Q_OBJECT
  
  public:
    CustomRuler(QWidget *parent=0);
    virtual void mousePressEvent ( QMouseEvent * event );
    void setPixelPerMark (double rate);
    static const int comboScale[];
  protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

  private:
    int m_cursorPosition;

  public slots:
    void slotNewValue ( int _value );
};

#endif
