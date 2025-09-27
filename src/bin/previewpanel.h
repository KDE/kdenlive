/*
SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KFileItem>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class QMediaPlayer;
class QLabel;
class QSlider;
class QToolButton;
class QVideoWidget;
class QFormLayout;
class QPushButton;
class KSqueezedTextLabel;

namespace KIO {
class PreviewJob;
}

/**
 * @class PreviewPanel
 * @brief The bin widget takes care of both item model and view upon project opening.
 */
class PreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewPanel(QWidget *parent = nullptr);
    const QUrl url() const;
    void setUrl(const QUrl url);
    void back();
    void forward();
    void fileSelected(KFileItemList files);
    void disableImport();
    void resetPlayer();

protected:
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QMediaPlayer *m_player{nullptr};
    QPointer<KIO::PreviewJob> m_previewJob{nullptr};
    KFileItem m_item;
    QVideoWidget *m_videoWidget;
    QLabel *m_imageWidget;
    QToolButton *m_playButton;
    QPushButton *m_importButton;
    QSlider *m_slider;
    QFormLayout *m_metadataLayout;
    KSqueezedTextLabel *m_resolutionLabel;
    KSqueezedTextLabel *m_durationLabel;
    KSqueezedTextLabel *m_dateLabel;
    bool m_isVideo{false};
    bool m_useMedia{false};
    void openExternalFile(const QUrl &url);
    void startPlaying();
    void pausePlaying();
    void stopPlaying();
    void buildPlayer();

private Q_SLOTS:
    void refreshPixmapView();
    void showPreview(const KFileItem &item, const QPixmap &pixmap);
    void showIcon(const KFileItem &item);
    void switchPlay();
    void updatePlayerPos(int position);

Q_SIGNALS:
    void importSelection();
};
