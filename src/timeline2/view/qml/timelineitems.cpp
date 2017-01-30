


#include <QQuickPaintedItem>
#include <QPainter>
#include <QPalette>
#include <QPainterPath>


class TimelineTriangle : public QQuickPaintedItem
{
public:
    TimelineTriangle()
    {
        setAntialiasing(QPainter::Antialiasing);
    }
    void paint(QPainter *painter)
    {
        QPainterPath path;
        path.moveTo(0, 0);
        path.lineTo(width(), 0);
        path.lineTo(0, height());
        painter->fillPath(path, Qt::black);
    }
};

class TimelinePlayhead : public QQuickPaintedItem
{
    void paint(QPainter *painter)
    {
        QPainterPath path;
        path.moveTo(width(), 0);
        path.lineTo(width() / 2.0, height());
        path.lineTo(0, 0);
        QPalette p;
        painter->fillPath(path, p.color(QPalette::WindowText));
    }
};


void registerTimelineItems()
{
    qmlRegisterType<TimelineTriangle>("Kdenlive.Controls", 1, 0, "TimelineTriangle");
    qmlRegisterType<TimelinePlayhead>("Kdenlive.Controls", 1, 0, "TimelinePlayhead");
}
