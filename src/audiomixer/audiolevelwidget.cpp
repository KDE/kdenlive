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
#include <cmath>

AudioLevelWidget::AudioLevelWidget(int width, QWidget *parent)
    : QWidget(parent)
    , audioChannels(pCore->audioChannels())
    , m_width(width)
    , m_offset(fontMetrics().boundingRect(QStringLiteral("-45")).width() + 5)
    , m_channelWidth(width / 2)
    , m_channelDistance(2)
    , m_channelFillWidth(m_channelWidth)
    , m_displayToolTip(false)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    QFont ft(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    ft.setPointSizeF(ft.pointSize() * 0.8);
    setFont(ft);
    setMinimumWidth(4);
}

AudioLevelWidget::~AudioLevelWidget() = default;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void AudioLevelWidget::enterEvent(QEnterEvent *event)
#else
void AudioLevelWidget::enterEvent(QEvent *event)
#endif
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
    if (height() == 0) {
        return;
    }
    QSize newSize = QWidget::size();
    if (!newSize.isValid()) {
        return;
    }
    m_pixmap = QPixmap(newSize);
    if (m_pixmap.isNull()) {
        return;
    }
    // Channel labels are vertical along the left.
    QVector<int> dbscale;
    dbscale << 0 << -5 << -10 << -15 << -20 << -25 << -30 << -35 << -40 << -50;
    m_maxDb = dbscale.first();
    int dbLabelCount = dbscale.size();

    newSize.setWidth(newSize.width() - m_offset - 1);
    QLinearGradient gradient(0, newSize.height(), 0, 0);
    double gradientVal = 0.;
    gradient.setColorAt(gradientVal, Qt::darkGreen);
    gradientVal = IEC_ScaleMax(-12, m_maxDb);
    gradient.setColorAt(gradientVal - 0.001, Qt::darkGreen);
    gradient.setColorAt(gradientVal, Qt::green); // -12db
    gradientVal = IEC_ScaleMax(-6, m_maxDb);
    gradient.setColorAt(gradientVal - 0.001, Qt::green);
    gradient.setColorAt(gradientVal, Qt::yellow); // -6db
    gradientVal = IEC_ScaleMax(0, m_maxDb);
    gradient.setColorAt(gradientVal - 0.001, Qt::yellow);
    gradient.setColorAt(gradientVal, Qt::red); // 0db
    m_pixmap.fill(Qt::transparent);
    int totalWidth;
    if (channels < 2) {
        m_channelWidth = newSize.width();
        totalWidth = m_channelWidth;
    } else {
        m_channelWidth = (newSize.width() - (channels - 1)) / channels;
        totalWidth = channels * m_channelWidth + (channels - 1);
    }
    QRect rect(m_offset, 0, newSize.width(), newSize.height());
    QPainter p(&m_pixmap);
    p.setOpacity(isEnabled() ? 0.8 : 0.4);
    p.setFont(font());
    p.fillRect(rect, QBrush(gradient));

    // dB scale is vertical along the bottom
    int labelHeight = fontMetrics().ascent();

    int prevY = -1;
    int y = 0;
    for (int i = 0; i < dbLabelCount; i++) {
        int value = dbscale[i];
        QString label = QString::asprintf("%d", value);
        y = newSize.height() - qRound(IEC_ScaleMax(value, m_maxDb) * (double)newSize.height());
        p.setPen(palette().window().color().rgb());
        p.drawLine(m_offset, y, m_offset + totalWidth - 1, y);
        y -= qRound(labelHeight / 2.0);
        if (y < 0) {
            y = 0;
        }
        if (prevY < 0 || y - prevY > 2) {
            p.setPen(palette().text().color().rgb());
            p.drawText(QRectF(0, y, m_offset - 5, labelHeight), label, QTextOption(Qt::AlignRight));
            prevY = y + labelHeight;
        }
    }

    p.setOpacity(isEnabled() ? 1 : 0.5);
    p.setPen(palette().dark().color());
    // Clear space between the channels
    p.setCompositionMode(QPainter::CompositionMode_Source);
    if (m_channelWidth < 4) {
        // too many audio channels, simple line between channels
        m_channelDistance = 1;
        m_channelFillWidth = m_channelWidth;
        for (int i = 0; i < channels; i++) {
            p.drawLine(m_offset + i * (m_channelWidth + m_channelDistance), 0, i * (m_channelWidth + m_channelDistance), rect.height() - 1);
        }
    } else if (channels > 1) {
        m_channelDistance = 2;
        m_channelFillWidth = m_channelWidth - 2;
        for (int i = 0; i < channels; i++) {
            p.drawRect(m_offset + i * (m_channelWidth + m_channelDistance), 0, m_channelWidth - 1, rect.height() - 1);
            if (i > 0) {
                p.fillRect(m_offset + i * (m_channelWidth + m_channelDistance) - 2, 0, 2, rect.height(), Qt::transparent);
            }
        }
    } else {
        m_channelDistance = 0;
        m_channelFillWidth = m_channelWidth;
        p.drawRect(m_offset, 0, m_channelWidth - 1, rect.height() - 1);
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

void AudioLevelWidget::paintEvent(QPaintEvent *pe)
{
    if (!isVisible()) {
        return;
    }
    QPainter p(this);
    p.setClipRect(pe->rect());
    QRect rect(0, 0, width(), height());
    if (m_values.isEmpty()) {
        p.setOpacity(0.2);
        p.drawPixmap(rect, m_pixmap);
        return;
    }
    p.drawPixmap(rect, m_pixmap);
    p.setOpacity(0.9);
    for (int i = 0; i < m_values.count(); i++) {
        if (m_values.at(i) >= 100) {
            continue;
        }
        int val = IEC_ScaleMax(m_values.at(i), m_maxDb) * rect.height();
        int peak = IEC_ScaleMax(m_peaks.at(i), m_maxDb) * rect.height();
        p.fillRect(m_offset + i * (m_channelWidth + m_channelDistance) + 1, 0, m_channelFillWidth, rect.height() - val, palette().window());
        p.fillRect(m_offset + i * (m_channelWidth + m_channelDistance) + 1, rect.height() - peak, m_channelFillWidth, 1, palette().text());
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
