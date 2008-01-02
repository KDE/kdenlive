#ifndef DOCUMENTTRACK_H
#define DOCUMENTTRACK_H

#include <QDomElement>
#include <QList>

class TrackView;

struct TrackViewClip {
  int startTime;
  int duration;
  int cropTime;
  QString producer;
};
  
class DocumentTrack : public QWidget
{
  Q_OBJECT
  
  public:
    DocumentTrack(QDomElement xml, TrackView * view, QWidget *parent=0);

    QList <TrackViewClip> clipList();
    int duration();

  protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

  private:
    QDomElement m_xml;
    QList <TrackViewClip> m_clipList;
    void parseXml();
    int m_trackDuration;

  public slots:

};

#endif
