/*
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"

#include <QColor>
#include <QVector>

static constexpr int GradientMaxStops = 32;
static constexpr qreal GradientMinDistance = 0.01;

struct GradientData
{
    QVector<QPair<qreal, QColor>> stops;

    void enforceMinStops();
    void sortStops();

    // Returns index of stop hit within tolerance, or -1
    int hitTest(const QVector<QPair<qreal, QColor>> &s, qreal pos, qreal tolerance) const;

    // Interpolate color at position from current stops
    QColor interpolateColor(qreal pos) const;

    // Add a stop at pos with interpolated color; returns new index or -1 if at max
    int addStop(qreal pos);

    // Remove stop at index; enforces minimum 2 stops
    bool removeStop(int index);

    QString toString() const;
    void fromString(const QString &str);
};

class GradientEditWidget : public AbstractParamWidget
{
    Q_OBJECT

public:
    explicit GradientEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);
    void slotRefresh() override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    GradientData m_data;
    int m_dragIndex{-1};
    int m_selectedIndex{-1};

    static constexpr int HandleRadius = 7;
    static constexpr int BarHeight = 24;

    int hitTest(int x) const;
    qreal xToPos(int x) const;
    int posToX(qreal pos) const;
};
