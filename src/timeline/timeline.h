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
* @class Timeline
* @brief Manages the timline
* @author Jean-Baptiste Mardelle
*/

#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include "timeline/customtrackscene.h"
#include "effectslist/effectslist.h"
#include "ui_timeline_ui.h"
#include "definitions.h"

#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QDomElement>

#include <mlt++/Mlt.h>

class Track;
class ClipItem;
class CustomTrackView;
class KdenliveDoc;
class TransitionHandler;
class CustomRuler;
class QUndoCommand;
class PreviewManager;

class ScrollEventEater : public QObject
{
    Q_OBJECT
public:
    explicit ScrollEventEater(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;
};

class Timeline : public QWidget, public Ui::TimeLine_UI
{
    Q_OBJECT

public:
    explicit Timeline(KdenliveDoc *doc, const QList<QAction *> &actions, const QList<QAction *> &rulerActions, bool *ok, QWidget *parent = nullptr);
    virtual ~ Timeline();

    /** @brief is multitrack view (split screen for tracks) enabled */
    bool multitrackView;
    int videoTarget;
    int audioTarget;
    Track *track(int i);
    /** @brief Number of tracks in the MLT playlist. */
    int tracksCount() const;
    /** @brief Number of visible tracks (= tracksCount() - 1 ) because black trck is not visible to user. */
    int visibleTracksCount() const;
    void setEditMode(const QString &editMode);
    const QString &editMode() const;
    QGraphicsScene *projectScene();
    CustomTrackView *projectView();
    int duration() const;
    KdenliveDoc *document();
    void refresh();
    int outPoint() const;
    int inPoint() const;
    int fitZoom() const;
    /** @brief This object handles all transition operation. */
    TransitionHandler *transitionHandler;
    void lockTrack(int ix, bool lock);
    bool isTrackLocked(int ix);
    /** @brief Dis / enable video for a track. */
    void doSwitchTrackVideo(int ix, bool hide);
    /** @brief Dis / enable audio for a track. */
    void doSwitchTrackAudio(int ix, bool mute);

    /** @brief Updates (redraws) the ruler.
    *
    * Used to change from displaying frames to timecode or vice versa. */
    void updateRuler();

    /** @brief Parse tracks to see if project has audio in it.
    *
    * Parses all tracks to check if there is audio data. */
    bool checkProjectAudio();

    /** @brief Load guides from data */
    void loadGuides(const QMap<double, QString> &guidesData);

    void checkTrackHeight(bool force = false);
    void updatePalette();
    void refreshIcons();
    /** @brief Returns a kdenlive effect xml description from an effect tag / id */
    static QDomElement getEffectByTag(const QString &effecttag, const QString &effectid);
    /** @brief Move a clip between tracks */
    bool moveClip(int startTrack, qreal startPos, int endTrack, qreal endPos, PlaylistState::ClipState state, TimelineMode::EditMode mode, bool duplicate);
    void renameTrack(int ix, const QString &name);
    void updateTrackState(int ix, int state);
    /** @brief Returns info about a track.
     *  @param ix The track number in MLT's coordinates (0 = black track, 1 = bottom audio, etc)
     *  deprecated use string version with track name instead */
    TrackInfo getTrackInfo(int ix);
    void setTrackInfo(int trackIndex, const TrackInfo &info);
    QList<TrackInfo> getTracksInfo();
    QStringList getTrackNames();
    void addTrackEffect(int trackIndex, QDomElement effect, bool addToPlaylist = true);
    bool removeTrackEffect(int trackIndex, int effectIndex, const QDomElement &effect);
    void setTrackEffect(int trackIndex, int effectIndex, QDomElement effect);
    bool enableTrackEffects(int trackIndex, const QList<int> &effectIndexes, bool disable);
    const EffectsList getTrackEffects(int trackIndex);
    QDomElement getTrackEffect(int trackIndex, int effectIndex);
    int hasTrackEffect(int trackIndex, const QString &tag, const QString &id);
    MltVideoProfile mltProfile() const;
    double fps() const;
    QPoint getTracksCount();
    /** @brief Check if we have a blank space on selected track.
     *  Returns -1 if track is shorter, 0 if not blank and > 0 for blank length */
    int getTrackSpaceLength(int trackIndex, int pos, bool fromBlankStart);
    void updateClipProperties(const QString &id, const QMap<QString, QString> &properties);
    int changeClipSpeed(const ItemInfo &info, const ItemInfo &speedIndependantInfo, PlaylistState::ClipState state, double speed, int strobe, Mlt::Producer *originalProd, bool removeEffect = false);
    /** @brief Set an effect's XML accordingly to MLT::filter values. */
    static void setParam(ProfileInfo info, QDomElement param, const QString &value);
    int getTracks();
    void getTransitions();
    void refreshTractor();
    void duplicateClipOnPlaylist(int tk, qreal startPos, int offset, Mlt::Producer *prod);
    int getSpaceLength(const GenTime &pos, int tk, bool fromBlankStart);
    void blockTrackSignals(bool block);
    /** @brief Load document */
    void loadTimeline();
    /** @brief Dis/enable all effects in timeline*/
    void disableTimelineEffects(bool disable);
    QString getKeyframes(Mlt::Service service, int &ix, const QDomElement &e);
    void getSubfilters(Mlt::Filter *effect, QDomElement &currenteffect);
    static bool isSlide(QString geometry);
    /** @brief Import amultitrack MLT playlist in timeline */
    void importPlaylist(const ItemInfo &info, const QMap<QString, QString> &idMaps, const QDomDocument &doc, QUndoCommand *command);
    /** @brief Creates an overlay track with a filtered clip */
    bool createOverlay(Mlt::Filter *filter, int tk, int startPos);
    /** @brief Creates an overlay track with a ripple transition*/
    bool createRippleWindow(int tk, int startPos, OperationType mode);
    void removeSplitOverlay();
    /** @brief Temporarily add/remove track before saving */
    void connectOverlayTrack(bool enable);
    /** @brief Update composite and mix transitions's tracks */
    void refreshTransitions();
    /** @brief Switch current track target state */
    void switchTrackTarget();
    /** @brief Refresh Header Leds */
    void updateHeaders();
    /** @brief Returns true if position is on the last clip */
    bool isLastClip(const ItemInfo &info);
    /** @brief find lowest video track in timeline. */
    int getLowestVideoTrack();
    /** @brief Returns the document properties with some added values from timeline. */
    QMap<QString, QString> documentProperties();
    void reloadTrack(int ix, int start = 0, int end = -1);
    void reloadTrack(const ItemInfo &info, bool includeLastFrame);
    /** @brief Add or remove current timeline zone to preview render zone. */
    void addPreviewRange(bool add);
    /** @brief Resets all preview render zones. */
    void clearPreviewRange();
    /** @brief Check if timeline preview profile changed and remove preview files if necessary. */
    void updatePreviewSettings(const QString &profile);
    /** @brief invalidate timeline preview for visible clips in a track */
    void invalidateTrack(int ix);
    /** @brief Start rendering preview rendering range. */
    void startPreviewRender();
    /** @brief Toggle current project's compositing mode. */
    void switchComposite(int mode);
    /** @brief Temporarily hide a clip if it is at cursor position so that we can extract an image. 
     *  @returns true if a track was temporarily hidden
    */
    bool hideClip(const QString &id, bool hide);

public slots:
    void slotDeleteClip(const QString &clipId, QUndoCommand *deleteCommand);
    void slotChangeZoom(int horizontal, int vertical = -1, bool zoomOnMouse = false);
    void setDuration(int dur);
    void slotSetZone(const QPoint &p, bool updateDocumentProperties = true);
    /** @brief Save a snapshot image of current timeline view */
    void slotSaveTimelinePreview(const QString &path);
    void checkDuration();
    void slotShowTrackEffects(int);
    void updateProfile(double fpsChanged);
    /** @brief Enable/disable multitrack view (split monitor in 4) */
    void slotMultitrackView(bool enable);
    /** @brief Stop rendering preview. */
    void stopPreviewRender();
    /** @brief Invalidate a preview rendering range. */
    void invalidateRange(const ItemInfo &info = ItemInfo());

private:
    Mlt::Tractor *m_tractor;
    QList<Track *> m_tracks;
    /** @brief number of special overlay tracks to preview effects */
    bool m_hasOverlayTrack;
    Mlt::Producer *m_overlayTrack;
    CustomRuler *m_ruler;
    CustomTrackView *m_trackview;
    QList<QString> m_invalidProducers;
    double m_scale;
    QString m_editMode;
    CustomTrackScene *m_scene;
    /** @brief A list of producer ids to be replaced when opening a corrupted document*/
    QMap<QString, QString> m_replacementProducerIds;

    KdenliveDoc *m_doc;
    int m_verticalZoom;
    QString m_documentErrors;
    QList<QAction *> m_trackActions;
    /** @brief sometimes grouped commands quickly send invalidate commands, so wait a little bit before processing*/
    PreviewManager *m_timelinePreview;
    bool m_usePreview;
    QAction *m_disablePreview;

    void adjustTrackHeaders();

    void parseDocument(const QDomDocument &doc);
    int loadTrack(int ix, int offset, Mlt::Playlist &playlist, int start = 0, int end = -1, bool updateReferences = true);
    void getEffects(Mlt::Service &service, ClipItem *clip, int track = 0);
    void adjustDouble(QDomElement &e, const QString &value);

    /** @brief Adjust kdenlive effect xml parameters to the MLT value*/
    void adjustparameterValue(QDomNodeList clipeffectparams, const QString &paramname, const QString &paramvalue);
    /** @brief Enable/disable track actions depending on number of tracks */
    void refreshTrackActions();
    /** @brief load existing timeline previews */
    void loadPreviewRender();
    void initializePreview();

private slots:
    void setCursorPos(int pos);
    void moveCursorPos(int pos);
    /** @brief The tracks count or a track name changed, rebuild and notify */
    void slotReloadTracks();
    void slotVerticalZoomDown();
    void slotVerticalZoomUp();
    /** @brief Changes the name of a track.
    * @param ix Number of the track
    * @param name New name */
    void slotRenameTrack(int ix, const QString &name);
    void slotRepaintTracks();

    /** @brief Adjusts the margins of the header area.
     *
     * Avoid a shift between header area and trackview if
     * the horizontal scrollbar is visible and the position
     * of the vertical scrollbar is maximal */
    void slotUpdateVerticalScroll(int min, int max);
    /** @brief Update the track label showing applied effects.*/
    void slotUpdateTrackEffectState(int);
    /** @brief Toggle use of timeline zone for editing.*/
    void slotEnableZone(bool enable);
    /** @brief Dis / enable video for a track. */
    void switchTrackVideo(int ix, bool hide);
    /** @brief Dis / enable audio for a track. */
    void switchTrackAudio(int ix, bool mute);
    /** @brief Dis / enable timeline preview. */
    void disablePreview(bool disable);
    /** @brief Resize ruler layout to adjust for timeline preview. */
    void resizeRuler(int height);
    /** @brief The timeline track headers were resized, store width. */
    void storeHeaderSize(int pos, int index);

signals:
    void mousePosition(int);
    void cursorMoved();
    void zoneMoved(int, int);
    void configTrack();
    void updateTracksInfo();
    void setZoom(int);
    void showTrackEffects(int, const TrackInfo &);
    /** @brief Indicate how many clips we are going to load */
    void startLoadingBin(int);
    /** @brief Indicate which clip we are currently loading */
    void loadingBin(int);
    /** @brief We are about to reload timeline, reset bin clip usage */
    void resetUsageCount();
    void displayMessage(const QString &, MessageType);
};

#endif
