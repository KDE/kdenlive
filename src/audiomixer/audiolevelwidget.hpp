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

#ifndef AUDIOLEVELWIDGET_H
#define AUDIOLEVELWIDGET_H

#include <QWidget>
#include <memory>

namespace Mlt {
class Filter;
} // namespace Mlt

class AudioLevelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioLevelWidget(int width, QWidget *parent = nullptr);
    ~AudioLevelWidget() override;
    void refreshPixmap();
    int audioChannels;
    bool isValid;
    void setVisibility(bool enable);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<Mlt::Filter> m_filter;
    int m_width;
    int m_offset;
    QPixmap m_pixmap;
    QVector<double> m_peaks;
    QVector<double> m_values;
    int m_channelWidth;
    int m_channelDistance;
    int m_channelFillWidth;
    void drawBackground(int channels = 2);

public slots:
    void setAudioValues(const QVector<double> &values);
};

#endif
