/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QPixmap>
#include <QSplashScreen>
#include <QStyleOptionProgressBar>

class Splash : public QSplashScreen
{
    Q_OBJECT

public:
    explicit Splash();
    //~Splash();

public Q_SLOTS:
    void showProgressMessage(const QString &message, int max = -1);
    void increaseProgressMessage();

private:
    int m_progress;
    QPixmap m_pixmap;
    QStyleOptionProgressBar m_pbStyle;

protected:
    void drawContents(QPainter *painter) override;
};
