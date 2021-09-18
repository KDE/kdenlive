/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KTHUMB_H
#define KTHUMB_H

#include <QImage>
#include <QUrl>

namespace Mlt {
class Producer;
class Frame;
} // namespace Mlt

namespace KThumb {
QPixmap getImage(const QUrl &url, int width, int height = -1);
QPixmap getImage(const QUrl &url, int frame, int width, int height = -1);
QImage getFrame(Mlt::Producer *producer, int framepos, int displayWidth, int height);
QImage getFrame(Mlt::Producer &producer, int framepos, int displayWidth, int height);
QImage getFrame(Mlt::Frame *frame, int width = 0, int height = 0, int scaledWidth = 0);
/** @brief Calculates image variance, useful to know if a thumbnail is interesting.
 *  @return an integer between 0 and 100. 0 means no variance, eg. black image while bigger values mean contrasted image
 * */
int imageVariance(const QImage &image);
} // namespace KThumb

#endif
