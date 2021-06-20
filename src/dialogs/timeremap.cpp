/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "timeremap.h"

#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "bin/projectclip.h"
#include "project/projectmanager.h"

#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QWheelEvent>
#include <QStylePainter>

#include <KColorScheme>
#include "klocalizedstring.h"

RemapView::RemapView(QWidget *parent)
    : QWidget(parent)
    , m_duration(1)
    , m_zoomFactor(1)
    , m_zoomStart(0)
{
    setMouseTracking(true);
    setMinimumSize(QSize(150, 60));
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    int size = QFontInfo(font()).pixelSize() * 3;
    m_lineHeight = int(size / 2.1);
    setFixedHeight(size * 2);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
}

void RemapView::setDuration(int duration)
{
    m_duration = duration;
    update();
}


void RemapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPalette pal = palette();
    KColorScheme scheme(pal.currentColorGroup(), KColorScheme::Window);
    m_colSelected = palette().highlight().color();
    m_colKeyframe = scheme.foreground(KColorScheme::NormalText).color();
    QStylePainter p(this);
    int maxWidth = width() - 2;
    m_scale = maxWidth / double(qMax(1, m_duration - 1));
    int headOffset = m_lineHeight / 2;
    /*m_zoomStart = m_zoomHandle.x() * maxWidth;
    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    int zoomEnd = qCeil(m_zoomHandle.y() * maxWidth);*/
    /* ticks */
    double fps = pCore->getCurrentFps();
    int displayedLength = int(m_duration / m_zoomFactor / fps);
    double factor = 1;
    if (displayedLength < 2) {
        // 1 frame tick
    } else if (displayedLength < 30 ) {
        // 1 sec tick
        factor = fps;
    } else if (displayedLength < 150) {
        // 5 sec tick
        factor = 5 * fps;
    } else if (displayedLength < 300) {
        // 10 sec tick
        factor = 10 * fps;
    } else if (displayedLength < 900) {
        // 30 sec tick
        factor = 30 * fps;
    } else if (displayedLength < 1800) {
        // 1 min. tick
        factor = 60 * fps;
    } else if (displayedLength < 9000) {
        // 5 min tick
        factor = 300 * fps;
    } else if (displayedLength < 18000) {
        // 10 min tick
        factor = 600 * fps;
    } else {
        // 30 min tick
        factor = 1800 * fps;
    }

    // Position of left border in frames
    double tickOffset = m_zoomStart * m_zoomFactor;
    double frameSize = factor * m_scale * m_zoomFactor;
    int base = int(tickOffset / frameSize);
    tickOffset = frameSize - (tickOffset - (base * frameSize));
    // Draw frame ticks
    int scaledTick = 0;
    for (int i = 0; i < maxWidth / frameSize; i++) {
        scaledTick = int((i * frameSize) + tickOffset);
        if (scaledTick >= maxWidth) {
            break;
        }
        p.drawLine(QPointF(scaledTick , m_lineHeight + 1), QPointF(scaledTick, m_lineHeight - 3));
    }



    p.setPen(palette().dark().color());

    /*
     * Time-"line"
     */
    p.setPen(m_colKeyframe);
    p.drawLine(0, m_lineHeight, width(), m_lineHeight);
    p.drawLine(0, m_lineHeight - headOffset / 2, 0, m_lineHeight + headOffset / 2);
    p.drawLine(width(), m_lineHeight - headOffset / 2, width(), m_lineHeight + headOffset / 2);

    /*
     * current position cursor
     */


    // Zoom bar
    /*p.setPen(Qt::NoPen);
    p.setBrush(palette().mid());
    p.drawRoundedRect(0, m_zoomHeight + 3, width() - 2 * 0, m_size - m_zoomHeight - 3, m_lineHeight / 5, m_lineHeight / 5);
    p.setBrush(palette().highlight());
    p.drawRoundedRect(int((width()) * m_zoomHandle.x()),
                      m_zoomHeight + 3,
                      int((width()) * (m_zoomHandle.y() - m_zoomHandle.x())),
                      m_size - m_zoomHeight - 3,
                      m_lineHeight / 5, m_lineHeight / 5);
                      */
}


TimeRemap::TimeRemap(QWidget *parent)
    : QWidget(parent)
    , m_clip(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);

    m_in = new TimecodeDisplay(pCore->timecode(), this);
    inLayout->addWidget(m_in);
    m_out = new TimecodeDisplay(pCore->timecode(), this);
    outLayout->addWidget(m_out);
    m_view = new RemapView(this);
    remapLayout->addWidget(m_view);
}

void TimeRemap::setClip(std::shared_ptr<ProjectClip> clip, int in, int out)
{
    m_clip = clip;
    if (m_clip != nullptr) {
        int min = in == -1 ? 0 : in;
        int max = out == -1 ? m_clip->getFramePlaytime() : out;
        m_in->setRange(min, max);
        m_out->setRange(min, max);
        m_view->setDuration(max - min);
        setEnabled(true);
    } else {
        setEnabled(false);
    }
}

TimeRemap::~TimeRemap()
{
    //delete m_previewTimer;
}

