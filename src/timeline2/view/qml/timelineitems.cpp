/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kdenlivesettings.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "capture/mediacapture.h"
#include "kdenlivesettings.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
#include <QQuickPaintedItem>
#include <QtMath>
#include <cmath>

class TimelineTriangle : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor MEMBER m_color)
public:
    TimelineTriangle(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setAntialiasing(true);
    }
    void paint(QPainter *painter) override
    {
        QPainterPath path;
        path.moveTo(0, 0);
        path.lineTo(width(), 0);
        path.lineTo(0, height());
        painter->fillPath(path, m_color);
        painter->setPen(Qt::white);
        painter->drawLine(int(width()), 0, 0, int(height()));
    }

private:
    QColor m_color;
};

class TimelinePlayhead : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor MEMBER m_color NOTIFY colorChanged)

public:
    TimelinePlayhead(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        connect(this, &TimelinePlayhead::colorChanged, this, [&](const QColor &){ update(); });
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
    void colorChanged(const QColor &);

private:
    QColor m_color;
};

class TimelineWaveform : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor0 MEMBER m_bgColor NOTIFY propertyChanged)
    Q_PROPERTY(QColor fillColor1 MEMBER m_color NOTIFY propertyChanged)
    Q_PROPERTY(QColor fillColor2 MEMBER m_color2 NOTIFY propertyChanged)
    Q_PROPERTY(int waveInPoint MEMBER m_inPoint NOTIFY propertyChanged)
    Q_PROPERTY(int channels MEMBER m_channels NOTIFY propertyChanged)
    Q_PROPERTY(int ix MEMBER m_index)
    Q_PROPERTY(QString binId MEMBER m_binId NOTIFY levelsChanged)
    Q_PROPERTY(int waveOutPoint MEMBER m_outPoint)
    Q_PROPERTY(int waveOutPointWithUpdate MEMBER m_outPoint NOTIFY propertyChanged)
    Q_PROPERTY(int audioStream MEMBER m_stream)
    Q_PROPERTY(double scaleFactor MEMBER m_scale)
    Q_PROPERTY(double speed MEMBER m_speed)
    Q_PROPERTY(bool format MEMBER m_format NOTIFY propertyChanged)
    Q_PROPERTY(bool enforceRepaint MEMBER m_repaint NOTIFY propertyChanged)
    Q_PROPERTY(bool normalize MEMBER m_normalize NOTIFY normalizeChanged)
    Q_PROPERTY(bool isFirstChunk MEMBER m_firstChunk)
    Q_PROPERTY(bool isOpaque MEMBER m_opaquePaint)

public:
    TimelineWaveform(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
        , m_repaint(false)
        , m_speed(1.)
        , m_opaquePaint(false)
    {
        setAntialiasing(false);
        setOpaquePainting(m_opaquePaint);
        setEnabled(false);
        m_precisionFactor = 1;
        //setRenderTarget(QQuickPaintedItem::FramebufferObject);
        //setMipmap(true);
        //setTextureSize(QSize(1, 1));
        connect(this, &TimelineWaveform::levelsChanged, [&]() {
            if (!m_binId.isEmpty()) {
                if (m_audioLevels.isEmpty() && m_stream >= 0) {
                    update();
                } else {
                    // Clip changed, reset levels
                    m_audioLevels.clear();
                }
            }
        });
        connect(this, &TimelineWaveform::normalizeChanged, [&]() {
            m_audioMax = KdenliveSettings::normalizechannels() ? pCore->projectItemModel()->getAudioMaxLevel(m_binId, m_stream) : 0;
            update();
        });
        connect(this, &TimelineWaveform::propertyChanged, this, static_cast<void (QQuickItem::*)()>(&QQuickItem::update));
    }

    void paint(QPainter *painter) override
    {
        if (m_binId.isEmpty()) {
            return;
        }
        if (m_audioLevels.isEmpty() && m_stream >= 0) {
            m_audioLevels = pCore->projectItemModel()->getAudioLevelsByBinID(m_binId, m_stream);
            if (m_audioLevels.isEmpty()) {
                return;
            }
            m_audioMax = KdenliveSettings::normalizechannels() ? pCore->projectItemModel()->getAudioMaxLevel(m_binId, m_stream) : 0;
        }

        if (m_outPoint == m_inPoint) {
            return;
        }
        QRectF bgRect(0, 0, width(), height());
        if (m_opaquePaint) {
            painter->fillRect(bgRect, m_bgColor);
        }
        QPen pen(painter->pen());
        double increment = qMax(1., m_scale / m_channels); //qMax(1., 1. / qAbs(indicesPrPixel));
        qreal indicesPrPixel = m_channels / m_scale * qAbs(m_speed); //qreal(m_outPoint - m_inPoint) / width() * m_precisionFactor;
        int h = int(height());
        double offset = 0;
        bool pathDraw = increment > 1.2;
        if (increment > 1. && !pathDraw) {
            pen.setWidth(int(ceil(increment)));
            offset = pen.width() / 2.;
            pen.setColor(m_color);
            pen.setCapStyle(Qt::FlatCap);
        } else if (pathDraw) {
            pen.setWidth(0);
            painter->setBrush(m_color);
            pen.setColor(m_bgColor.darker(200));
        } else {
            pen.setColor(m_color);
        }
        painter->setPen(pen);
        double scaleFactor = 255;
        if (m_audioMax > 1) {
            scaleFactor = m_audioMax;
        }
        bool reverse = m_speed < 0;
        int maxLength = m_audioLevels.length();
        if (reverse) {
            m_inPoint = qMin(m_inPoint, maxLength - m_channels);
        }
        int startPos = int(m_inPoint / indicesPrPixel);
        if (!KdenliveSettings::displayallchannels()) {
            // Draw merged channels
            double i = 0;
            int j = 0;
            int idx = 0;
            QPainterPath path;
            if (pathDraw) {
                path.moveTo(j - 1, height());
            }
            for (; i <= width(); j++) {
                double level;
                i = j * increment;
                if (reverse) {
                    idx = qCeil((startPos - i) * indicesPrPixel);
                    idx -= idx % m_channels;
                } else {
                    idx = qCeil((startPos + i) * indicesPrPixel);
                    idx += idx % m_channels;
                }
                i -= offset;
                if (idx + m_channels >= maxLength || idx < 0) {
                    break;
                }
                level = m_audioLevels.at(idx) / scaleFactor;
                for (int k = 1; k < m_channels; k++) {
                    level = qMax(level, m_audioLevels.at(idx + k) / scaleFactor);
                }
                if (pathDraw) {
                    double val = height() - level * height();
                    path.lineTo(i, val);
                    path.lineTo(( j + 1) * increment - offset, val);
                } else {
                    painter->drawLine(int(i), h, int(i), int(h - (h * level)));
                }
            }
            if (pathDraw) {
                path.lineTo(i, height());
                painter->drawPath(path);
            }
        } else {
            double channelHeight = height() / m_channels;
            QPen pen(painter->pen());
            // Draw separate channels
            scaleFactor = channelHeight / (2 * scaleFactor);
            bgRect.setHeight(channelHeight);
            // Path for vector drawing
            for (int channel = 0; channel < m_channels; channel++) {
                double level;
                // y is channel median pos
                double y = (channel * channelHeight) + channelHeight / 2;
                QPainterPath path;
                path.moveTo(-1, y);
                if (channel % 2 == 0) {
                    // Add dark background on odd channels
                    painter->setOpacity(0.2);
                    bgRect.moveTo(0, channel * channelHeight);
                    painter->fillRect(bgRect, Qt::black);
                }
                // Draw channel median line
                pen.setColor(channel % 2 == 0 ? m_color : m_color2);
                painter->setBrush(channel % 2 == 0 ? m_color : m_color2);
                painter->setOpacity(0.5);
                pen.setWidthF(0);
                painter->setPen(pen);
                painter->drawLine(QLineF(0., y, width(), y));
                pen.setWidth(int(ceil(increment)));
                painter->setPen(pathDraw ? Qt::NoPen : pen);
                painter->setOpacity(1);
                double i = 0;
                int j = 0;
                int idx = 0;
                for (; i <= width(); j++) {
                    i = j * increment;
                    if (reverse) {
                        idx = qCeil((startPos - i) * indicesPrPixel);
                        idx -= idx % m_channels;
                    } else {
                        idx = qCeil((startPos + i) * indicesPrPixel);
                        idx += idx % m_channels;
                    }
                    i -= offset;
                    idx += channel;
                    if (idx >= maxLength || idx < 0) break;
                    if (pathDraw) {
                        level = m_audioLevels.at(idx) * scaleFactor;
                        path.lineTo(i, y - level);
                    } else {
                        level = m_audioLevels.at(idx) * scaleFactor; // divide height by 510 (2*255) to get height
                        painter->drawLine(int(i), int(y - level), int(i), int(y + level));
                    }
                }
                if (pathDraw) {
                    path.lineTo(i, y);
                    painter->drawPath(path);
                    QTransform tr(1, 0, 0, -1, 0, 2 * y);
                    painter->drawPath(tr.map(path));
                }
                if (m_firstChunk && m_channels > 1 && m_channels < 7) {
                    const QStringList chanelNames{"L", "R", "C", "LFE", "BL", "BR"};
                    painter->drawText(2, int(y + channelHeight / 2), chanelNames[channel]);
                }
            }
        }
    }

signals:
    void levelsChanged();
    void propertyChanged();
    void normalizeChanged();
    void inPointChanged();
    void audioChannelsChanged();

private:
    QVector<uint8_t> m_audioLevels;
    int m_inPoint;
    int m_outPoint;
    QString m_binId;
    QColor m_bgColor;
    QColor m_color;
    QColor m_color2;
    bool m_format;
    bool m_repaint;
    bool m_normalize;
    int m_channels;
    int m_precisionFactor;
    int m_stream;
    double m_scale;
    double m_speed;
    double m_audioMax;
    bool m_firstChunk;
    bool m_opaquePaint;
    int m_index;
};

class TimelineRecWaveform : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor0 MEMBER m_bgColor NOTIFY propertyChanged)
    Q_PROPERTY(QColor fillColor1 MEMBER m_color NOTIFY propertyChanged)
    Q_PROPERTY(QColor fillColor2 MEMBER m_color2 NOTIFY propertyChanged)
    Q_PROPERTY(int waveInPoint MEMBER m_inPoint NOTIFY propertyChanged)
    Q_PROPERTY(int channels MEMBER m_channels NOTIFY propertyChanged)
    Q_PROPERTY(int ix MEMBER m_index)
    Q_PROPERTY(int waveOutPoint MEMBER m_outPoint)
    Q_PROPERTY(int waveOutPointWithUpdate MEMBER m_outPoint NOTIFY propertyChanged)
    Q_PROPERTY(double scaleFactor MEMBER m_scale)
    Q_PROPERTY(bool format MEMBER m_format NOTIFY propertyChanged)
    Q_PROPERTY(bool enforceRepaint MEMBER m_repaint NOTIFY propertyChanged)
    Q_PROPERTY(bool isFirstChunk MEMBER m_firstChunk)
    Q_PROPERTY(bool isOpaque MEMBER m_opaquePaint)

public:
    TimelineRecWaveform(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
        , m_repaint(false)
        , m_opaquePaint(false)
    {
        setAntialiasing(false);
        setOpaquePainting(m_opaquePaint);
        setEnabled(false);
        m_precisionFactor = 1;
        //setRenderTarget(QQuickPaintedItem::FramebufferObject);
        //setMipmap(true);
        //setTextureSize(QSize(1, 1));
        connect(this, &TimelineRecWaveform::propertyChanged, this, static_cast<void (QQuickItem::*)()>(&QQuickItem::update));
    }

    void paint(QPainter *painter) override
    {
        const QVector<double> &audioLevels = pCore->getAudioDevice()->recLevels();
        if (audioLevels.isEmpty()) {
            return;
        }

        if (m_outPoint == m_inPoint) {
            return;
        }
        QRectF bgRect(0, 0, width(), height());
        if (m_opaquePaint) {
            painter->fillRect(bgRect, m_bgColor);
        }
        QPen pen(painter->pen());
        int maxLength = audioLevels.length();
        double increment = 1 / m_scale;
        qreal indicesPrPixel = m_channels / m_scale; //qreal(m_outPoint - m_inPoint) / width() * m_precisionFactor;
        int h = int(height());
        double offset = 0;
        bool pathDraw = increment > 1.2;
        if (increment > 1. && !pathDraw) {
            pen.setWidth(int(ceil(increment)));
            offset = pen.width() / 2.;
            pen.setColor(m_color);
            pen.setCapStyle(Qt::FlatCap);
        } else if (pathDraw) {
            pen.setWidth(0);
            painter->setBrush(m_color);
            pen.setColor(m_bgColor.darker(200));
        } else {
            pen.setColor(m_color);
        }
        painter->setPen(pen);
        int startPos = int(m_inPoint / indicesPrPixel);
        // Draw merged channels
        double i = 0;
        int j = 0;
        int idx = 0;
        QPainterPath path;
        if (pathDraw) {
            path.moveTo(j - 1, height());
        }
        for (; i <= width(); j++) {
            double level;
            i = j * increment;
            idx = qCeil((startPos + i) * indicesPrPixel);
            idx += idx % m_channels;
            i -= offset;
            if (idx + m_channels >= maxLength || idx < 0) {
                break;
            }
            level = audioLevels.at(idx);
            if (pathDraw) {
                double val = height() - level * height();
                path.lineTo(i, val);
                path.lineTo(( j + 1) * increment - offset, val);
            } else {
                painter->drawLine(int(i), h, int(i), int(h - (h * level)));
            }
        }
        if (pathDraw) {
            path.lineTo(i, height());
            painter->drawPath(path);
        }
    }

signals:
    void levelsChanged();
    void propertyChanged();
    void inPointChanged();
    void audioChannelsChanged();

private:
    int m_inPoint;
    int m_outPoint;
    QColor m_bgColor;
    QColor m_color;
    QColor m_color2;
    bool m_format;
    bool m_repaint;
    int m_channels;
    int m_precisionFactor;
    double m_scale;
    bool m_firstChunk;
    bool m_opaquePaint;
    int m_index;
};

void registerTimelineItems()
{
    qmlRegisterType<TimelineTriangle>("Kdenlive.Controls", 1, 0, "TimelineTriangle");
    qmlRegisterType<TimelinePlayhead>("Kdenlive.Controls", 1, 0, "TimelinePlayhead");
    qmlRegisterType<TimelineWaveform>("Kdenlive.Controls", 1, 0, "TimelineWaveform");
    qmlRegisterType<TimelineRecWaveform>("Kdenlive.Controls", 1, 0, "TimelineRecWaveform");
}

#include "timelineitems.moc"
