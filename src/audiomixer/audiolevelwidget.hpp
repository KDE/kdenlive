/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
