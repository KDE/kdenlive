
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <QFrame>
#include <QWidget>
#include <QPainter>


#include "headertrack.h"

HeaderTrack::HeaderTrack(QWidget *parent)
        : QWidget(parent) {
    setFixedHeight(50);
}

// virtual
void HeaderTrack::paintEvent(QPaintEvent *e) {
    QRect region = e->rect();
    region.setBottomRight(QPoint(region.right() - 1, region.bottom() - 1));
    QPainter painter(this);
    painter.drawRect(region);
}


#include "headertrack.moc"
