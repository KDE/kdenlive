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
#include <QStyleOptionProgressBar>

Splash::Splash(const QPixmap &pixmap)
    : QSplashScreen(pixmap)
    , m_progress(0)
{
}


void Splash::showProgressMessage(const QString &message, int progress)
{
    m_progress = qBound(0, progress, 100);
    if (!message.isEmpty()) {
        showMessage(message, Qt::AlignRight | Qt::AlignBottom, Qt::white);
    }
    repaint();
}

void Splash::drawContents(QPainter *painter)
{
  QSplashScreen::drawContents(painter);

  // Set style for progressbar...
  QStyleOptionProgressBar pbstyle;
  pbstyle.initFrom(this);
  pbstyle.state = QStyle::State_Enabled;
  pbstyle.textVisible = false;
  pbstyle.minimum = 0;
  pbstyle.maximum = 100;
  pbstyle.progress = m_progress;
  pbstyle.invertedAppearance = false;
  pbstyle.rect = QRect(4, height() - 24, width() / 2, 20); // Where is it.

  // Draw it...
  style()->drawControl(QStyle::CE_ProgressBar, &pbstyle, painter, this);
}

