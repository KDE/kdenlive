#ifndef DOCUMENTTRACK_H
#define DOCUMENTTRACK_H



class DocumentTrack : public QWidget
{
  Q_OBJECT
  
  public:
    DocumentTrack(QWidget *parent=0);

  protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

  private:

  public slots:

};

#endif
