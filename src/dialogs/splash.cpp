/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "splash.hpp"
#include <QStyle>

Splash::Splash(const QPixmap &pixmap)
    : QSplashScreen(pixmap)
    , m_progress(0)
{
    // Set style for progressbar...
    m_pbStyle.initFrom(this);
    m_pbStyle.state = QStyle::State_Enabled;
    m_pbStyle.textVisible = false;
    m_pbStyle.minimum = 0;
    m_pbStyle.maximum = 100;
    m_pbStyle.progress = 0;
    m_pbStyle.invertedAppearance = false;
    m_pbStyle.rect = QRect(4, pixmap.height() - 24, pixmap.width() / 2, 20); // Where is it.
}


void Splash::showProgressMessage(const QString &message, int progress, int max)
{
    if (max > -1) {
        m_pbStyle.maximum = max;
    }
    if (progress > 0) {
        m_progress++;
    }
    if (!message.isEmpty()) {
        showMessage(message, Qt::AlignRight | Qt::AlignBottom, Qt::white);
    }
    repaint();
}

void Splash::drawContents(QPainter *painter)
{
  QSplashScreen::drawContents(painter);

  if (m_progress > 0) {
    // Set style for progressbar and draw it
    m_pbStyle.progress = m_progress;
    // Draw it...
    style()->drawControl(QStyle::CE_ProgressBar, &m_pbStyle, painter, this);
  }
}

