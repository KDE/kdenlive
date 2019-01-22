

#include "kdenlivesettings.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QQuickPaintedItem>

class TimelineTriangle : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor MEMBER m_color)
public:
    TimelineTriangle() { setAntialiasing(true); }
    void paint(QPainter *painter) override
    {
        QPainterPath path;
        path.moveTo(0, 0);
        path.lineTo(width(), 0);
        path.lineTo(0, height());
        painter->fillPath(path, m_color);
        painter->setPen(Qt::white);
        painter->drawLine(width(), 0, 0, height());
    }

private:
    QColor m_color;
};

class TimelinePlayhead : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor MEMBER m_color NOTIFY colorChanged)

public:
    TimelinePlayhead()
    {
        connect(this, SIGNAL(colorChanged(QColor)), this, SLOT(update()));
    }

    void paint(QPainter *painter) override
    {
        QPainterPath path;
        path.moveTo(width(), 0);
        path.lineTo(width() / 2.0, height());
        path.lineTo(0, 0);
        painter->fillPath(path, m_color);
    }
signals:
    void colorChanged(const QColor&);
private:
    QColor m_color;
};

class TimelineWaveform : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariant levels MEMBER m_audioLevels NOTIFY propertyChanged)
    Q_PROPERTY(QColor fillColor MEMBER m_color NOTIFY propertyChanged)
    Q_PROPERTY(int inPoint MEMBER m_inPoint NOTIFY inPointChanged)
    Q_PROPERTY(int channels MEMBER m_channels NOTIFY audioChannelsChanged)
    Q_PROPERTY(int outPoint MEMBER m_outPoint NOTIFY outPointChanged)
    Q_PROPERTY(bool format MEMBER m_format NOTIFY propertyChanged)
    Q_PROPERTY(bool showItem MEMBER m_showItem)

public:
    TimelineWaveform()
    {
        setAntialiasing(false);
        setClip(true);
        setEnabled(false);
        setRenderTarget(QQuickPaintedItem::FramebufferObject);
        setMipmap(true);
        setTextureSize(QSize(width(), height()));
        connect(this, SIGNAL(propertyChanged()), this, SLOT(update()));
        // Fill gradient
        /*m_gradient.setStart(0, 0);
        m_gradient.setFinalStop(0, height());
        m_gradient.setColorAt(1, QColor(129, 233, 139));
        m_gradient.setColorAt(0.4, QColor(129, 233, 139));
        m_gradient.setColorAt(0.2, QColor(233, 215, 129));
        m_gradient.setColorAt(0.1, QColor(255, 0, 0));
        m_gradient.setSpread(QGradient::ReflectSpread);*/
    }

    void paint(QPainter *painter) override
    {
        if (!m_showItem) return;
        QVariantList data = m_audioLevels.toList();
        if (data.isEmpty()) return;

        const qreal indicesPrPixel = qreal(m_outPoint - m_inPoint) / width();
        QPen pen = painter->pen();
        pen.setColor(m_color);
        pen.setWidthF(0.5);
        painter->setPen(pen);
        painter->setBrush(m_color);
        if (!KdenliveSettings::displayallchannels()) {
            // Draw merged channels
            QPainterPath path;
            path.moveTo(-1, height());
            double i = 0;
            double increment = qMax(1., 1 / indicesPrPixel);
            int lastIdx = -1;
            for (; i <= width(); i += increment) {
                int idx = m_inPoint + int(i * indicesPrPixel);
                if (lastIdx == idx) {
                    continue;
                }
                lastIdx = idx;
                if (idx + m_channels >= data.length()) break;
                double level = data.at(idx).toDouble() / 256;
                for (int j = 1; j < m_channels; j++) {
                    level = qMax(level, data.at(idx + j).toDouble() / 256);
                }
                path.lineTo(i, height() - level * height());
            }
            path.lineTo(i, height());
            painter->drawPath(path);
        } else {
            // Fill gradient
            double channelHeight = height() / (2 * m_channels);

            // Draw separate channels
            QMap<int, QPainterPath> positiveChannelPaths;
            QMap<int, QPainterPath> negativeChannelPaths;
            double i = 0;
            double increment = qMax(1., 1 / indicesPrPixel);
            QLineF midLine(0, 0, width(), 0);
            QRectF bgRect(0, 0, width(), 2 * channelHeight);
            for (int channel = 0; channel < m_channels; channel++) {
                double y = height() - (2 * channel * channelHeight) - channelHeight;
                positiveChannelPaths[channel].moveTo(-1, y);
                negativeChannelPaths[channel].moveTo(-1, y);
                painter->setOpacity(0.2);
                if (channel % 2 == 0) {
                    // Add dark background on odd channels
                    bgRect.moveTo(0, y - channelHeight);
                    painter->fillRect(bgRect, Qt::black);
                }
                // Draw channel median line
                midLine.translate(0, y);
                painter->drawLine(midLine);
                int lastIdx = -1;
                for (i = 0; i <= width(); i += increment) {
                    int idx = m_inPoint + int(i * indicesPrPixel);
                    if (lastIdx == idx) {
                        continue;
                    }
                    lastIdx = idx;
                    if (idx + channel >= data.length()) break;
                    qreal level = data.at(idx + channel).toReal() / 256;
                    positiveChannelPaths[channel].lineTo(i, y - level * channelHeight);
                    negativeChannelPaths[channel].lineTo(i, y + level * channelHeight);
                }
                painter->setOpacity(1);
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
    void audioChannelsChanged();

private:
    QVariant m_audioLevels;
    int m_inPoint;
    int m_outPoint;
    QColor m_color;
    bool m_format;
    QLinearGradient m_gradient;
    bool m_showItem;
    int m_channels;
};

void registerTimelineItems()
{
    qmlRegisterType<TimelineTriangle>("Kdenlive.Controls", 1, 0, "TimelineTriangle");
    qmlRegisterType<TimelinePlayhead>("Kdenlive.Controls", 1, 0, "TimelinePlayhead");
    qmlRegisterType<TimelineWaveform>("Kdenlive.Controls", 1, 0, "TimelineWaveform");
}

#include "timelineitems.moc"
