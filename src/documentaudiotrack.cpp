
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <QFrame>
#include <QWidget>
#include <QPainter>


#include "documentaudiotrack.h"

DocumentAudioTrack::DocumentAudioTrack(QDomElement xml, TrackView * view, QWidget *parent)
    : DocumentTrack(xml, view, parent), m_trackView(view)
{
  setFixedHeight(50);
}

// virtual
void DocumentAudioTrack::paintEvent(QPaintEvent *e )
{
    QRect region = e->rect();
    QPainter painter(this);
    painter.fillRect(region, QBrush(Qt::green));
    painter.drawLine(region.bottomLeft (), region.bottomRight ());
}


#include "documentaudiotrack.moc"
