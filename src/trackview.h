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

/**
* @class TrackView
* @brief Manages the timline
* @author Jean-Baptiste Mardelle
*/

#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QScrollArea>
#include <QGroupBox>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QDomElement>

#include <KRuler>

#include "customtrackscene.h"
#include "effectslist.h"
#include "ui_timeline_ui.h"
#include "definitions.h"

class ClipItem;
class Transition;
class CustomTrackView;
class KdenliveDoc;
class CustomRuler;
class DocClipBase;

class TrackView : public QWidget, public Ui::TimeLine_UI
{
    Q_OBJECT

public:
    explicit TrackView(KdenliveDoc *doc, bool *ok, QWidget *parent = 0);
    virtual ~ TrackView();
    void setEditMode(const QString & editMode);
    const QString & editMode() const;
    QGraphicsScene *projectScene();
    CustomTrackView *projectView();
    int duration() const;
    int tracksNumber() const;
    KdenliveDoc *document();
    void refresh() ;
    void updateProjectFps();
    int outPoint() const;
    int inPoint() const;
    int fitZoom() const;

    /** @brief Updates (redraws) the ruler.
    *
    * Used to change from displaying frames to timecode or vice versa. */
    void updateRuler();

    /** @brief Parse tracks to see if project has audio in it.
    *
    * Parses all tracks to check if there is audio data. */
    bool checkProjectAudio() const;

protected:
    virtual void keyPressEvent(QKeyEvent * event);

public slots:
    void slotDeleteClip(const QString &clipId);
    void slotChangeZoom(int horizontal, int vertical = -1);
    void setDuration(int dur);
    void slotSetZone(QPoint p);

private:
    CustomRuler *m_ruler;
    CustomTrackView *m_trackview;
    QList <QString> m_invalidProducers;
    double m_scale;
    int m_projectTracks;
    QString m_editMode;
    CustomTrackScene *m_scene;

    KdenliveDoc *m_doc;
    int m_verticalZoom;
    QString m_documentErrors;
    void parseDocument(QDomDocument doc);
    int slotAddProjectTrack(int ix, QDomElement xml, bool locked);
    DocClipBase *getMissingProducer(const QString id) const;
    void adjustTrackHeaders();
    void slotAddProjectEffects(QDomNodeList effects, QDomElement parentNode, ClipItem *clip, int trackIndex);

private slots:
    void setCursorPos(int pos);
    void moveCursorPos(int pos);
    /** @brief Rebuilds the track headers */
    void slotRebuildTrackHeaders();
    /** @brief The tracks count or a track name changed, rebuild and notify */
    void slotReloadTracks();
    void slotChangeTrackLock(int ix, bool lock);
    void slotVerticalZoomDown();
    void slotVerticalZoomUp();

    /** @brief Changes the name of a track.
    * @param ix Number of the track
    * @param name New name */
    void slotRenameTrack(int ix, QString name);
    void slotRepaintTracks();

    /** @brief Adjusts the margins of the header area.
     *
     * Avoid a shift between header area and trackview if
     * the horizontal scrollbar is visible and the position
     * of the vertical scrollbar is maximal */
    void slotUpdateVerticalScroll(int min, int max);
    void slotShowTrackEffects(int);

signals:
    void mousePosition(int);
    void cursorMoved();
    void zoneMoved(int, int);
    void insertTrack(int);
    void deleteTrack(int);
    void configTrack(int);
    void updateTracksInfo();
    void setZoom(int);
    void showTrackEffects(int, TrackInfo);
};

#endif
