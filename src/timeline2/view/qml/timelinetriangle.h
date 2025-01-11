/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <QtQuick/QQuickPaintedItem>

class TimelineTriangle : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor MEMBER m_color)
    Q_PROPERTY(bool endFade MEMBER m_endFade)
    Q_PROPERTY(int curveType MEMBER m_curveType NOTIFY curveChanged)

public:
    TimelineTriangle(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

private:
    QColor m_color;
    int m_curveType{0};
    bool m_endFade{false};

Q_SIGNALS:
    void curveChanged();
};
