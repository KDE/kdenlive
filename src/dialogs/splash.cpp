/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "splash.hpp"
#include <QStyle>
#include <QPainter>
#include <KLocalizedString>
#include <QDebug>

Splash::Splash()
    : QSplashScreen()
    , m_progress(0)
{
    QPixmap pixmap(":/pics/splash-background.png");

    // Set style for progressbar...
    m_pbStyle.initFrom(this);
    m_pbStyle.state = QStyle::State_Enabled;
    m_pbStyle.textVisible = false;
    m_pbStyle.minimum = 0;
    m_pbStyle.maximum = 100;
    m_pbStyle.progress = 0;
    m_pbStyle.invertedAppearance = false;
    m_pbStyle.rect = QRect(4, pixmap.height() - 24, pixmap.width() / 2, 20); // Where is it.

    // Add KDE branding to pixmap
    QPainter *paint = new QPainter(&pixmap);
    paint->setPen(Qt::white);
    QPixmap kde(":/pics/kde-logo.png");
    const int logoSize = 32;
    QPoint pos(12, 12);
    paint->drawPixmap(pos.x(), pos.y(), logoSize, logoSize, kde);
    paint->drawText(pos.x() + logoSize, pos.y() + (logoSize / 2) + paint->fontMetrics().strikeOutPos(), i18n("Made by KDE"));
    setPixmap(pixmap);
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

