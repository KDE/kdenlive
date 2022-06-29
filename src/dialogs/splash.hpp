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

public slots:
    void showProgressMessage(const QString &message, int progress = 0, int max = -1);

private:
    int m_progress;
    QPixmap m_pixmap;
    QStyleOptionProgressBar m_pbStyle;

protected:
    void drawContents(QPainter *painter) override;
};
