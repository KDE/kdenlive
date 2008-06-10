/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include <QPen>

#include <KDebug>

#include "guide.h"

Guide::Guide(GenTime pos, QString label, double scale, double fps, double height)
        : QGraphicsLineItem(), m_position(pos), m_label(label), m_fps(fps) {

    setLine(m_position.frames(m_fps) * scale, 0, m_position.frames(m_fps) * scale, height);
    setPen(QPen(QColor(0, 0, 200, 180)));
    setZValue(999);
}


void Guide::updatePosition(double scale, double height) {
    setLine(m_position.frames(m_fps) * scale, 0, m_position.frames(m_fps) * scale, height);
}

GenTime Guide::position() {
	return m_position;
}



