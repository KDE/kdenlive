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
constexpr int SPACE_FOR_CLIPPING_INDICATOR = 9; // Size of the clipping indicator + space between it and the levels (6px + 4px = 10px, see AudioLevelRenderer)
constexpr double MIN_DISPLAY_DB = -100.0;       // Minimum displayable audio level, used to hide decayed peaks and no audio audio levels
constexpr double CLIPPING_THRESHOLD_DB = 0.0;   // Maximum displayable audio level
constexpr int NO_AUDIO_PRIMARY_AXIS_POSITION = -1;

/**
 * @brief AudioLevelWidget constructor
 * @param parent
 * @param orientation Qt::Vertical or Qt::Horizontal
 * @param tickLabelsMode TickLabelsMode for drawing tick marks and labels
 * @param showClippingIndicator Whether to show clipping indicators
 * @param backgroundColor Optional color for channel background, defaults to Window color
 */
AudioLevelWidget::AudioLevelWidget(QWidget *parent, Qt::Orientation orientation, AudioLevel::TickLabelsMode tickLabelsMode, bool showClippingIndicator)
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
    , m_axisDimensionsNeedUpdate(true)
    , m_renderer(new AudioLevelRenderer(this))
    , m_showClippingIndicator(showClippingIndicator)
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
    m_axisDimensionsNeedUpdate = true;
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

    AudioLevelLayoutState layoutState(createLayoutConfig());

    if (m_orientation == Qt::Horizontal && layoutState.isInHoverLabelMode()) {
        // clip to prevent drawing over labels in hovering mode
        QRect clipRect(0, 0, width(), height() - layoutState.getEffectiveBorderOffset());
        p.setClipRect(clipRect);
    }

    updateAxisLengths();

    AudioLevelRenderer::RenderData renderData = createRenderData();
    m_renderer->drawChannelLevels(p, renderData);

    if (m_displayClipping) {
        m_renderer->drawClippingIndicators(p, renderData);
    }

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
    // Create level fill style actions
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
    if (m_showClippingIndicator) {
        switch (m_orientation) {
        case Qt::Horizontal:
            m_displayClipping = newSize.width() > 5 * SPACE_FOR_CLIPPING_INDICATOR;
            break;
        default:
            m_displayClipping = newSize.height() > 5 * SPACE_FOR_CLIPPING_INDICATOR;
            break;
        }
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
    // Clamp all incoming audio values to reasonable bounds that we can visualize
    m_valueDecibels.clear();
    m_valueDecibels.reserve(values.size());
    for (double value : values) {
        double clampedValue = qBound(MIN_DISPLAY_DB, value, CLIPPING_THRESHOLD_DB);
        m_valueDecibels.append(clampedValue);
    }

    bool channelCountChanged = m_valueDecibels.size() != audioChannels;
    bool peaksInitialized = m_peakDecibels.size() != m_valueDecibels.size();

    if (channelCountChanged) {
        audioChannels = m_valueDecibels.size();
        m_axisDimensionsNeedUpdate = true;
        updateLayoutAndSizing();
    }

    // Initialize clipping states and frame counters if needed
    if (m_showClippingIndicator && (channelCountChanged || m_clippingStates.size() != audioChannels)) {
        m_clippingStates.resize(audioChannels);
        m_clippingFrameCounters.resize(audioChannels);
        for (int i = 0; i < audioChannels; i++) {
            m_clippingStates[i] = false;
            m_clippingFrameCounters[i] = 0;
        }
    }

    if (peaksInitialized) {
        m_peakDecibels = m_valueDecibels;
    }

    if (peaksInitialized || channelCountChanged) {
        drawBackground();
    } else {
        // Peak decay logic: peaks slowly decay over time to make the peak naturally fade out
        // Each time new values arrive, existing peaks decay by 0.2 dB
        // If current level is higher than decaying peak, peak is updated to current level
        // This creates a "hold" effect that makes it easier to see brief loud moments.
        for (int i = 0; i < m_valueDecibels.size(); i++) {
            m_peakDecibels[i] -= .2;
            if (m_valueDecibels.at(i) > m_peakDecibels.at(i)) {
                m_peakDecibels[i] = m_valueDecibels.at(i);
            }
            // Clamp peaks to minimum level when fully decayed
            if (m_peakDecibels[i] < MIN_DISPLAY_DB) {
                m_peakDecibels[i] = MIN_DISPLAY_DB;
            }
        }
    }

    if (m_displayClipping) {
        updateClippingStates();
    }

    updatePrimaryAxisPositions();
    update();
}

void AudioLevelWidget::reset()
{
    // Reset levels
    for (int i = 0; i < m_valueDecibels.size(); i++) {
        m_valueDecibels[i] = MIN_DISPLAY_DB;
    }

    // Reset clipping indicators
    if (m_showClippingIndicator) {
        for (int i = 0; i < audioChannels; i++) {
            m_clippingStates[i] = false;
            m_clippingFrameCounters[i] = 0;
        }
    }

    // Reset peaks
    for (int i = 0; i < m_peakDecibels.size(); i++) {
        m_peakDecibels[i] = MIN_DISPLAY_DB;
    }

    updatePrimaryAxisPositions();
    update();
}

void AudioLevelWidget::updateClippingStates()
{
    if (!m_showClippingIndicator || !m_displayClipping || m_clippingStates.size() != audioChannels) {
        return;
    }

    // Calculate frame count for 4-second decay based on current project frame rate
    const double currentFps = pCore->getCurrentFps();
    const int clippingDecayFrames = static_cast<int>(currentFps * 4.0);

    // Clipping decay logic: clipping indicators persist for a fixed time period
    // to make brief clipping events visible even if they occur between updates
    for (int i = 0; i < audioChannels; i++) {
        bool isClipping = m_valueDecibels.at(i) >= CLIPPING_THRESHOLD_DB;

        if (isClipping) {
            // Start or restart the frame counter when clipping is detected
            m_clippingStates[i] = true;
            m_clippingFrameCounters[i] = 0; // Reset frame counter
        } else if (m_clippingStates[i]) {
            // Increment frame counter and check if decay period has expired
            m_clippingFrameCounters[i]++;
            if (m_clippingFrameCounters[i] >= clippingDecayFrames) {
                m_clippingStates[i] = false;
            }
        }
    }
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
        if (m_valueDecibels.at(i) <= MIN_DISPLAY_DB) {
            tip.append(i18n("No audio"));
        } else {
            // Format the number with 2 digits before decimal point and 2 after
            tip.append(QString::asprintf("%+6.2f%s", m_valueDecibels.at(i), i18n("dB").toUtf8().constData()));
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
                m_borderOffset = fontMetrics().height() + MARGIN_BETWEEN_LABEL_AND_LEVELS;
            } else {
                m_borderOffset = fontMetrics().boundingRect(QStringLiteral("-45")).width() + MARGIN_BETWEEN_LABEL_AND_LEVELS;
            }
        } else {
            m_borderOffset = 0;
        }
        m_axisDimensionsNeedUpdate = true;
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
        // Hide current level position if below display threshold
        if (m_valueDecibels.at(i) <= MIN_DISPLAY_DB) {
            m_valuePrimaryAxisPositions[i] = NO_AUDIO_PRIMARY_AXIS_POSITION;
        } else {
            m_valuePrimaryAxisPositions[i] = AudioLevelRenderer::dBToPrimaryOffset(m_valueDecibels.at(i), m_maxDb, m_cachedPrimaryAxisLength, m_orientation);
        }

        // Hide peak position if below display threshold
        if (m_peakDecibels.at(i) <= MIN_DISPLAY_DB) {
            m_peakPrimaryAxisPositions[i] = NO_AUDIO_PRIMARY_AXIS_POSITION;
        } else {
            m_peakPrimaryAxisPositions[i] = AudioLevelRenderer::dBToPrimaryOffset(m_peakDecibels.at(i), m_maxDb, m_cachedPrimaryAxisLength, m_orientation);
        }
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

    int borderOffset = layoutState.getBorderOffset();
    int borderOffsetWithLabels = layoutState.getBorderOffsetWithLabels();

    m_axisDimensionsNeedUpdate = m_axisDimensionsNeedUpdate || m_borderOffset != borderOffset;
    m_borderOffset = borderOffset;
    m_borderOffsetWithLabels = borderOffsetWithLabels;
    m_drawLabels = layoutState.shouldDrawLabels();

    if (m_orientation == Qt::Horizontal) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        int minHeight = audioChannels * MINIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1; // Using 1 for CHANNEL_BORDER_WIDTH
        int maxHeight = audioChannels * MAXIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1;
        if (m_tickLabelsMode != AudioLevel::TickLabelsMode::Hide) {
            minHeight += m_borderOffset;
            maxHeight += m_borderOffsetWithLabels;
        }
        setMinimumHeight(minHeight);
        setMaximumHeight(maxHeight);
    } else {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        int minWidth = audioChannels * MINIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1; // Using 1 for CHANNEL_BORDER_WIDTH
        int maxWidth = audioChannels * MAXIMUM_SECONDARY_AXIS_LENGTH + (audioChannels + 1) * 1;
        if (m_tickLabelsMode != AudioLevel::TickLabelsMode::Hide) {
            minWidth += m_borderOffset;
            maxWidth += m_borderOffsetWithLabels;
        }
        setMinimumWidth(minWidth);
        setMaximumWidth(maxWidth);
    }
    updateGeometry();
}

void AudioLevelWidget::updateAxisLengths()
{
    if (!m_axisDimensionsNeedUpdate) {
        return;
    }

    // Calculate available size for audio levels, accounting for clipping indicators
    QSize availableSize = size();
    if (m_displayClipping) {
        if (m_orientation == Qt::Horizontal) {
            // Reserve space on the right for clipping indicators
            availableSize.setWidth(availableSize.width() - SPACE_FOR_CLIPPING_INDICATOR);
        } else {
            // Reserve space at the top for clipping indicators
            availableSize.setHeight(availableSize.height() - SPACE_FOR_CLIPPING_INDICATOR);
        }
    }

    m_cachedPrimaryAxisLength = AudioLevelRenderer::calculatePrimaryAxisLength(availableSize, m_orientation, AudioLevelConfig::instance().drawBlockLines());

    // Use layout state to calculate secondary axis length
    AudioLevelLayoutState layoutState(createLayoutConfig());
    m_cachedSecondaryAxisLength = layoutState.calculateSecondaryAxisLength();
    m_axisDimensionsNeedUpdate = false;
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
    renderData.showClippingIndicator = m_showClippingIndicator;
    renderData.clippingStates = m_clippingStates;

    return renderData;
}
