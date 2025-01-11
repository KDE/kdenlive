/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include <QtQuick/QQuickPaintedItem>

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
    TimelineRecWaveform(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

Q_SIGNALS:
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
