

#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QQuickPaintedItem>

class TimelineTriangle : public QQuickPaintedItem
{
public:
    TimelineTriangle() { setAntialiasing(QPainter::Antialiasing != 0u); }
    void paint(QPainter *painter) override
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
    void paint(QPainter *painter) override
    {
        QPainterPath path;
        path.moveTo(width(), 0);
        path.lineTo(width() / 2.0, height());
        path.lineTo(0, 0);
        QPalette p;
        painter->fillPath(path, p.color(QPalette::WindowText));
    }
};

class TimelineWaveform : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariant levels MEMBER m_audioLevels NOTIFY propertyChanged)
    Q_PROPERTY(QColor fillColor MEMBER m_color NOTIFY propertyChanged)
    Q_PROPERTY(int inPoint MEMBER m_inPoint NOTIFY inPointChanged)
    Q_PROPERTY(int outPoint MEMBER m_outPoint NOTIFY outPointChanged)

public:
    TimelineWaveform()
    {
        setAntialiasing(QPainter::Antialiasing != 0u);
        connect(this, SIGNAL(propertyChanged()), this, SLOT(update()));
    }

    void paint(QPainter *painter) override
    {
        QVariantList data = m_audioLevels.toList();
        if (data.isEmpty()) return;

        const qreal indicesPrPixel = qreal(m_outPoint - m_inPoint) / width();

        QPainterPath path;
        path.moveTo(-1, height());
        int i = 0;
        for (; i < width(); ++i) {
            int idx = m_inPoint + int(i * indicesPrPixel);
            if (idx + 1 >= data.length()) break;
            qreal level = qMax(data.at(idx).toReal(), data.at(idx + 1).toReal()) / 256;
            path.lineTo(i, height() - level * height());
        }
        path.lineTo(i, height());
        painter->fillPath(path, m_color.lighter());

        QPen pen(painter->pen());
        pen.setColor(m_color.darker());
        painter->strokePath(path, pen);
    }

signals:
    void propertyChanged();
    void inPointChanged();
    void outPointChanged();

private:
    QVariant m_audioLevels;
    int m_inPoint;
    int m_outPoint;
    QColor m_color;
};

void registerTimelineItems()
{
    qmlRegisterType<TimelineTriangle>("Kdenlive.Controls", 1, 0, "TimelineTriangle");
    qmlRegisterType<TimelinePlayhead>("Kdenlive.Controls", 1, 0, "TimelinePlayhead");
    qmlRegisterType<TimelineWaveform>("Kdenlive.Controls", 1, 0, "TimelineWaveform");
}

#include "timelineitems.moc"
