/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QCheckBox>

#include "definitions.h"

class QScrollArea;
class QToolButton;

class ElidedCheckBox : public QCheckBox
{
    Q_OBJECT

public:
    explicit ElidedCheckBox(QWidget *parent = nullptr);
    void setBoxText(const QString &text);
    void updateText(int width);
    int currentWidth() const;

private:
    QString m_text;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

class EffectSettings : public QWidget
{
    Q_OBJECT

public:
    explicit EffectSettings(QWidget *parent = nullptr);

    ElidedCheckBox *checkAll;
    QScrollArea *container;
    void updatePalette();
    void setLabel(const QString &label, const QString &tooltip = QString());
    void updateCheckState(int state);
    QToolButton *effectCompare;

protected:
    void resizeEvent(QResizeEvent *event) override;
};
