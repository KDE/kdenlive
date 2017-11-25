

#include "kdenlivesettings.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QQuickPaintedItem>

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
        painter->fillPath(path, Qt::red);
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
    Q_PROPERTY(bool showItem MEMBER m_showItem)

public:
    TimelineWaveform()
    {
        setAntialiasing(false);
        setClip(true);
        setEnabled(false);
        setRenderTarget(QQuickPaintedItem::FramebufferObject);
        setMipmap(true);
        setTextureSize(QSize(width(),height()));
        connect(this, SIGNAL(propertyChanged()), this, SLOT(update()));
        // Fill gradient
        m_gradient.setStart(0, 0);
        m_gradient.setFinalStop(0, height());
        m_gradient.setColorAt(1, QColor(129, 233, 139));
        m_gradient.setColorAt(0.4, QColor(129, 233, 139));
        m_gradient.setColorAt(0.2, QColor(233, 215, 129));
        m_gradient.setColorAt(0.1, QColor(255, 0, 0));
        m_gradient.setSpread(QGradient::ReflectSpread);
    }

    void paint(QPainter *painter) override
    {
        if (!m_showItem) return;
        QVariantList data = m_audioLevels.toList();
        if (data.isEmpty()) return;

        const qreal indicesPrPixel = qreal(m_outPoint - m_inPoint) / width();
        QPen pen = painter->pen();
        pen.setWidthF(0.5);
        pen.setColor(Qt::black);

        if (!KdenliveSettings::displayallchannels()) {
            m_gradient.setFinalStop(0, height());
            painter->setBrush(m_gradient);

            // Draw merged channels
            QPainterPath path;
            path.moveTo(-1, height());
            int i = 0;
            int lastIdx = -1;
            for (; i < width(); ++i) {
                int idx = m_inPoint + int(i * indicesPrPixel);
                if (lastIdx == idx) {
                    continue;
                }
                lastIdx = idx;
                if (idx + 1 >= data.length()) break;
                qreal level = qMax(data.at(idx).toReal(), data.at(idx + 1).toReal()) / 256;
                path.lineTo(i, height() - level * height());
            }
            path.lineTo(i, height());
            painter->drawPath(path);
        } else {
            // Fill gradient
            m_gradient.setFinalStop(0, height() / 4);
            painter->setBrush(m_gradient);

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
                int lastIdx = -1;
                for (i = 0; i < width(); ++i) {
                    int idx = m_inPoint + int(i * indicesPrPixel);
                    if (lastIdx == idx) {
                        continue;
                    }
                    lastIdx = idx;
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
    QLinearGradient m_gradient;
    bool m_showItem;
};

void registerTimelineItems()
{
    qmlRegisterType<TimelineTriangle>("Kdenlive.Controls", 1, 0, "TimelineTriangle");
    qmlRegisterType<TimelinePlayhead>("Kdenlive.Controls", 1, 0, "TimelinePlayhead");
    qmlRegisterType<TimelineWaveform>("Kdenlive.Controls", 1, 0, "TimelineWaveform");
}

#include "timelineitems.moc"
