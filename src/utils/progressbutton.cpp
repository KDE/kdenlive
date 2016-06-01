/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#include "progressbutton.h"

#include <klocalizedstring.h>
#include <QAction>
#include <QPainter>

ProgressButton::ProgressButton(const QString text, QWidget *parent) : QToolButton(parent)
    , m_defaultAction(NULL)
{
    setPopupMode(MenuButtonPopup);
    m_progress = width() - 6;
    QPixmap pix(1,1);
    pix.fill(Qt::transparent);
    m_dummyAction = new QAction(QIcon(pix), text, this);
    initStyleOption(&m_buttonStyle);
    m_iconSize = height() - 4;
    m_progressFont = font();
    m_progressFont.setPixelSize(m_iconSize / 2);
}

ProgressButton::~ProgressButton()
{
    delete m_dummyAction;
}

void ProgressButton::setProgress(int progress) 
{
    int prog = (m_iconSize) * (double) progress / 1000;
    QString remaining;
    if (m_timer.isValid() && progress > 0) {
        // calculate remaining time
        qint64 ms = m_timer.elapsed() * (1000.0 / progress - 1);
        if (ms < 60000)
            // xgettext:no-c-format
            remaining = i18nc("s as seconds", "%1s", ms / 1000);
        else if (ms < 3600000)
            // xgettext:no-c-format
            remaining = i18nc("m as minutes", "%1m", ms / 60000);
        else {
            remaining = i18nc("h as hours", "%1h", qMin(99, (int) (ms / 3600000)));
        }
    }
    if (progress < 0) {
        if (m_defaultAction)
            setDefaultAction(m_defaultAction);
        m_remainingTime.clear();
        m_timer.invalidate();
        m_progress = -1;
        update();
        return;
    }
    if (!m_timer.isValid() || progress == 0) {
        if (!m_defaultAction) {
            m_defaultAction = defaultAction();
        }
        setDefaultAction(m_dummyAction);
        m_timer.start();
    }
    if (progress == 1000) {
        if (m_defaultAction)
            setDefaultAction(m_defaultAction);
        m_remainingTime.clear();
        m_timer.invalidate();
    }
    m_progress = prog;
    if (remaining != m_remainingTime) {
        m_remainingTime = remaining;
        update();
    }
}

int ProgressButton::progress() const
{
    return m_progress;
}

void ProgressButton::paintEvent(QPaintEvent *event)
{
    QToolButton::paintEvent(event);
    if (m_progress < m_iconSize) {
        QPainter painter(this);
        painter.setFont(m_progressFont);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QRectF rect(3, (height() - m_iconSize) / 2, m_iconSize, m_iconSize);
        // draw remaining time
        painter.drawText(rect, Qt::AlignHCenter, m_remainingTime);
        if (m_progress < 0) {
            painter.fillRect(rect.x(), rect.bottom() - 5, rect.width(), 3, Qt::red);
        } else {
            QColor w(Qt::white);
            w.setAlpha(40);
            painter.fillRect(rect.x(), rect.bottom() - 6, m_progress, 4, palette().highlight().color());
            painter.fillRect(rect.x(), rect.bottom() - 6, rect.width(), 4, w);
        }
        painter.setPen(palette().shadow().color());
        painter.drawRoundedRect(rect.x(), rect.bottom() - 7, rect.width(), 6, 2, 2);
    }
}
