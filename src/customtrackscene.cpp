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

#include "customtrackscene.h"
#include "kdenlivedoc.h"

#include <KStandardDirs>

CustomTrackScene::CustomTrackScene(KdenliveDoc *doc, QObject *parent) :
        QGraphicsScene(parent),
        m_document(doc),
        m_scale(1.0, 1.0),
        m_editMode(NORMALEDIT)
{
}

CustomTrackScene::~CustomTrackScene()
{
}

double CustomTrackScene::getSnapPointForPos(double pos, bool doSnap)
{
    double maximumOffset;
    if (doSnap) {
        if (m_scale.x() > 3) maximumOffset = 10 / m_scale.x();
        else maximumOffset = 6 / m_scale.x();
        for (int i = 0; i < m_snapPoints.size(); ++i) {
            if (qAbs((int)(pos - m_snapPoints.at(i).frames(m_document->fps()))) < maximumOffset) {
                return m_snapPoints.at(i).frames(m_document->fps());
            }
            if (m_snapPoints.at(i).frames(m_document->fps()) > pos) break;
        }
    }
    return GenTime(pos, m_document->fps()).frames(m_document->fps());
}

void CustomTrackScene::setSnapList(QList <GenTime> snaps)
{
    m_snapPoints = snaps;
}

GenTime CustomTrackScene::previousSnapPoint(GenTime pos) const
{
    for (int i = 0; i < m_snapPoints.size(); ++i) {
        if (m_snapPoints.at(i) >= pos) {
            if (i == 0) return GenTime();
            return m_snapPoints.at(i - 1);
        }
    }
    return GenTime();
}

GenTime CustomTrackScene::nextSnapPoint(GenTime pos) const
{
    for (int i = 0; i < m_snapPoints.size(); ++i) {
        if (m_snapPoints.at(i) > pos) {
            return m_snapPoints.at(i);
        }
    }
    return pos;
}

void CustomTrackScene::setScale(double scale, double vscale)
{
    m_scale.setX(scale);
    m_scale.setY(vscale);
}

QPointF CustomTrackScene::scale() const
{
    return m_scale;
}

int CustomTrackScene::tracksCount() const
{
    return m_document->tracksCount();
}

MltVideoProfile CustomTrackScene::profile() const
{
    return m_document->mltProfile();
}

void CustomTrackScene::setEditMode(EDITMODE mode)
{
    m_editMode = mode;
}

EDITMODE CustomTrackScene::editMode() const
{
    return m_editMode;
}

#include "customtrackscene.moc"
