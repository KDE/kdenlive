/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef SPLASH_H
#define SPLASH_H

#include <QSplashScreen>
#include <QStyleOptionProgressBar>

class Splash : public QSplashScreen
{
    Q_OBJECT

public:
    explicit Splash(const QPixmap &pixmap);
    //~Splash();

public slots:
    void showProgressMessage(const QString &message, int progress = 0, int max = -1);

private:
    int m_progress;
    QStyleOptionProgressBar m_pbStyle;

protected:
    void drawContents(QPainter *painter) override;
};

#endif
