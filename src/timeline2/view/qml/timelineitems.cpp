/*
Based on Shotcut, Copyright (c) 2015-2016 Meltytech, LLC
Copyright (C) 2019  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "kdenlivesettings.h"
#include "core.h"
#include "bin/projectitemmodel.h"
#include <QPainter>
#include <QPainterPath>
#include <QQuickPaintedItem>
#include <QElapsedTimer>
#include <cmath>

const QStringList chanelNames{"L", "R", "C", "LFE", "BL", "BR"};

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
    TimelinePlayhead() { connect(this, SIGNAL(colorChanged(QColor)), this, SLOT(update())); }

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
    Q_PROPERTY(QColor fillColor1 MEMBER m_color NOTIFY propertyChanged)
    Q_PROPERTY(QColor fillColor2 MEMBER m_color2 NOTIFY propertyChanged)
    Q_PROPERTY(int waveInPoint MEMBER m_inPoint NOTIFY propertyChanged)
    Q_PROPERTY(int drawInPoint MEMBER m_drawInPoint NOTIFY propertyChanged)
    Q_PROPERTY(int drawOutPoint MEMBER m_drawOutPoint NOTIFY propertyChanged)
    Q_PROPERTY(int channels MEMBER m_channels NOTIFY audioChannelsChanged)
    Q_PROPERTY(QString binId MEMBER m_binId NOTIFY levelsChanged)
    Q_PROPERTY(int waveOutPoint MEMBER m_outPoint)
    Q_PROPERTY(int waveOutPointWithUpdate MEMBER m_outPoint NOTIFY propertyChanged)
    Q_PROPERTY(int audioStream MEMBER m_stream)
    Q_PROPERTY(bool format MEMBER m_format NOTIFY propertyChanged)
    Q_PROPERTY(bool showItem READ showItem  WRITE setShowItem NOTIFY showItemChanged)
    Q_PROPERTY(bool isFirstChunk MEMBER m_firstChunk)

public:
    TimelineWaveform()
    {
        setAntialiasing(false);
        // setClip(true);
        setEnabled(false);
        m_showItem = false;
        m_precisionFactor = 1;
        //setRenderTarget(QQuickPaintedItem::FramebufferObject);
        //setMipmap(true);
        setTextureSize(QSize(1, 1));
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
        connect(this, &TimelineWaveform::propertyChanged, [&]() {
            update();
        });
    }
    bool showItem() const
    {
        return m_showItem;
    }
    void setShowItem(bool show)
    {
        m_showItem = show;
        if (show) {
            setTextureSize(QSize(width(), height()));
            update();
        } else {
            // Free memory
            setTextureSize(QSize(1, 1));
        }
    }

    void paint(QPainter *painter) override
    {
        if (!m_showItem || m_binId.isEmpty()) {
            return;
        }
        if (m_audioLevels.isEmpty() && m_stream >= 0) {
            m_audioLevels = pCore->projectItemModel()->getAudioLevelsByBinID(m_binId, m_stream);
            if (m_audioLevels.isEmpty()) {
                return;
            }
        }
        qreal indicesPrPixel = qreal(m_outPoint - m_inPoint) / width() * m_precisionFactor;
        QPen pen = painter->pen();
        pen.setColor(m_color);
        painter->setBrush(m_color);
        pen.setCapStyle(Qt::FlatCap);
        double increment = qMax(1., 1. / qAbs(indicesPrPixel));
        int h = height();
        double offset = 0;
        bool pathDraw = increment > 1.2;
        if (increment > 1. && !pathDraw) {
            pen.setWidth(ceil(increment));
            offset = pen.width() / 2.;
        } else if (pathDraw) {
            pen.setWidthF(0);
        }
        painter->setPen(pen);
        int startPos = m_inPoint / indicesPrPixel;
        if (!KdenliveSettings::displayallchannels()) {
            // Draw merged channels
            double i = 0;
            double level;
            int j = 0;
            QPainterPath path;
            if (m_drawInPoint > 0) {
                j = m_drawInPoint / increment;
            }
            if (pathDraw) {
                path.moveTo(j - 1, height());
            }
            for (; i <= width() && i < m_drawOutPoint; j++) {
                i = j * increment;
                int idx = ceil((startPos + i) * indicesPrPixel);
                idx += idx % m_channels;
                i -= offset;
                if (idx + m_channels >= m_audioLevels.length() || idx < 0) {
                    break;
                }
                level = m_audioLevels.at(idx) / 255.;
                for (int k = 1; k < m_channels; k++) {
                    level = qMax(level, m_audioLevels.at(idx + k) / 255.);
                }
                if (pathDraw) {
                    path.lineTo(i, height() - level * height());
                } else {
                    painter->drawLine(i, h, i, h - (h * level));
                }
            }
            if (pathDraw) {
                path.lineTo(i, height());
                painter->drawPath(path);
            }
        } else {
            double channelHeight = (double)height() / m_channels;
            // Draw separate channels
            double i = 0;
            double level;
            QRectF bgRect(0, 0, width(), channelHeight);
            // Path for vector drawing
            //qDebug()<<"==== DRAWING FROM: "<<m_drawInPoint<<" - "<<m_drawOutPoint<<", FIRST: "<<m_firstChunk;
            for (int channel = 0; channel < m_channels; channel++) {
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
                pen.setWidth(ceil(increment));
                painter->setPen(pathDraw ? Qt::NoPen : pen);
                painter->setOpacity(1);
                i = 0;
                int j = 0;
                if (m_drawInPoint > 0) {
                    j = m_drawInPoint / increment;
                }
                if (pathDraw) {
                    path.moveTo(m_drawInPoint - 1, y);
                }
                for (; i <= width() && i < m_drawOutPoint; j++) {
                    i = j * increment;
                    int idx = ceil((startPos + i) * indicesPrPixel);
                    idx += idx % m_channels;
                    i -= offset;
                    idx += channel;
                    if (idx >= m_audioLevels.length() || idx < 0) break;
                    if (pathDraw) {
                        level = m_audioLevels.at(idx) * channelHeight / 510.;
                        path.lineTo(i, y - level);
                    } else {
                        level = m_audioLevels.at(idx) * channelHeight / 510.; // divide height by 510 (2*255) to get height
                        painter->drawLine(i, y - level, i, y + level);
                    }
                }
                if (pathDraw) {
                    path.lineTo(i, y);
                    painter->drawPath(path);
                    QTransform tr(1, 0, 0, -1, 0, 2 * y);
                    painter->drawPath(tr.map(path));
                }
                if (m_firstChunk && m_channels > 1 && m_channels < 7) {
                    painter->drawText(2, y + channelHeight / 2, chanelNames[channel]);
                }
            }
        }
    }

signals:
    void levelsChanged();
    void propertyChanged();
    void inPointChanged();
    void showItemChanged();
    void audioChannelsChanged();

private:
    QVector<uint8_t> m_audioLevels;
    int m_inPoint;
    int m_outPoint;
    // Pixels outside the view, can be dropped
    int m_drawInPoint;
    int m_drawOutPoint;
    QString m_binId;
    QColor m_color;
    QColor m_color2;
    bool m_format;
    bool m_showItem;
    int m_channels;
    int m_precisionFactor;
    int m_stream;
    bool m_firstChunk;
};

void registerTimelineItems()
{
    qmlRegisterType<TimelineTriangle>("Kdenlive.Controls", 1, 0, "TimelineTriangle");
    qmlRegisterType<TimelinePlayhead>("Kdenlive.Controls", 1, 0, "TimelinePlayhead");
    qmlRegisterType<TimelineWaveform>("Kdenlive.Controls", 1, 0, "TimelineWaveform");
}

#include "timelineitems.moc"
