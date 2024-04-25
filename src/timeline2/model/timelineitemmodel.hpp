/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/keyframes/model/keyframemodellist.hpp"
#include "timelinemodel.hpp"
#include "undohelper.hpp"

class MarkerListModel;

/** @class TimelineItemModel
    @brief This class is the thin wrapper around the TimelineModel that provides interface for the QML.

   It derives from AbstractItemModel to provide the model to the QML interface. An itemModel is organized with row and columns that contain the data. It can be
   hierarchical, meaning that a given index (row,column) can contain another level of rows and column.
   Our organization is as follows: at the top level, each row contains a track. These rows are in the same order as in the actual timeline.
   Then each of this row contains itself sub-rows that correspond to the clips.
   Here the order of these sub-rows is unrelated to the chronological order of the clips,
   but correspond to their Id order. For example, if you have three clips, with ids 12, 45 and 150, they will receive row index 0,1 and 2.
   This is because the order actually doesn't matter since the clips are rendered based on their positions rather than their row order.
   The id order has been chosen because it is consistent with a valid ordering of the clips.
   The columns are never used, so the data is always in column 0

   An ModelIndex in the ItemModel consists of a row number, a column number, and a parent index. In our case, tracks have always an empty parent, and the clip
   have a track index as parent.
   A ModelIndex can also store one additional integer, and we exploit this feature to store the unique ID of the object it corresponds to.
   */
class TimelineItemModel : public TimelineModel
{
    Q_OBJECT

public:
    /** @brief construct a timeline object and returns a pointer to the created object
       @param undo_stack is a weak pointer to the undo stack of the project
       @param guideModel ptr to the guide model of the project
     */
    static std::shared_ptr<TimelineItemModel> construct(const QUuid &uuid, std::weak_ptr<DocUndoStack> undo_stack);

    friend bool constructTimelineFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, Mlt::Tractor tractor);

protected:
    /** @brief this constructor should not be called. Call the static construct instead
     */
    TimelineItemModel(const QUuid &uuid, std::weak_ptr<DocUndoStack> undo_stack);

public:
    ~TimelineItemModel() override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    // QModelIndex makeIndex(int trackIndex, int clipIndex) const;

    /** @brief Add multiple tracks at a specific position (used by the Add Track
     * dialog). Returns true if successful, false otherwise. */
    bool addTracksAtPosition(int position, int tracksCount, QString &trackName, bool addAudioTrack, bool addAVTrack, bool addRecTrack);

    /** @brief Creates an index based on the ID of the clip*/
    QModelIndex makeClipIndexFromID(int clipId) const override;
    /** @brief Creates an index based on the ID of the compoition*/
    QModelIndex makeCompositionIndexFromID(int compoId) const override;
    void subtitleChanged(int subId, const QVector<int> &roles);
    /** @brief Creates an index based on the ID of the track*/
    QModelIndex makeTrackIndexFromID(int trackId) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    Q_INVOKABLE void setTrackProperty(int tid, const QString &name, const QString &value);
    /** @brief Enabled/disabled a track's effect stack */
    Q_INVOKABLE void setTrackStackEnabled(int tid, bool enable);
    Q_INVOKABLE QVariant getTrackProperty(int tid, const QString &name) const;
    /** @brief Sets a track name
       @param trackId is of the track to alter
       @param text is the new track name.
    */
    Q_INVOKABLE void setTrackName(int trackId, const QString &text);
    Q_INVOKABLE bool copyClipEffect(int clipId, const QString sourceId);
    /** @brief returns the lower video track index in timeline.
     **/
    int getFirstVideoTrackIndex() const;
    int getFirstAudioTrackIndex() const;
    const QString getTrackFullName(int tid) const;
    void notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, bool start, bool duration, bool updateThumb) override;
    void notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, const QVector<int> &roles) override;
    void notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, int role) override;

    /** @brief Import track effects */
    void importTrackEffects(int tid, std::weak_ptr<Mlt::Service> service);
    /** @brief Save the sequence properties in MLT tractor */
    void passSequenceProperties(const QMap<QString, QString> baseProperties);
    /** @brief Return the count of items sharing a same effect in a group */
    int clipAssetGroupInstances(int cid, const QString &assetId);
    void applyClipAssetGroupCommand(int cid, const QString &assetId, const QModelIndex &index, const QString &previousValue, QString value,
                                    QUndoCommand *command);
    void applyClipAssetGroupKeyframeCommand(int cid, const QString &assetId, const QModelIndex &index, GenTime pos, const QVariant &previousValue,
                                            const QVariant &value, int ix, QUndoCommand *command);
    void applyClipAssetGroupMultiKeyframeCommand(int cid, const QString &assetId, const QList<QModelIndex> &indexes, GenTime pos,
                                                 const QStringList &sourceValues, const QStringList &values, QUndoCommand *command);
    QList<std::shared_ptr<KeyframeModelList>> getGroupKeyframeModels(int cid, const QString &assetId);
    void removeEffectFromGroup(int cid, const QString &assetId);
    void disableEffectFromGroup(int cid, const QString &assetId, bool disable, Fun &undo, Fun &redo);

    const QString groupsData();
    bool loadGroups(const QString &groupsData);
    std::shared_ptr<SubtitleModel> createSubtitleModel();
    void processTimelineReplacement(QList<int> instances, const QString &originalId, const QString &replacementId, int maxDuration, bool replaceAudio,
                                    bool replaceVideo);

    /** @brief Rebuild track compositing */
    void buildTrackCompositing(bool rebuild = false) override;
    /** @brief Register all tracks in the mixer */
    void rebuildMixer();
    void _beginRemoveRows(const QModelIndex & /*unused*/, int /*unused*/, int /*unused*/) override;
    void _beginInsertRows(const QModelIndex & /*unused*/, int /*unused*/, int /*unused*/) override;
    void _endRemoveRows() override;
    void _endInsertRows() override;
    void _resetView() override;

protected:
    /** @brief This is an helper function that finishes a construction of a freshly created TimelineItemModel */
    static void finishConstruct(const std::shared_ptr<TimelineItemModel> &ptr);

Q_SIGNALS:
    /** @brief Triggered when a video track visibility changed */
    void trackVisibilityChanged();
    void showTrackEffectStack(int tid);
    void subtitleModelInitialized();
    void subtitlesListChanged();
};
