#ifndef SMALLRULER_H
#define SMALLRULER_H

#include <KRuler>

class SmallRuler : public KRuler
{
  Q_OBJECT
  
  public:
    SmallRuler(QWidget *parent=0);
    virtual void mousePressEvent ( QMouseEvent * event );

    void setPixelPerMark ( double rate );

  protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

  private:
    int m_cursorPosition;
    double m_scale;

  public slots:
    void slotNewValue ( int _value );

  signals:
    void seekRenderer(int);
};

#endif
