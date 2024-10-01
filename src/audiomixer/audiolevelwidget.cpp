/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiolevelwidget.hpp"
#include "core.h"
#include "iecscale.h"
#include "mlt++/Mlt.h"
#include "profiles/profilemodel.hpp"

#include <KLocalizedString>
#include <QFont>
#include <QFontDatabase>
#include <QPaintEvent>
#include <QPainter>
#include <QToolTip>

AudioLevelWidget::AudioLevelWidget(int width, int sliderHandle, QWidget *parent)
    : QWidget(parent)
    , audioChannels(pCore->audioChannels())
    , m_width(width)
    , m_offset(fontMetrics().boundingRect(QStringLiteral("-45")).width() + 4)
    , m_channelWidth(width / 2)
    , m_channelDistance(2)
    , m_channelFillWidth(m_channelWidth)
    , m_displayToolTip(false)
    , m_sliderHandle(sliderHandle)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    QFont ft(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    ft.setPointSizeF(ft.pointSize() * 0.8);
    setFont(ft);
    setMinimumWidth(4);
}

AudioLevelWidget::~AudioLevelWidget() = default;

void AudioLevelWidget::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_displayToolTip = true;
    updateToolTip();
}

void AudioLevelWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_displayToolTip = false;
}

void AudioLevelWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    drawBackground(m_peaks.size());
}

void AudioLevelWidget::refreshPixmap()
{
    drawBackground(m_peaks.size());
}

void AudioLevelWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange) {
        drawBackground(m_peaks.size());
    }
    QWidget::changeEvent(event);
}

void AudioLevelWidget::drawBackground(int channels)
{
    if (height() == 0 || channels == 0) {
        return;
    }
    QSize newSize = QWidget::size();
    if (!newSize.isValid()) {
        return;
    }
    qreal scalingFactor = devicePixelRatioF();
    m_pixmap = QPixmap(newSize * scalingFactor);
    m_pixmap.setDevicePixelRatio(scalingFactor);
    if (m_pixmap.isNull()) {
        return;
    }
    // Channel labels are vertical along the left.
    const QVector<int> dbscale = {0, -6, -12, -18, -24, -30, -36, -42, -48, -54};
    m_maxDb = dbscale.first();
    int dbLabelCount = dbscale.size();

    // Ensure width is a multiple of channels count
    int maxChannelsWidth = newSize.width() - 2 * m_offset - 4;
    // Calculate channels width
    m_channelDistance = channels < 2 ? 0 : 2;
    maxChannelsWidth -= m_channelDistance * (channels - 1);
    m_channelWidth = maxChannelsWidth / channels;
    if (m_channelWidth < 4) {
        m_channelDistance = 1;
        maxChannelsWidth += (channels - 1);
        m_channelWidth = maxChannelsWidth / channels;
    }
    m_channelWidth = qMax(m_channelWidth, 1);
    int newWidth = channels * m_channelWidth + m_channelDistance * (channels - 1) + (m_channelDistance == 1 ? 3 : 4);
    newSize.setWidth(newWidth);
    QLinearGradient gradient(0, newSize.height() - 4, 0, 0);
    double gradientVal = 0.;
    gradient.setColorAt(gradientVal, Qt::darkGreen);
    gradientVal = IEC_ScaleMax(-12, m_maxDb);
    gradient.setColorAt(gradientVal, Qt::green); // -12db, green
    gradientVal = IEC_ScaleMax(-8, m_maxDb);
    gradient.setColorAt(gradientVal, Qt::yellow); // -8db, yellow
    gradientVal = IEC_ScaleMax(-5, m_maxDb);
    gradient.setColorAt(gradientVal, QColor(255, 200, 20)); // -5db, orange
    gradient.setColorAt(1., Qt::red);                       // 0db, red
    m_pixmap.fill(Qt::transparent);
    QRect rect(m_offset, 1, newWidth - 2, newSize.height() - 2);
    QPainter p(&m_pixmap);
    p.setOpacity(isEnabled() ? 0.8 : 0.4);
    p.setFont(font());
    QPen pen = p.pen();
    pen.setColor(palette().dark().color());
    pen.setWidth(2);
    p.setPen(pen);
    p.fillRect(QRect(m_offset, 2, newWidth - 2, newSize.height() - 4), QBrush(gradient));
    p.setOpacity(1);
    p.drawRect(rect);

    // dB scale is vertical along the bottom
    int labelHeight = fontMetrics().ascent();

    int prevY = -1;
    int y = 0;
    if (newSize.height() > 2 * labelHeight) {
        for (int i = 0; i < dbLabelCount; i++) {
            int value = dbscale.at(i);
            y = newSize.height() - 2 - qRound(IEC_ScaleMax(value, m_maxDb) * ((double)newSize.height() - 4));
            p.setOpacity(0.6);
            p.setPen(palette().window().color().rgb());
            p.drawLine(m_offset + 1, y, m_offset + newWidth - 4, y);
            y -= qRound(labelHeight / 2.0);
            if (y < 0) {
                y = 0;
            }
            if (prevY < 0 || y - prevY > 2) {
                const QString label = QString::asprintf("%d", value);
                p.setOpacity(0.8);
                p.setPen(palette().text().color().rgb());
                p.drawText(QRectF(0, y, m_offset - 5, labelHeight), label, QTextOption(Qt::AlignRight));
                prevY = y + labelHeight;
            }
        }
        prevY = -1;
        const QVector<int> gains = {-24, -10, -4, 0, 4, 10, 24};
        p.setOpacity(0.8);
        p.setPen(palette().text().color().rgb());
        int sliderHeight = newSize.height() - m_sliderHandle;
        for (int i = 0; i < gains.count(); i++) {
            y = newSize.height() - m_sliderHandle / 2 - (fromDB(gains.at(i)) * sliderHeight / 100.);
            y -= qRound(labelHeight / 2.0);
            y = qBound(0, y, newSize.height() - labelHeight);
            if (prevY < 0 || prevY - y > 2) {
                const QString label = QString::asprintf("%d", gains.at(i));
                p.drawText(QRectF(m_offset + newWidth, y, (m_pixmap.width() / scalingFactor) - (m_offset + newWidth) - 2, labelHeight), label,
                           QTextOption(Qt::AlignRight));
                prevY = qMax(0, y - labelHeight);
            }
        }
    }

    p.setOpacity(isEnabled() ? 1 : 0.5);
    pen = p.pen();
    pen.setColor(palette().dark().color());
    pen.setWidth(2);
    p.setPen(pen);
    if (m_channelWidth < 4) {
        // too many audio channels, simple line between channels
        m_channelFillWidth = m_channelWidth;
        pen.setWidth(1);
        p.setPen(pen);
        // p.drawRect(m_offset, 0, channels * (m_channelWidth + m_channelDistance) - 2, rect.height() - 1);
        for (int i = 1; i < channels; i++) {
            p.drawLine(m_offset + i * (m_channelWidth + m_channelDistance), 0, m_offset + i * (m_channelWidth + m_channelDistance), rect.height() - 1);
        }
    } else if (channels > 1) {
        m_channelFillWidth = m_channelWidth - 1;
        // p.drawRect(m_offset, 0, channels * (m_channelWidth + m_channelDistance) - 2, rect.height() - 1);
        for (int i = 1; i < channels; i++) {
            int x = m_offset + i * (m_channelWidth + m_channelDistance);
            p.drawLine(x, 2, x, rect.height() - 1);
        }
    } else {
        m_channelDistance = 0;
        m_channelFillWidth = m_channelWidth;
        // p.drawRect(m_offset, 0, m_channelWidth - 1, rect.height() - 1);
    }
    p.end();
}

// cppcheck-suppress unusedFunction
void AudioLevelWidget::setAudioValues(const QVector<double> &values)
{
    m_values = values;
    if (m_peaks.size() != m_values.size()) {
        m_peaks = values;
        drawBackground(values.size());
    } else {
        for (int i = 0; i < m_values.size(); i++) {
            m_peaks[i] -= .2;
            if (m_values.at(i) > m_peaks.at(i)) {
                m_peaks[i] = m_values.at(i);
            }
        }
    }
    update();
}

void AudioLevelWidget::setVisibility(bool enable)
{
    if (enable) {
        setVisible(true);
        setFixedWidth(m_width);
    } else {
        // set height to 0 so the toolbar layout is not affected
        setFixedHeight(0);
        setVisible(false);
    }
}

void AudioLevelWidget::paintEvent(QPaintEvent * /*pe*/)
{
    if (!isVisible()) {
        return;
    }
    QPainter p(this);
    // p.setClipRect(pe->rect());
    QRect rect(0, 0, width(), height());
    if (m_values.isEmpty()) {
        p.setOpacity(0.3);
        p.drawPixmap(0, 0, m_pixmap);
        return;
    }
    p.drawPixmap(0, 0, m_pixmap);
    p.setOpacity(0.9);
    int levelsHeight = height() - 4;
    for (int i = 0; i < m_values.count(); i++) {
        if (m_values.at(i) >= 100) {
            continue;
        }
        int val = IEC_ScaleMax(m_values.at(i), m_maxDb) * levelsHeight;
        int peak = IEC_ScaleMax(m_peaks.at(i), m_maxDb) * levelsHeight;
        int xPos = m_offset + 1 + i * (m_channelWidth + m_channelDistance);
        p.fillRect(xPos, 2, m_channelFillWidth + (m_channelDistance == 2 ? 1 : 0), levelsHeight - val, palette().window());
        p.fillRect(xPos, 2 + levelsHeight - peak, m_channelFillWidth + (m_channelDistance == 2 ? 1 : 0), 1, palette().text());
    }
    if (m_displayToolTip) {
        updateToolTip();
    }
}

void AudioLevelWidget::updateToolTip()
{
    QString tip;
    int channels = m_values.count();
    for (int i = 0; i < channels; i++) {
        if (m_values.at(i) >= 100) {
            tip.append(QStringLiteral("-100dB"));
        } else {
            tip.append(QString::number(m_values.at(i), 'f', 2) + QStringLiteral("dB"));
        }
        if (channels == 2 && i == 0) {
            tip.prepend(i18nc("L as in Left", "L:"));
            tip.append(i18nc("R as in Right", "\nR:"));
        }
    }
    QToolTip::showText(QCursor::pos(), tip, this);
}
