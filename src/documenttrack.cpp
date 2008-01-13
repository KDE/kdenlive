
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <QFrame>
#include <QWidget>
#include <QPainter>


#include "documenttrack.h"

DocumentTrack::DocumentTrack(QDomElement xml, TrackView * view, QWidget *parent)
    : QWidget(parent), m_xml(xml), m_trackDuration(0)
{
  setFixedHeight(50);
  addFunctionDecorator("move", "move");
  parseXml();
}

int DocumentTrack::documentTrackIndex()
{
  return 0;
}

TrackViewClip *DocumentTrack::getClipAt(GenTime pos)
{
  return 0;
}

void DocumentTrack::addFunctionDecorator(const QString & mode, const QString & function) 
{
  m_trackPanelFunctions[mode].append(function);
}

QStringList DocumentTrack::applicableFunctions(const QString & mode) 
{
  return m_trackPanelFunctions[mode];
}

void DocumentTrack::parseXml()
{
  m_clipList.clear();
  int position = 0;
  for(QDomNode n = m_xml.firstChild(); !n.isNull(); n = n.nextSibling())
  {
    QDomElement elem = n.toElement();
   if (elem.tagName() == "blank") {
    position += elem.attribute("length", 0).toInt();
   }
   else if (elem.tagName() == "entry") {
    TrackViewClip clip;
    clip.startTime = position;
    int in = elem.attribute("in", 0).toInt();
    int out = elem.attribute("out", 0).toInt() - in;
    clip.cropTime = in;
    clip.duration = out;
    position += out;
    clip.producer = elem.attribute("producer", QString::null);
    kDebug()<<"++++++++++++++\n\n / / /ADDING CLIP: "<<clip.cropTime<<", out: "<<clip.duration<<", Producer: "<<clip.producer<<"\n\n++++++++++++++++++++";
    m_clipList.append(clip);
   }
  }
  m_trackDuration = position;
}

int DocumentTrack::duration()
{
  return m_trackDuration;
}

QList <TrackViewClip> DocumentTrack::clipList()
{
  return m_clipList;
}

// virtual
void DocumentTrack::paintEvent(QPaintEvent *e )
{
    QRect region = e->rect();
    region.setBottomRight(QPoint(region.right() - 1, region.bottom() - 1));
    QPainter painter(this);
    painter.fillRect(region, QBrush(Qt::red));
    painter.drawLine(region.bottomLeft (), region.bottomRight ());
}


#include "documenttrack.moc"
