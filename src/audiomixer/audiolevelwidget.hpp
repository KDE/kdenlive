/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;

private:
    std::unique_ptr<Mlt::Filter> m_filter;
    int m_width;
    int m_offset;
    QPixmap m_pixmap;
    QVector<double> m_peaks;
    QVector<double> m_values;
    int m_maxDb;
    int m_channelWidth;
    int m_channelDistance;
    int m_channelFillWidth;
    bool m_displayToolTip;
    void drawBackground(int channels = 2);
    /** @brief Update tooltip with current dB values */
    void updateToolTip();

public slots:
    void setAudioValues(const QVector<double> &values);
};
