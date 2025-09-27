/*
SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "previewpanel.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <KConfigGroup>
#include <KDualAction>
#include <KIO/PreviewJob>
#include <KIconLoader>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KSqueezedTextLabel>
#include <QAction>
#include <QAudioOutput>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVideoWidget>

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *panelLayout = new QVBoxLayout(this);
    // lay->setSpacing(0);

    // Video preview
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setMinimumHeight(KIconLoader::SizeEnormous);
    m_videoWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    panelLayout->addWidget(m_videoWidget);
    m_videoWidget->installEventFilter(this);

    // Image preview
    m_imageWidget = new QLabel(this);
    m_imageWidget->setMinimumHeight(KIconLoader::SizeEnormous);
    m_imageWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_imageWidget->setScaledContents(false);
    m_imageWidget->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_imageWidget->installEventFilter(this);
    panelLayout->addWidget(m_imageWidget);

    // Controls panel
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    m_playButton = new QToolButton(this);
    m_playButton->setToolTip(i18nc("@info:tooltip play a video or audio file", "Play media"));
    m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_playButton->setAutoRaise(true);
    controlsLayout->addWidget(m_playButton);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 100);
    controlsLayout->addWidget(m_slider);
    QToolButton *playAudio = new QToolButton(this);
    KDualAction *playAudioAction = new KDualAction(i18n("Mute"), i18n("Unmute"), this);
    playAudioAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-muted")));
    playAudioAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
    playAudioAction->setCheckable(true);
    playAudioAction->setInactiveToolTip(i18nc("@info:tooltip", "Mute audio"));
    playAudioAction->setActiveToolTip(i18nc("@info:tooltip", "Unmute audio"));
    playAudio->setDefaultAction(playAudioAction);
    playAudio->setAutoRaise(true);
    playAudioAction->setActive(KdenliveSettings::mediaBrowserWithAudio());
    controlsLayout->addWidget(playAudio);
    connect(playAudioAction, &KDualAction::activeChangedByUser, this, [this](bool muteAudio) {
        KdenliveSettings::setMediaBrowserWithAudio(muteAudio);
        bool wasPlaying = m_player && m_player->isPlaying();
        pausePlaying();
        if (m_player) {
            m_player->setAudioOutput(nullptr);
        }
        if (wasPlaying) {
            startPlaying();
        }
    });
    panelLayout->addLayout(controlsLayout);

    // Info panel
    m_metadataLayout = new QFormLayout();
    m_resolutionLabel = new KSqueezedTextLabel(this);
    m_durationLabel = new KSqueezedTextLabel(this);
    m_dateLabel = new KSqueezedTextLabel(this);
    // Resolution and fps
    m_metadataLayout->addRow(m_resolutionLabel);
    // Duration
    m_metadataLayout->addRow(m_durationLabel);
    // Date
    m_metadataLayout->addRow(m_dateLabel);
    QCheckBox *autoPlay = new QCheckBox(i18nc("@action:checkbox enable automatic playback", "Autoplay"), this);
    autoPlay->setChecked(KdenliveSettings::mediaBrowserAutoPlay());
    connect(autoPlay, &QCheckBox::toggled, this, [](bool enable) { KdenliveSettings::setMediaBrowserAutoPlay(enable); });

    m_metadataLayout->addRow(autoPlay);
    panelLayout->addLayout(m_metadataLayout);

    connect(m_playButton, &QToolButton::clicked, this, &PreviewPanel::switchPlay);

    m_importButton = new QPushButton(i18nc("@action:button", "Import File"), this);
    panelLayout->addWidget(m_importButton);
    m_importButton->setEnabled(false);
    connect(m_importButton, &QPushButton::clicked, this, &PreviewPanel::importSelection);

    m_playButton->setEnabled(false);
    m_slider->setEnabled(false);
    m_videoWidget->hide();
    m_imageWidget->show();
}

void PreviewPanel::switchPlay()
{
    buildPlayer();
    if (m_player->isPlaying()) {
        pausePlaying();
    } else {
        if (!m_videoWidget->isVisible() && m_player->hasVideo()) {
            m_videoWidget->show();
            m_imageWidget->hide();
        }
        startPlaying();
    }
}

bool PreviewPanel::eventFilter(QObject *watched, QEvent *event)
{
    // To avoid shortcut conflicts between the media browser and main app, we dis/enable actions when we gain/lose focus
    if (event->type() == QEvent::MouseButtonPress && (watched == m_videoWidget || watched == m_imageWidget)) {
        if (m_playButton->isEnabled()) {
            switchPlay();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PreviewPanel::hideEvent(QHideEvent *event)
{
    resetPlayer();
    QWidget::hideEvent(event);
}

bool PreviewPanel::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        resetPlayer();
    }
    return QWidget::event(event);
}

void PreviewPanel::startPlaying()
{
    buildPlayer();
    if (KdenliveSettings::mediaBrowserWithAudio() && m_player->hasAudio()) {
        if (m_player->audioOutput() == nullptr) {
            m_player->setAudioOutput(new QAudioOutput);
        }
    }
    m_player->play();
}

void PreviewPanel::pausePlaying()
{
    if (m_player) {
        m_player->pause();
    }
}

void PreviewPanel::stopPlaying()
{
    if (m_player) {
        m_player->setAudioOutput(nullptr);
        m_player->stop();
    }
}

void PreviewPanel::disableImport()
{
    m_importButton->setEnabled(false);
}

void PreviewPanel::fileSelected(KFileItemList files)
{
    const KFileItem item = files.constFirst();
    if (files.size() > 1) {
        m_importButton->setText(i18nc("@action:button", "Import %1 Files", files.size()));
    } else {
        if (item.isDir()) {
            m_importButton->setText(i18nc("@action:button", "Import Folder"));
        } else {
            m_importButton->setText(i18nc("@action:button", "Import File"));
        }
    }
    if (m_item.entry() == item.entry()) {
        return;
    }

    m_item = item;
    m_importButton->setEnabled(true);
    const QString mimeType = m_item.mimetype();
    m_isVideo = mimeType.startsWith(QLatin1String("video/"));
    m_useMedia = m_isVideo || mimeType.startsWith(QLatin1String("audio/"));
    if (m_useMedia) {
        if (m_player) {
            m_player->stop();
            m_slider->setValue(0);
            m_player->setSource(m_item.url());
        } else {
            buildPlayer();
        }
        m_playButton->setEnabled(true);
        m_slider->setEnabled(true);
        if (KdenliveSettings::mediaBrowserAutoPlay()) {
            if (m_isVideo) {
                m_imageWidget->hide();
                m_videoWidget->show();
            } else {
                m_imageWidget->show();
                m_videoWidget->hide();
                refreshPixmapView();
            }
            startPlaying();
            return;
        }
    } else {
        if (m_player) {
            stopPlaying();
            m_player->setSource(QUrl());
        }
        m_playButton->setEnabled(false);
        m_slider->setEnabled(false);
    }
    m_videoWidget->hide();
    m_imageWidget->show();
    refreshPixmapView();
}

void PreviewPanel::refreshPixmapView()
{
    // If there is a preview job, kill it to prevent that we have jobs for
    // multiple items running, and thus a race condition (bug 250787).
    if (m_previewJob) {
        m_previewJob->kill();
    }
    qDebug() << "::: LAUNCHING PREVIEW FOR: " << m_item.url();

    // try to get a preview pixmap from the item...

    // Mark the currently shown preview as outdated. This is done
    // with a small delay to prevent a flickering when the next preview
    // can be shown within a short timeframe.
    // m_outdatedPreviewTimer->start();

    const KConfigGroup globalConfig(KSharedConfig::openConfig(), "PreviewSettings");
    const QStringList plugins = globalConfig.readEntry("Plugins", KIO::PreviewJob::defaultPlugins());
    m_previewJob = new KIO::PreviewJob(KFileItemList() << m_item, QSize(300, 300), &plugins);
    m_previewJob->setScaleType(KIO::PreviewJob::Unscaled);
    m_previewJob->setIgnoreMaximumSize(m_item.isLocalFile() && !m_item.isSlow());
    m_previewJob->setDevicePixelRatio(m_imageWidget->devicePixelRatioF());
    if (m_previewJob->uiDelegate()) {
        KJobWidgets::setWindow(m_previewJob, this);
    }

    connect(m_previewJob.data(), &KIO::PreviewJob::gotPreview, this, &PreviewPanel::showPreview);
    connect(m_previewJob.data(), &KIO::PreviewJob::failed, this, &PreviewPanel::showIcon);
}

void PreviewPanel::showIcon(const KFileItem &item)
{
    // m_outdatedPreviewTimer->stop();
    QIcon icon = QIcon::fromTheme(item.iconName());
    // QPixmap pixmap = KIconUtils::addOverlays(icon, item.overlays()).pixmap(m_preview->size(), devicePixelRatioF());
    QPixmap pixmap = (icon).pixmap(m_imageWidget->size(), m_imageWidget->devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_imageWidget->setPixmap(pixmap);
}

void PreviewPanel::showPreview(const KFileItem &item, const QPixmap &pixmap)
{
    // m_outdatedPreviewTimer->stop();

    QPixmap p = pixmap;
    m_imageWidget->setPixmap(p.scaled(m_imageWidget->size(), Qt::KeepAspectRatio));

    //(m_imageWidget->size(), m_imageWidget->devicePixelRatioF());

    /*if (!item.overlays().isEmpty()) {
        // Avoid scaling the images that are smaller than the preview size, to be consistent when there is no overlays
        if (pixmap.height() < m_preview->height() && pixmap.width() < m_preview->width()) {
            p = QPixmap(m_preview->size() * devicePixelRatioF());
            p.fill(Qt::transparent);
            p.setDevicePixelRatio(devicePixelRatioF());

            QPainter painter(&p);
            painter.drawPixmap(QPointF{m_preview->width() / 2.0 - pixmap.width() / pixmap.devicePixelRatioF() / 2,
                                       m_preview->height() / 2.0 - pixmap.height() / pixmap.devicePixelRatioF() / 2}
                                   .toPoint(),
                               pixmap);
        }
        p = KIconUtils::addOverlays(p, item.overlays()).pixmap(m_preview->size(), devicePixelRatioF());
        p.setDevicePixelRatio(devicePixelRatioF());
    }*/
}

void PreviewPanel::buildPlayer()
{
    if (m_player) {
        // Player already initialized, just change source
        return;
    }
    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(m_videoWidget);
    connect(m_slider, &QAbstractSlider::sliderMoved, this, &PreviewPanel::updatePlayerPos);
    m_slider->setValue(0);
    if (m_useMedia) {
        m_player->setSource(m_item.url());
    }
    connect(m_player, &QMediaPlayer::playingChanged, this, [this](bool playing) {
        if (playing) {
            m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        } else {
            m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        }
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        qDebug() << ":::: MEDIA STATUS CHANGED: " << status;
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::BufferingMedia) {
            m_playButton->setEnabled(true);
            if (m_player->hasVideo()) {
                if (m_player->isPlaying()) {
                    m_videoWidget->show();
                    m_imageWidget->hide();
                }
            } else {
                m_imageWidget->show();
                m_videoWidget->hide();
            }
            if (m_player->hasAudio()) {
                if (KdenliveSettings::mediaBrowserWithAudio() && m_player->audioOutput() == nullptr) {
                    m_player->setAudioOutput(new QAudioOutput);
                }
            } else {
                m_player->setAudioOutput(nullptr);
            }
        } else if (status == QMediaPlayer::EndOfMedia) {
            m_player->setPosition(0);
            m_playButton->setEnabled(true);
        } else {
            m_playButton->setEnabled(false);
        }
    });

    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
        qDebug() << "---- RECEIVED MEDIA PLAYER ERROR: " << errorString;
        qDebug() << "HO BACKEDNS: " << qgetenv("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES");
        if (error == QMediaPlayer::FormatError) {
        }
    });

    connect(m_player, &QMediaPlayer::metaDataChanged, this, [this]() {
        const QMediaMetaData mData = m_player->metaData();
        if (!mData.value(QMediaMetaData::Resolution).isNull() || !mData.value(QMediaMetaData::VideoFrameRate).isNull()) {
            const QString fps = QString("%1").arg(mData.value(QMediaMetaData::VideoFrameRate).toReal(), 0, 'f', 2);
            m_resolutionLabel->setText(
                QStringLiteral("%1, %2%3")
                    .arg(mData.stringValue(QMediaMetaData::Resolution), fps, i18nc("@info:label fps as frames per second shortcut", "fps")));
        } else {
            m_resolutionLabel->clear();
        }
        int audioTracks = m_player->audioTracks().size();
        QString audioTracksLabel;
        switch (audioTracks) {
        case 0:
            audioTracksLabel = i18n("No audio");
            break;
        default:
            audioTracksLabel = i18ncp("@info:label indicate the number of audio tracks (streams) in a file", "1 audio track", "%1 audio tracks", audioTracks);
            break;
        }

        if (!mData.value(QMediaMetaData::Duration).isNull()) {
            m_durationLabel->setText(QStringLiteral("%1, %2").arg(mData.stringValue(QMediaMetaData::Duration)).arg(audioTracksLabel));
        } else {
            if (!m_player->source().isEmpty()) {
                m_durationLabel->setText(audioTracksLabel);
            } else {
                m_durationLabel->clear();
            }
        }
        if (!mData.value(QMediaMetaData::Date).isNull()) {
            m_dateLabel->setText(mData.stringValue(QMediaMetaData::Date));
        } else {
            m_dateLabel->clear();
        }
    });

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_slider->setValue(100 * position / m_player->duration());
        }
    });
}

void PreviewPanel::updatePlayerPos(int position)
{
    if (!m_player) {
        return;
    }
    if (!m_videoWidget->isVisible() && m_player->hasVideo()) {
        m_imageWidget->hide();
        m_videoWidget->show();
    }
    if (m_player->mediaStatus() == QMediaPlayer::BufferingMedia) {
        // Don't ask to seek if we are buffering
        return;
    }
    m_player->setPosition(m_player->duration() * position / 100);
    if (m_player->playbackState() == QMediaPlayer::StoppedState) {
        m_player->play();
    }
}

void PreviewPanel::resetPlayer()
{
    if (m_player == nullptr) {
        return;
    }
    stopPlaying();
    disconnect(m_slider, &QAbstractSlider::sliderMoved, this, &PreviewPanel::updatePlayerPos);
    delete m_player;
    m_player = nullptr;
    m_videoWidget->hide();
    m_imageWidget->show();
}
