/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "audiolevelconfig.h"
#include "audiolevelrenderer.hpp"
#include "audiolevelstyleprovider.h"
#include "audioleveltypes.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QWidget>
#include <Qt>
#include <memory>

class AudioLevelRenderer;

class AudioLevelWidget : public QWidget
{
    Q_OBJECT
public:
    // Structure to hold theme-aware level colors
    struct LevelColors
    {
        QColor green;
        QColor yellow;
        QColor red;
    };

    explicit AudioLevelWidget(QWidget *parent = nullptr, Qt::Orientation orientation = Qt::Vertical,
                              AudioLevel::TickLabelsMode tickLabelsMode = AudioLevel::TickLabelsMode::Show);
    ~AudioLevelWidget() override;
    void refreshPixmap();
    int audioChannels;
    QSize sizeHint() const override;
    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const;

protected:
    void paintEvent(QPaintEvent *pe) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QPixmap m_backgroundCache;
    QPixmap m_bordersCache;
    QVector<double> m_valueDecibels;
    QVector<double> m_peakDecibels;
    QVector<int> m_valuePrimaryAxisPositions;
    QVector<int> m_peakPrimaryAxisPositions;
    int m_maxDb;
    int m_offset;
    int m_offset_with_labels;
    bool m_displayToolTip;
    Qt::Orientation m_orientation;
    bool m_drawTicksAndLabels;
    bool m_drawLabels;
    AudioLevel::TickLabelsMode m_tickLabelsMode;
    bool m_isHovered;
    QAction *m_solidStyleAction;
    QAction *m_gradientStyleAction;
    QAction *m_blockLinesAction;
    QActionGroup m_levelsFillStyleGroup;
    QActionGroup m_peakStyleGroup;
    QAction *m_colorfulPeakAction;
    QAction *m_monochromePeakAction;
    int m_cachedPrimaryAxisLength = 0;   // Along the orientation axis (excluding borders)
    int m_cachedSecondaryAxisLength = 0; // Perpendicular to the orientation axis (excluding borders)
    bool m_axisLengthsDirty = true;
    AudioLevelRenderer *m_renderer;

    void drawBackground();
    void setupContextMenu();
    void updateContextMenu();
    /** @brief Update tooltip with current dB values */
    void updateToolTip();
    void updateLayoutAndSizing();
    void updatePrimaryAxisPositions();
    void updateAxisLengths();

    AudioLevelLayoutState::Config createLayoutConfig() const;
    AudioLevelRenderer::RenderData createRenderData() const;

public Q_SLOTS:
    void setAudioValues(const QVector<double> &values);
};
