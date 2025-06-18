/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiolevelwidget.hpp"
#include "audiomixer/iecscale.h"
#include "core.h"
#include "mlt++/Mlt.h"
#include "profiles/profilemodel.hpp"

#include <KColorScheme>
#include <KLocalizedString>
#include <QFont>
#include <QFontDatabase>
#include <QPaintEvent>
#include <QPainter>
#include <QToolTip>
#include <array>

// Drawing constants moved to AudioLevelRenderer
constexpr int MINIMUM_SECONDARY_AXIS_LENGTH = 3;   // minimum height/width for audio level channels
constexpr int MAXIMUM_SECONDARY_AXIS_LENGTH = 7;   // maximum height/width for audio level channels
constexpr int MARGIN_BETWEEN_LABEL_AND_LEVELS = 4; // px between decibels scale labels and audio levels
constexpr int TICK_MARK_LENGTH = 2;                // px for tick mark
constexpr int NO_AUDIO_DB = -100;
constexpr int NO_AUDIO_PRIMARY_AXIS_POSITION = -1;

/**
 * @brief AudioLevelWidget constructor
 * @param parent
 * @param orientation Qt::Vertical or Qt::Horizontal
 * @param tickLabelsMode TickLabelsMode for drawing tick marks and labels
 * @param backgroundColor Optional color for channel background, defaults to Window color
 */
AudioLevelWidget::AudioLevelWidget(QWidget *parent, Qt::Orientation orientation, AudioLevel::TickLabelsMode tickLabelsMode)
    : QWidget(parent)
    , audioChannels(pCore->audioChannels())
    , m_displayToolTip(false)
    , m_orientation(orientation)
    , m_drawTicksAndLabels(tickLabelsMode != AudioLevel::TickLabelsMode::Hide)
    , m_drawLabels(tickLabelsMode != AudioLevel::TickLabelsMode::Hide)
    , m_tickLabelsMode(tickLabelsMode)
    , m_isHovered(false)
    , m_solidStyleAction(nullptr)
    , m_gradientStyleAction(nullptr)
    , m_blockLinesAction(nullptr)
    , m_levelsFillStyleGroup(this)
    , m_peakStyleGroup(this)
    , m_colorfulPeakAction(nullptr)
    , m_monochromePeakAction(nullptr)
    , m_cachedPrimaryAxisLength(0)
    , m_cachedSecondaryAxisLength(0)
    , m_axisLengthsDirty(true)
    , m_renderer(new AudioLevelRenderer(this))
{
    QFont ft(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    ft.setPointSizeF(ft.pointSize() * 0.6);
    setFont(ft);
    setupContextMenu();
    updateContextMenu();
    updateLayoutAndSizing();

    connect(pCore.get(), &Core::audioLevelsConfigChanged, this, [this]() {
        updateContextMenu();
        update();
    });
}
AudioLevelWidget::~AudioLevelWidget() = default;

void AudioLevelWidget::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_displayToolTip = true;
    m_isHovered = true;
    updateLayoutAndSizing();
    drawBackground();
    updateToolTip();
}

void AudioLevelWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_displayToolTip = false;
    m_isHovered = false;
    updateLayoutAndSizing();
    drawBackground();
}

void AudioLevelWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_axisLengthsDirty = true;
    updateLayoutAndSizing();
    updatePrimaryAxisPositions();
    drawBackground();
}

void AudioLevelWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::PaletteChange) {
        drawBackground();
    }
    QWidget::changeEvent(event);
}

void AudioLevelWidget::paintEvent(QPaintEvent * /*pe*/)
{
    if (!isVisible()) {
        return;
    }
    QPainter p(this);
    p.drawPixmap(0, 0, m_backgroundCache);

    if (m_valueDecibels.isEmpty()) {
        return;
    }

    // Create layout state
    AudioLevelLayoutState layoutState(createLayoutConfig());

    if (m_orientation == Qt::Horizontal && layoutState.isInHoverLabelMode()) {
        // clip to prevent drawing over labels in hovering mode
        QRect clipRect(0, 0, width(), height() - layoutState.getEffectiveOffset());
        p.setClipRect(clipRect);
    }

    updateAxisLengths();

    // Create render data
    AudioLevelRenderer::RenderData renderData = createRenderData();

    m_renderer->drawChannelLevels(p, renderData);

    // Draw cached channel borders on top
    p.drawPixmap(0, 0, m_bordersCache);

    if (m_displayToolTip) {
        updateToolTip();
    }
}

void AudioLevelWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(m_solidStyleAction);
    menu.addAction(m_gradientStyleAction);
    menu.addSeparator();
    menu.addAction(m_blockLinesAction);
    menu.addSeparator();
    menu.addAction(m_colorfulPeakAction);
    menu.addAction(m_monochromePeakAction);
    menu.exec(event->globalPos());
    event->accept();
}

void AudioLevelWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        // As we want to show our own context menu capture right click so it does not propagate to the parent widget
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void AudioLevelWidget::setupContextMenu()
{
    // Create context menu actions
    m_solidStyleAction = new QAction(i18n("Solid Fill"), this);
    m_solidStyleAction->setCheckable(true);
    m_solidStyleAction->setChecked(AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Solid);
    m_solidStyleAction->setActionGroup(&m_levelsFillStyleGroup);

    m_gradientStyleAction = new QAction(i18n("Gradient Fill"), this);
    m_gradientStyleAction->setCheckable(true);
    m_gradientStyleAction->setChecked(AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Gradient);
    m_gradientStyleAction->setActionGroup(&m_levelsFillStyleGroup);

    m_blockLinesAction = new QAction(i18n("Show Block Lines"), this);
    m_blockLinesAction->setCheckable(true);
    m_blockLinesAction->setChecked(AudioLevelConfig::instance().drawBlockLines());
    m_blockLinesAction->setEnabled(AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Solid);

    // Create peak indicator style actions
    m_colorfulPeakAction = new QAction(i18n("Colorful Peak Indicator"), this);
    m_colorfulPeakAction->setCheckable(true);
    m_colorfulPeakAction->setChecked(AudioLevelConfig::instance().peakIndicatorStyle() == AudioLevel::PeakIndicatorStyle::Colorful);
    m_colorfulPeakAction->setActionGroup(&m_peakStyleGroup);

    m_monochromePeakAction = new QAction(i18n("Monochrome Peak Indicator"), this);
    m_monochromePeakAction->setCheckable(true);
    m_monochromePeakAction->setChecked(AudioLevelConfig::instance().peakIndicatorStyle() == AudioLevel::PeakIndicatorStyle::Monochrome);
    m_monochromePeakAction->setActionGroup(&m_peakStyleGroup);

    connect(m_solidStyleAction, &QAction::triggered, this, [this]() {
        AudioLevelConfig::instance().setLevelStyle(AudioLevel::LevelStyle::Solid);
        drawBackground();
        update();
    });

    connect(m_gradientStyleAction, &QAction::triggered, this, [this]() {
        AudioLevelConfig::instance().setLevelStyle(AudioLevel::LevelStyle::Gradient);
        AudioLevelConfig::instance().setDrawBlockLines(false);
        drawBackground();
        update();
    });

    connect(m_blockLinesAction, &QAction::triggered, this, [this](bool checked) {
        AudioLevelConfig::instance().setDrawBlockLines(checked);
        drawBackground();
        update();
    });

    connect(m_colorfulPeakAction, &QAction::triggered, this, [this]() {
        AudioLevelConfig::instance().setPeakIndicatorStyle(AudioLevel::PeakIndicatorStyle::Colorful);
        drawBackground();
        update();
    });

    connect(m_monochromePeakAction, &QAction::triggered, this, [this]() {
        AudioLevelConfig::instance().setPeakIndicatorStyle(AudioLevel::PeakIndicatorStyle::Monochrome);
        drawBackground();
        update();
    });
}

void AudioLevelWidget::updateContextMenu()
{
    // Update UI to match loaded settings
    m_solidStyleAction->setChecked(AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Solid);
    m_gradientStyleAction->setChecked(AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Gradient);
    m_blockLinesAction->setChecked(AudioLevelConfig::instance().drawBlockLines());
    m_blockLinesAction->setEnabled(AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Solid);
    m_colorfulPeakAction->setChecked(AudioLevelConfig::instance().peakIndicatorStyle() == AudioLevel::PeakIndicatorStyle::Colorful);
    m_monochromePeakAction->setChecked(AudioLevelConfig::instance().peakIndicatorStyle() == AudioLevel::PeakIndicatorStyle::Monochrome);
}

void AudioLevelWidget::refreshPixmap()
{
    drawBackground();
}

void AudioLevelWidget::drawBackground()
{
    if (height() == 0 || width() == 0 || audioChannels == 0 || m_valueDecibels.isEmpty()) {
        return;
    }
    QSize newSize = QWidget::size();
    if (!newSize.isValid()) {
        return;
    }
    qreal scalingFactor = devicePixelRatioF();
    m_backgroundCache = QPixmap(newSize * scalingFactor);
    m_backgroundCache.setDevicePixelRatio(scalingFactor);
    if (m_backgroundCache.isNull()) {
        return;
    }
    const QVector<int> dbscale = {0, -6, -12, -18, -24, -30, -36, -42, -48, -54};
    m_maxDb = dbscale.first();
    m_backgroundCache.fill(Qt::transparent);
    QPainter p(&m_backgroundCache);

    updateAxisLengths();

    // Create render data
    AudioLevelRenderer::RenderData renderData = createRenderData();

    m_renderer->drawBackground(p, renderData);
    p.end();

    // Update borders cache
    m_bordersCache = QPixmap(newSize * scalingFactor);
    m_bordersCache.setDevicePixelRatio(scalingFactor);
    if (!m_bordersCache.isNull()) {
        m_renderer->drawChannelBordersToPixmap(m_bordersCache, renderData);
    }

    update();
}

// cppcheck-suppress unusedFunction
void AudioLevelWidget::setAudioValues(const QVector<double> &values)
{
    m_valueDecibels = values;
    bool channelCountChanged = m_valueDecibels.size() != audioChannels;
    bool peaksInitialized = m_peakDecibels.size() != m_valueDecibels.size();

    if (channelCountChanged) {
        audioChannels = m_valueDecibels.size();
        m_axisLengthsDirty = true;
        updateLayoutAndSizing();
    }
    if (peaksInitialized) {
        m_peakDecibels = values;
    }

    if (peaksInitialized || channelCountChanged) {
        drawBackground();
    } else {
        for (int i = 0; i < m_valueDecibels.size(); i++) {
            m_peakDecibels[i] -= .2;
            if (m_valueDecibels.at(i) > m_peakDecibels.at(i)) {
                m_peakDecibels[i] = m_valueDecibels.at(i);
            }
        }
    }
    updatePrimaryAxisPositions();
    update();
}

void AudioLevelWidget::updateToolTip()
{
    if (!isEnabled()) {
        return;
    }
    QString tip;
    int channels = m_valueDecibels.count();
    for (int i = 0; i < channels; i++) {
        // Add channel prefix
        if (channels == 2) {
            // Stereo mode
            tip.append(i == 0 ? i18nc("L as in Left", "L: ") : i18nc("R as in Right", "R: "));
        } else {
            // Non-stereo mode
            QString channelPrefix = i18nc("CH as in Channel", "CH");
            tip.append(QStringLiteral("%1%2: ").arg(channelPrefix).arg(i + 1));
        }

        // Add value or "No audio"
        if (m_valueDecibels.at(i) == NO_AUDIO_DB) {
            tip.append(i18n("No audio"));
        } else {
            // Format the number with 2 digits before decimal point and 2 after
            tip.append(QString::asprintf("%+6.2fdB", m_valueDecibels.at(i)));
        }

        // Add newline if not the last channel
        if (i < channels - 1) {
            tip.append(QStringLiteral("\n"));
        }
    }
    QToolTip::showText(QCursor::pos(), tip, this);
}

void AudioLevelWidget::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation != orientation) {
        m_orientation = orientation;
        // Update offset based on whether we draw ticks/labels
        if (m_drawTicksAndLabels) {
            if (m_orientation == Qt::Horizontal) {
                m_offset = fontMetrics().height() + MARGIN_BETWEEN_LABEL_AND_LEVELS;
            } else {
                m_offset = fontMetrics().boundingRect(QStringLiteral("-45")).width() + MARGIN_BETWEEN_LABEL_AND_LEVELS;
            }
        } else {
            m_offset = 0;
        }
        m_axisLengthsDirty = true;
        drawBackground();
    }
}

Qt::Orientation AudioLevelWidget::orientation() const
{
    return m_orientation;
}

void AudioLevelWidget::updatePrimaryAxisPositions()
{
    int channels = m_valueDecibels.count();
    m_valuePrimaryAxisPositions.resize(channels);
    m_peakPrimaryAxisPositions.resize(channels);
    for (int i = 0; i < channels; i++) {
        if (m_valueDecibels.at(i) == NO_AUDIO_DB || m_peakDecibels.at(i) == NO_AUDIO_DB || m_valueDecibels.at(i) >= 100) {
            m_valuePrimaryAxisPositions[i] = NO_AUDIO_PRIMARY_AXIS_POSITION;
            m_peakPrimaryAxisPositions[i] = NO_AUDIO_PRIMARY_AXIS_POSITION;
            continue;
        }
        m_valuePrimaryAxisPositions[i] = AudioLevelRenderer::dBToPrimaryOffset(m_valueDecibels.at(i), m_maxDb, m_cachedPrimaryAxisLength, m_orientation);
        m_peakPrimaryAxisPositions[i] = AudioLevelRenderer::dBToPrimaryOffset(m_peakDecibels.at(i), m_maxDb, m_cachedPrimaryAxisLength, m_orientation);
    }
}

QSize AudioLevelWidget::sizeHint() const
{
    if (!isVisible()) {
        return QSize(0, 0);
    }
    if (m_orientation == Qt::Horizontal) {
        return QSize(QWidget::sizeHint().width(), maximumHeight());
    }
    return QSize(maximumWidth(), QWidget::sizeHint().height());
}

void AudioLevelWidget::updateLayoutAndSizing()
{
    // Create layout state to calculate offsets
    AudioLevelLayoutState layoutState(createLayoutConfig());

    int offset = layoutState.getOffset();
    int offsetWithLabels = layoutState.getOffsetWithLabels();

    m_axisLengthsDirty = m_axisLengthsDirty || m_offset != offset;
    m_offset = offset;
    m_offset_with_labels = offsetWithLabels;
    m_drawLabels = layoutState.shouldDrawLabels();

    if (m_orientation == Qt::Horizontal) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        int minHeight = audioChannels * MINIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1; // Using 1 for CHANNEL_BORDER_WIDTH
        int maxHeight = audioChannels * MAXIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1;
        if (m_tickLabelsMode != AudioLevel::TickLabelsMode::Hide) {
            minHeight += m_offset;
            maxHeight += m_offset_with_labels;
        }
        setMinimumHeight(minHeight);
        setMaximumHeight(maxHeight);
    } else {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        int minWidth = audioChannels * MINIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1; // Using 1 for CHANNEL_BORDER_WIDTH
        int maxWidth = audioChannels * MAXIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1;
        if (m_tickLabelsMode != AudioLevel::TickLabelsMode::Hide) {
            minWidth += m_offset;
            maxWidth += m_offset_with_labels;
        }
        setMinimumWidth(minWidth);
        setMaximumWidth(maxWidth);
    }
    updateGeometry();
}

void AudioLevelWidget::updateAxisLengths()
{
    if (!m_axisLengthsDirty) {
        return;
    }

    m_cachedPrimaryAxisLength = AudioLevelRenderer::calculatePrimaryAxisLength(size(), m_orientation, AudioLevelConfig::instance().drawBlockLines());

    // Use layout state to calculate secondary axis length
    AudioLevelLayoutState layoutState(createLayoutConfig());
    m_cachedSecondaryAxisLength = layoutState.calculateSecondaryAxisLength();
    m_axisLengthsDirty = false;
}

AudioLevelLayoutState::Config AudioLevelWidget::createLayoutConfig() const
{
    AudioLevelLayoutState::Config config;
    config.tickLabelsMode = m_tickLabelsMode;
    config.orientation = m_orientation;
    config.widgetSize = size();
    config.fontMetrics = fontMetrics();
    config.audioChannels = audioChannels;
    config.isHovered = m_isHovered;
    return config;
}

AudioLevelRenderer::RenderData AudioLevelWidget::createRenderData() const
{
    AudioLevelRenderer::RenderData renderData(createLayoutConfig());
    renderData.valueDecibels = m_valueDecibels;
    renderData.peakDecibels = m_peakDecibels;
    renderData.valuePrimaryAxisPositions = m_valuePrimaryAxisPositions;
    renderData.peakPrimaryAxisPositions = m_peakPrimaryAxisPositions;
    renderData.audioChannels = audioChannels;
    renderData.orientation = m_orientation;
    renderData.maxDb = m_maxDb;
    renderData.isEnabled = isEnabled();
    renderData.palette = palette();
    renderData.font = font();
    renderData.fontMetrics = fontMetrics();
    renderData.primaryAxisLength = m_cachedPrimaryAxisLength;
    renderData.secondaryAxisLength = m_cachedSecondaryAxisLength;
    return renderData;
}