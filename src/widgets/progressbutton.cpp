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

#include <QAction>
#include <QPainter>
#include <klocalizedstring.h>

ProgressButton::ProgressButton(const QString &text, double max, QWidget *parent)
    : QToolButton(parent)
    , m_defaultAction(nullptr)
    , m_max(max)
{
    setPopupMode(MenuButtonPopup);
    m_progress = width() - 6;
    m_iconSize = height() - 4;
    m_progressFont = font();
    m_progressFont.setPixelSize(m_iconSize / 2);
    initStyleOption(&m_buttonStyle);
    QPixmap pix(1, 1);
    pix.fill(Qt::transparent);
    m_dummyAction = new QAction(QIcon(pix), text, this);
}

ProgressButton::~ProgressButton()
{
    delete m_dummyAction;
}

void ProgressButton::defineDefaultAction(QAction *action, QAction *actionInProgress)
{
    setDefaultAction(action);
    m_defaultAction = action;
    if (actionInProgress) {
        connect(m_dummyAction, &QAction::triggered, actionInProgress, &QAction::trigger);
    }
}

void ProgressButton::setProgress(int progress)
{
    int prog = m_iconSize * progress / m_max;
    QString remaining;
    if (m_timer.isValid() && progress > 0) {
        // calculate remaining time
        qint64 ms = m_timer.elapsed() * (m_max - progress) / progress;
        if (ms < 60000)
        // xgettext:no-c-format
        {
            remaining = i18nc("s as seconds", "%1s", ms / 1000);
        } else if (ms < 3600000)
        // xgettext:no-c-format
        {
            remaining = i18nc("m as minutes", "%1m", ms / 60000);
        } else {
            remaining = i18nc("h as hours", "%1h", qMin(99, (int)(ms / 3600000)));
        }
    }
    if (progress < 0) {
        setDefaultAction(m_defaultAction);
        m_remainingTime.clear();
        m_timer.invalidate();
        m_progress = -1;
        update();
        return;
    }
    if (!m_timer.isValid() || progress == 0) {
        setDefaultAction(m_dummyAction);
        m_timer.start();
    }
    if (progress == m_max) {
        setDefaultAction(m_defaultAction);
        m_remainingTime.clear();
        m_timer.invalidate();
    }
    if (remaining != m_remainingTime || m_progress != prog) {
        m_progress = prog;
        m_remainingTime = remaining;
        update();
    } else {
        m_progress = prog;
        m_remainingTime = remaining;
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
        QRect rect(3, (height() - m_iconSize) / 2, m_iconSize, m_iconSize);
        // draw remaining time
        if (m_remainingTime.isEmpty() && m_progress >= 0) {
            // We just started task, no time estimation yet, display starting status
            if (m_defaultAction) {
                painter.drawPixmap(rect.adjusted(4, 0, -4, -8), m_defaultAction->icon().pixmap(m_iconSize - 8, m_iconSize - 8));
            }
        } else {
            painter.drawText(rect, Qt::AlignHCenter, m_remainingTime);
        }
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
