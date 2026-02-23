/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDoubleSpinBox>
#include <QEvent>
#include <QPalette>
#include <QSpinBox>

class StyledDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit StyledDoubleSpinBox(double neutral, QWidget *parent = nullptr);
    bool event(QEvent *event) override;
    void setNeutralPosition(double neutral);

private:
    double m_neutral;
    void updateStyle(double value);
};

class StyledSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    explicit StyledSpinBox(int neutral, QWidget *parent = nullptr);
    bool event(QEvent *event) override;
    void setNeutralPosition(int neutral);

private:
    int m_neutral;
    void updateStyle(int value);
};