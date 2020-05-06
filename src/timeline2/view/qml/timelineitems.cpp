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
    Q_PROPERTY(QColor fillColor MEMBER m_color NOTIFY propertyChanged)
    Q_PROPERTY(int waveInPoint MEMBER m_inPoint NOTIFY propertyChanged)
    Q_PROPERTY(int channels MEMBER m_channels NOTIFY audioChannelsChanged)
    Q_PROPERTY(QString binId MEMBER m_binId NOTIFY levelsChanged)
    Q_PROPERTY(int waveOutPoint MEMBER m_outPoint)
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
        //setRenderTarget(QQuickPaintedItem::FramebufferObject);
        //setMipmap(true);
        setTextureSize(QSize(1, 1));
        connect(this, &TimelineWaveform::levelsChanged, [&]() {
            if (!m_binId.isEmpty() && m_audioLevels.isEmpty() && m_stream >= 0) {
                m_audioLevels = pCore->projectItemModel()->getAudioLevelsByBinID(m_binId, m_stream);
                update();
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
        qreal indicesPrPixel = qreal(m_outPoint - m_inPoint) / width();
        QPen pen = painter->pen();
        pen.setColor(m_color);
        pen.setWidthF(0);
        painter->setBrush(m_color);
        if (!KdenliveSettings::displayallchannels()) {
            // Draw merged channels
            QPainterPath path;
            path.moveTo(-1, height());
            double i = 0;
            double increment = qMax(1., 1 / qAbs(indicesPrPixel));
            double level;
            int lastIdx = -1;
            for (; i <= width(); i += increment) {
                int idx = m_inPoint + int(i * indicesPrPixel);
                if (idx + m_channels >= m_audioLevels.length() || idx < 0) {
                    break;
                }
                if (lastIdx == idx) {
                    continue;
                }
                lastIdx = idx;
                level = m_audioLevels.at(idx) / 255.;
                for (int j = 1; j < m_channels; j++) {
                    level = qMax(level, m_audioLevels.at(idx + j) / 255.);
                }
                path.lineTo(i, height() - level * height());
            }
            path.lineTo(i, height());
            painter->drawPath(path);
        } else {
            double channelHeight = height() / (2 * m_channels);
            QFont font = painter->font();
            font.setPixelSize(channelHeight - 1);
            painter->setFont(font);
            // Draw separate channels
            double i = 0;
            double increment = qMax(1., 1 / indicesPrPixel);
            double level;
            QRectF bgRect(0, 0, width(), 2 * channelHeight);
            QVector<QPainterPath> channelPaths(m_channels);
            for (int channel = 0; channel < m_channels; channel++) {
                double y = height() - (2 * channel * channelHeight) - channelHeight;
                channelPaths[channel].moveTo(-1, y);
                painter->setOpacity(0.2);
                if (channel % 2 == 0) {
                    // Add dark background on odd channels
                    bgRect.moveTo(0, y - channelHeight);
                    painter->fillRect(bgRect, Qt::black);
                }
                // Draw channel median line
                painter->setPen(pen);
                painter->drawLine(QLineF(0., y, width(), y));
                painter->setOpacity(1);
                int lastIdx = -1;
                for (i = 0; i <= width(); i += increment) {
                    int idx = m_inPoint + ceil(i * indicesPrPixel);
                    if (lastIdx == idx) {
                        continue;
                    }
                    lastIdx = idx;
                    idx += channel;
                    if (idx >= m_audioLevels.length() || idx < 0) break;
                    level = m_audioLevels.at(idx) * channelHeight / 255.;
                    channelPaths[channel].lineTo(i, y - level);
                }
                if (m_firstChunk && m_channels > 1 && m_channels < 7) {
                    painter->drawText(2, y + channelHeight, chanelNames[channel]);
                }
                channelPaths[channel].lineTo(i, y);
                painter->setPen(Qt::NoPen);
                painter->drawPath(channelPaths.value(channel));
                QTransform tr(1, 0, 0, -1, 0, 2 * y);
                painter->drawPath(tr.map(channelPaths.value(channel)));
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
    QString m_binId;
    QColor m_color;
    bool m_format;
    bool m_showItem;
    int m_channels;
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
