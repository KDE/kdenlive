/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
