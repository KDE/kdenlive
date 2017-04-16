

#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QQuickPaintedItem>
#include "kdenlivesettings.h"

class TimelineTriangle : public QQuickPaintedItem
{
public:
    TimelineTriangle() { setAntialiasing(true); }
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
    Q_PROPERTY(bool format MEMBER m_format NOTIFY propertyChanged)

public:
    TimelineWaveform()
    {
        setAntialiasing(true);
        connect(this, SIGNAL(propertyChanged()), this, SLOT(update()));
    }

    void paint(QPainter *painter) override
    {
        QVariantList data = m_audioLevels.toList();
        if (data.isEmpty()) return;

        const qreal indicesPrPixel = qreal(m_outPoint - m_inPoint) / width();
        QPen pen = painter->pen();
        pen.setWidthF(0.5);
        pen.setColor(m_color.darker());
        painter->setPen(pen);
        painter->setBrush(QBrush(m_color.lighter()));

        if (!KdenliveSettings::displayallchannels()) {
            // Draw merged channels
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
            painter->drawPath(path);
        } else {
            // Draw separate channels
            QMap<int, QPainterPath> positiveChannelPaths;
            QMap<int, QPainterPath> negativeChannelPaths;
            // TODO: get channels count
            int channels = 2;
            int i = 0;
            for (int channel = 0; channel < channels; channel++) {
                int y = height() - (2 * channel + 1) * height() / 4;
                positiveChannelPaths[channel].moveTo(-1, y);
                negativeChannelPaths[channel].moveTo(-1, y);
                // Draw channel median line
                painter->drawLine(0, y, width(), y);
                for (i = 0; i < width(); ++i) {
                    int idx = m_inPoint + int(i * indicesPrPixel);
                    if (idx + channel >= data.length()) break;
                    qreal level = data.at(idx + channel).toReal() / 256;
                    positiveChannelPaths[channel].lineTo(i, y - level * height() / 4);
                    negativeChannelPaths[channel].lineTo(i, y + level * height() / 4);
                }
            }
            for (int channel = 0; channel < channels; channel++) {
                int y = height() - (2 * channel + 1) * height() / 4;
                positiveChannelPaths[channel].lineTo(i, y);
                negativeChannelPaths[channel].lineTo(i, y);
                painter->drawPath(positiveChannelPaths.value(channel));
                painter->drawPath(negativeChannelPaths.value(channel));
            }
        }
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
    bool m_format;
};

void registerTimelineItems()
{
    qmlRegisterType<TimelineTriangle>("Kdenlive.Controls", 1, 0, "TimelineTriangle");
    qmlRegisterType<TimelinePlayhead>("Kdenlive.Controls", 1, 0, "TimelinePlayhead");
    qmlRegisterType<TimelineWaveform>("Kdenlive.Controls", 1, 0, "TimelineWaveform");
}

#include "timelineitems.moc"
