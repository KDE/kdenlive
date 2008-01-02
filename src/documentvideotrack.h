#ifndef DOCUMENTVIDEOTRACK_H
#define DOCUMENTVIDEOTRACK_H


#include "documenttrack.h"
#include "trackview.h"

class DocumentVideoTrack : public DocumentTrack
{
  Q_OBJECT
  
  public:
    DocumentVideoTrack(QDomElement xml, TrackView * view, QWidget *parent=0);

  protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

  private:
    TrackView *m_trackView;
  public slots:

};

#endif
