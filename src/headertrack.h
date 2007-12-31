#ifndef HEADERTRACK_H
#define HEADERTRACK_H



class HeaderTrack : public QWidget
{
  Q_OBJECT
  
  public:
    HeaderTrack(QWidget *parent=0);

  protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

  private:

  public slots:

};

#endif
