
#include <QMouseEvent>
#include <QStylePainter>
#include <QFrame>
#include <QWidget>
#include <QPainter>

#include <KLocale>
#include <KDebug>
#include "headertrack.h"

HeaderTrack::HeaderTrack(int index, QWidget *parent)
        : QWidget(parent), m_index(index) {
    setFixedHeight(50);
    //setFixedWidth(30);
    m_label = QString::number(m_index);
}

// virtual
void HeaderTrack::paintEvent(QPaintEvent *e) {
    QRect region = e->rect();
    region.setTopLeft(QPoint(region.left() + 1, region.top() + 1));
    region.setBottomRight(QPoint(region.right() - 1, region.bottom() - 1));
    QPainter painter(this);
    painter.fillRect(region, QBrush(QColor(255, 255, 255)));
    painter.drawText(region, Qt::AlignCenter, m_label);
}


#include "headertrack.moc"
