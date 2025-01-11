/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include <QtQuick/QQuickPaintedItem>

class TimelineWaveform : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor bgColorEven MEMBER m_bgColorEven NOTIFY needRedraw)
    Q_PROPERTY(QColor bgColorOdd MEMBER m_bgColorOdd NOTIFY needRedraw)
    Q_PROPERTY(QColor fgColorEven MEMBER m_fgColorEven NOTIFY needRedraw)
    Q_PROPERTY(QColor fgColorOdd MEMBER m_fgColorOdd NOTIFY needRedraw)
    Q_PROPERTY(int channels MEMBER m_channels NOTIFY needRecompute)
    Q_PROPERTY(QString binId MEMBER m_binId NOTIFY needRecompute)
    Q_PROPERTY(double waveInPoint MEMBER m_inPoint NOTIFY needRecompute)
    Q_PROPERTY(double waveOutPoint MEMBER m_outPoint NOTIFY needRecompute)
    Q_PROPERTY(int audioStream MEMBER m_stream NOTIFY needRecompute)
    Q_PROPERTY(double scaleFactor MEMBER m_scale NOTIFY needRecompute)
    Q_PROPERTY(double speed MEMBER m_speed NOTIFY needRecompute)
    Q_PROPERTY(bool format MEMBER m_separateChannels NOTIFY needRecompute)
    Q_PROPERTY(bool normalize MEMBER m_normalize NOTIFY normalizeChanged)
    Q_PROPERTY(bool drawChannelNames MEMBER m_drawChannelNames NOTIFY needRedraw)
    Q_PROPERTY(bool isOpaque MEMBER m_opaquePaint)

public:
    TimelineWaveform(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

Q_SIGNALS:
    void needRecompute();
    void needRedraw();
    void normalizeChanged();

private:
    QVector<int16_t> m_audioLevels;
    double m_inPoint{0};
    double m_outPoint{0};
    QString m_binId;
    QColor m_bgColorEven;
    QColor m_bgColorOdd;
    QColor m_fgColorEven;
    QColor m_fgColorOdd;
    bool m_separateChannels{true};
    bool m_normalize{false};
    int m_channels{1};
    int m_stream{0};
    double m_scale{1};
    double m_speed{1};
    double m_normalizeFactor = 1.0;
    bool m_opaquePaint{false};
    bool m_needRecompute{true};
    bool m_drawChannelNames{false};
    double m_pointsPerPixel{1};

    void drawWaveformLines(QPainter *painter, int ch, int channels, qreal yMiddle, qreal channelHeight);
    void drawWaveformPath(QPainter *painter, int ch, int channels, qreal yMiddle, qreal channelHeight);
    void compute();
};
