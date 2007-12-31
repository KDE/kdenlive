
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <QFrame>
#include <QWidget>
#include <QPainter>


#include "documenttrack.h"

DocumentTrack::DocumentTrack(QWidget *parent)
    : QWidget(parent)
{
  setFixedHeight(50);
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
