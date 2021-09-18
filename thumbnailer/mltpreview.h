/*
    SPDX-FileCopyrightText: 2006-2008Marco Gulino <marco@kmobiletools.org>
    adapted for MLT video preview by Jean-Baptiste Mardelle
    SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MLTPREVIEW_H
#define MLTPREVIEW_H

#include <kio/thumbcreator.h>
#include <memory>
#include <mlt++/Mlt.h>

#include <QObject>

class MltPreview : public ThumbCreator
{
public:
    MltPreview();
    ~MltPreview() override;
    bool create(const QString &path, int width, int height, QImage &img) override;
    Flags flags() const override;

protected:
    static int imageVariance(const QImage &image);
    QImage getFrame(std::shared_ptr<Mlt::Producer> producer, int framepos, int width, int height);
};

#endif
