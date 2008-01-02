
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <QFrame>
#include <QWidget>
#include <QPainter>


#include "documentvideotrack.h"

DocumentVideoTrack::DocumentVideoTrack(QDomElement xml, TrackView * view, QWidget *parent)
    : DocumentTrack(xml, view, parent), m_trackView(view)
{
  setFixedHeight(50);
}

// virtual
void DocumentVideoTrack::paintEvent(QPaintEvent *e )
{
    QList <TrackViewClip> trackClipList = clipList();
    double scale = m_trackView->zoomFactor();
    QRect region = e->rect();
    QPainter painter(this);
    //painter.fillRect(region, QBrush(Qt::red));
    painter.drawLine(region.bottomLeft (), region.bottomRight ());
    for (int i = 0; i < trackClipList.size(); ++i) {
      int start = (int) (trackClipList.at(i).startTime * scale);
      int end = (int) (trackClipList.at(i).duration * scale);
      QRect clipRect(start, region.top(), end, region.bottom());
      QPainterPath path;
      painter.setRenderHint(QPainter::Antialiasing);
      painter.fillRect(clipRect, QBrush(Qt::red));
      painter.drawRect(clipRect);

      QRect textRect = painter.boundingRect ( clipRect, Qt::AlignCenter, " " + trackClipList.at(i).producer + " " );
      painter.fillRect(textRect, QBrush(QColor(255, 255, 255, 100)));
      painter.drawText(clipRect, Qt::AlignCenter, trackClipList.at(i).producer);
    }
}


#include "documentvideotrack.moc"
