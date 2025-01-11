/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include <QtQuick/QQuickPaintedItem>

class TimelinePlayhead : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor MEMBER m_color NOTIFY colorChanged)
    QML_ELEMENT

public:
    TimelinePlayhead(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
Q_SIGNALS:
    void colorChanged(const QColor &);

private:
    QColor m_color;
};