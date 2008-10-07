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


#ifndef CUSTOMTRACKSCENE_H
#define CUSTOMTRACKSCENE_H

#include <QList>
#include <QGraphicsScene>
#include <QPixmap>

#include "gentime.h"

class KdenliveDoc;
class TrackInfo;

/** This class holds all properties that need to be used by clip items */

class CustomTrackScene : public QGraphicsScene {
    Q_OBJECT

public:
    CustomTrackScene(KdenliveDoc *doc, QObject *parent = 0);
    virtual ~ CustomTrackScene();
    void setSnapList(QList <GenTime> snaps);
    GenTime previousSnapPoint(GenTime pos);
    GenTime nextSnapPoint(GenTime pos);
    double getSnapPointForPos(double pos, bool doSnap = true);
    void setScale(double scale);
    double scale() const;
    QList <TrackInfo> m_tracksList;
    int tracksCount() const;
    QPixmap m_transitionPixmap;

private:
    KdenliveDoc *m_document;
    QList <GenTime> m_snapPoints;
    double m_scale;
};

#endif

