/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "audiomixer/audiolevels/audiolevelwidget.hpp"
#include "scopewidget.h"
#include <QWidget>
#include <memory>

class MonitorAudioLevel : public ScopeWidget
{
    Q_OBJECT
public:
    explicit MonitorAudioLevel(QWidget *parent = nullptr);
    ~MonitorAudioLevel() override;
    void refreshPixmap();
    int audioChannels;
    bool isValid;
    void setVisibility(bool enable);
    QSize sizeHint() const override;

private:
    std::unique_ptr<AudioLevelWidget> m_audioLevelWidget;
    void refreshScope(const QSize &size, bool full) override;

public Q_SLOTS:
    void setAudioValues(const QVector<double> &values);

Q_SIGNALS:
    void audioLevelsAvailable(const QVector<double>& levels);
};
