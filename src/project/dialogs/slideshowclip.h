/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef SLIDESHOWCLIP_H
#define SLIDESHOWCLIP_H

#include "definitions.h"
#include "timecode.h"
#include "ui_slideshowclip_ui.h"

#include <KIO/PreviewJob>

class ProjectClip;

class SlideshowClip : public QDialog
{
    Q_OBJECT

public:
    explicit SlideshowClip(const Timecode &tc, QString clipFolder, ProjectClip *clip = nullptr, QWidget *parent = nullptr);
    ~SlideshowClip() override;
    /** return selected path for slideshow in MLT format */
    QString selectedPath();
    QString clipName() const;
    QString clipDuration() const;
    QString lumaDuration() const;
    int imageCount() const;
    bool loop() const;
    bool crop() const;
    bool fade() const;
    QString lumaFile() const;
    int softness() const;
    QString animation() const;

    /** @brief Get the image frame number from a file path, for example image_047.jpg will return 47. */
    static int getFrameNumberFromPath(const QUrl &path);
    /** @brief return the url pattern for selected slideshow. */
    static QString selectedPath(const QUrl &url, bool isMime, QString extension, QStringList *list);
    /** @brief Convert the selection animation style into an affine geometry string. */
    static QString animationToGeometry(const QString &animation, int &ttl);

private slots:
    void parseFolder();
    void slotEnableLuma(int state);
    void slotEnableThumbs(int state);
    void slotEnableLumaFile(int state);
    void slotUpdateDurationFormat(int ix);
    void slotGenerateThumbs();
    void slotSetPixmap(const KFileItem &fileItem, const QPixmap &pix);
    /** @brief Display correct widget depending on user choice (MIME type or pattern method). */
    void slotMethodChanged(bool active);

private:
    Ui::SlideshowClip_UI m_view;
    int m_count;
    Timecode m_timecode;
    KIO::PreviewJob *m_thumbJob;
};

#endif
