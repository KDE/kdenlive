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

#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <KIcon>
#include <KCursor>
#include <KGlobalSettings>

#include "keyframehelper.h"

#include "definitions.h"


KeyframeHelper::KeyframeHelper(QWidget *parent)
        : QWidget(parent), m_geom(NULL) {
    setFont(KGlobalSettings::toolBarFont());

}

// virtual
void KeyframeHelper::mousePressEvent(QMouseEvent * event) {
    /*    if (event->button() == Qt::RightButton) {
            m_contextMenu->exec(event->globalPos());
            return;
        }
        m_view->activateMonitor();
        int pos = (int)((event->x() + offset()));
        m_moveCursor = RULER_CURSOR;
        if (event->y() > 10) {
            if (qAbs(pos - m_zoneStart * m_factor) < 4) m_moveCursor = RULER_START;
            else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2) * m_factor) < 4) m_moveCursor = RULER_MIDDLE;
            else if (qAbs(pos - m_zoneEnd * m_factor) < 4) m_moveCursor = RULER_END;
        }
        if (m_moveCursor == RULER_CURSOR)
            m_view->setCursorPos((int) pos / m_factor);
    */
}

// virtual
void KeyframeHelper::mouseMoveEvent(QMouseEvent * event) {
    /*    if (event->buttons() == Qt::LeftButton) {
            int pos = (int)((event->x() + offset()) / m_factor);
            if (pos < 0) pos = 0;
            if (m_moveCursor == RULER_CURSOR) {
                m_view->setCursorPos(pos);
                return;
            } else if (m_moveCursor == RULER_START) m_zoneStart = pos;
            else if (m_moveCursor == RULER_END) m_zoneEnd = pos;
            else if (m_moveCursor == RULER_MIDDLE) {
                int move = pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2);
                m_zoneStart += move;
                m_zoneEnd += move;
            }
            m_view->setDocumentModified();
            update();
        } else {
            int pos = (int)((event->x() + offset()));
            if (event->y() <= 10) setCursor(Qt::ArrowCursor);
            else if (qAbs(pos - m_zoneStart * m_factor) < 4) setCursor(KCursor("left_side", Qt::SizeHorCursor));
            else if (qAbs(pos - m_zoneEnd * m_factor) < 4) setCursor(KCursor("right_side", Qt::SizeHorCursor));
            else if (qAbs(pos - (m_zoneStart + (m_zoneEnd - m_zoneStart) / 2) * m_factor) < 4) setCursor(Qt::SizeHorCursor);
            else setCursor(Qt::ArrowCursor);
        }
    */
}


// virtual
void KeyframeHelper::wheelEvent(QWheelEvent * e) {
    /*    int delta = 1;
        if (e->modifiers() == Qt::ControlModifier) delta = m_timecode.fps();
        if (e->delta() < 0) delta = 0 - delta;
        m_view->moveCursorPos(delta);
    */
}

// virtual
void KeyframeHelper::paintEvent(QPaintEvent *e) {
    QStylePainter p(this);
    p.setClipRect(e->rect());

    if (m_geom != NULL) {
        int pos = 0;
        Mlt::GeometryItem item;
        for (int i = 0; i < m_geom->length(); i++) {
            m_geom->next_key(&item, pos);
            pos = item.frame();
            kDebug() << "++ PAINTING POS: " << pos;
            int scaledPos = pos * width() / m_length;
            p.fillRect(QRect(scaledPos - 1, 0, 2, 15), QBrush(QColor(255, 20, 20)));
            pos++;
        }
    }
}

void KeyframeHelper::setKeyGeometry(Mlt::Geometry *geom, const int length) {
    m_geom = geom;
    m_length = length;
    kDebug() << "KEYFRAMES: " << m_geom->length() << ", TRANS SOZE: " << m_length;
    update();
}

#include "keyframehelper.moc"
