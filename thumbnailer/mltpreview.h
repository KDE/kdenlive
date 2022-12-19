/*
    SPDX-FileCopyrightText: 2006-2008 Marco Gulino <marco@kmobiletools.org>
    adapted for MLT video preview by Jean-Baptiste Mardelle
    SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KIO/ThumbnailCreator>
#include <memory>
#include <mlt++/Mlt.h>

#include <QObject>

class MltPreview : public KIO::ThumbnailCreator
{
public:
    MltPreview(QObject *parent, const QVariantList &args);
    ~MltPreview() override;
    KIO::ThumbnailResult create(const KIO::ThumbnailRequest &request) override;

protected:
    static int imageVariance(const QImage &image);
    QImage getFrame(std::shared_ptr<Mlt::Producer> producer, int framepos, int width, int height);
};
