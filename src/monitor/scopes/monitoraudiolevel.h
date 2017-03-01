/*
Copyright (C) 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MONITORAUDIOLEVEL_H
#define MONITORAUDIOLEVEL_H

#include "scopewidget.h"
#include <QWidget>

namespace Mlt
{
class Profile;
class Filter;
}

class MonitorAudioLevel : public ScopeWidget
{
    Q_OBJECT
public:
    explicit MonitorAudioLevel(Mlt::Profile *profile, int height, QWidget *parent = nullptr);
    virtual ~MonitorAudioLevel();
    void refreshPixmap();
    int audioChannels;
    bool isValid;
    void setVisibility(bool enable);

protected:
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

private:
    Mlt::Filter *m_filter;
    int m_height;
    QPixmap m_pixmap;
    QVector <int> m_peaks;
    QVector <int> m_values;
    int m_channelHeight;
    int m_channelDistance;
    int m_channelFillHeight;
    void drawBackground(int channels = 2);
    void refreshScope(const QSize &size, bool full) Q_DECL_OVERRIDE;

private slots:
    void setAudioValues(const QVector <int> &values);
};

#endif
