/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef MONITORAUDIOLEVEL_H
#define MONITORAUDIOLEVEL_H

#include "scopewidget.h"
#include <QWidget>
#include <memory>

namespace Mlt {
class Filter;
} // namespace Mlt

class MonitorAudioLevel : public ScopeWidget
{
    Q_OBJECT
public:
    explicit MonitorAudioLevel(int height, QWidget *parent = nullptr);
    ~MonitorAudioLevel() override;
    void refreshPixmap();
    int audioChannels;
    bool isValid;
    void setVisibility(bool enable);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<Mlt::Filter> m_filter;
    int m_height;
    QPixmap m_pixmap;
    QVector<int> m_peaks;
    QVector<int> m_values;
    int m_channelHeight;
    int m_channelDistance;
    int m_channelFillHeight;
    void drawBackground(int channels = 2);
    void refreshScope(const QSize &size, bool full) override;

public slots:
    void setAudioValues(const QVector<int> &values);
};

#endif
