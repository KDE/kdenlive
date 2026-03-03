/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QQuickWidget>
#include <QWidget>

class EffectStackModel;

class DopeWidget : public QQuickWidget
{
public:
    DopeWidget(QWidget *parent = nullptr);
    void deleteItem();
    void doKeyPressEvent(QKeyEvent *ev);

public Q_SLOTS:
    void registerDopeStack(std::shared_ptr<EffectStackModel> model);
};
