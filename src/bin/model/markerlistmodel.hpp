/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "undohelper.hpp"
#include "utils/gentime.h"

#include <QAbstractListModel>
#include <QReadWriteLock>

#include <array>
#include <map>
#include <memory>

class ProjectClip;
class DocUndoStack;
class SnapInterface;

/** @class MarkerListModel
    @brief This class is the model for a list of markers.
    A marker is defined by a time, a type (the color used to represent it) and a comment string.
    We store them in a sorted fashion using a std::map

    A marker is essentially bound to a clip.
 */
class MarkerListModel : public QAbstractListModel, public enable_shared_from_this_virtual<MarkerListModel>
{
    Q_OBJECT

    friend class ClipController;
    friend class KdenliveDoc;

public:
    /** @brief Construct a marker list bound to the bin clip with given id */
    explicit MarkerListModel(QString clipId, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent = nullptr);

    enum { CommentRole = Qt::UserRole + 1, PosRole, FrameRole, ColorRole, TypeRole, IdRole, TCRole };

    /** @brief Adds a marker at the given position. If there is already one, the comment will be overridden
       @param pos defines the position of the marker, relative to the clip
       @param comment is the text associated with the marker
       @param type is the type (color) associated with the marker. If -1 is passed, then the value is pulled from kdenlive's defaults
     */
    bool addMarker(GenTime pos, const QString &comment, int type = -1);
    bool addMarkers(const QMap<GenTime, QString> &markers, int type = -1);

protected:
    /** @brief Same function but accumulates undo/redo */
    bool addMarker(GenTime pos, const QString &comment, int type, Fun &undo, Fun &redo);

public:
    /** @brief Removes the marker at the given position.
       Returns false if no marker was found at given pos
     */
    bool removeMarker(GenTime pos);
    /** @brief Delete all the markers of the model */
    bool removeAllMarkers();

    /** @brief Same function but accumulates undo/redo */
    bool removeMarker(GenTime pos, Fun &undo, Fun &redo);

public:
    /** @brief Edit a marker
       @param oldPos is the old position of the marker
       @param pos defines the new position of the marker, relative to the clip
       @param comment is the text associated with the marker
       @param type is the type (color) associated with the marker. If -1 is passed, then the value is pulled from kdenlive's defaults
    */
    bool editMarker(GenTime oldPos, GenTime pos, QString comment = QString(), int type = -1);

    /** @brief Moves all markers from on to another position
       @param markers list of markers to move
       @param fromPos
       @param toPos
       @param undo
       @param redo
    */
    bool moveMarkers(const QList<CommentedTime> &markers, GenTime fromPos, GenTime toPos, Fun &undo, Fun &redo);
    bool moveMarker(int mid, GenTime pos);
    void moveMarkersWithoutUndo(const QVector<int> &markersId, int offset, bool updateView = true);

    /** @brief Returns a marker data at given pos */
    CommentedTime getMarker(const GenTime &pos, bool *ok) const;
    CommentedTime getMarker(int frame, bool *ok) const;

    /** @brief Returns all markers in model or – if a type is given – all markers of the given type */
    QList<CommentedTime> getAllMarkers(int type = -1) const;

    /** @brief Returns all markers of model that are intersect with a given range.
     * @param start is the position where start to search for markers
     * @param end is the position after which markers will not be returned, set to -1 to get all markers after start
     */
    QList<CommentedTime> getMarkersInRange(int start, int end) const;
    QVector<int> getMarkersIdInRange(int start, int end) const;

    /** @brief Returns a marker position in frames given it's id */
    int getMarkerPos(int mid) const;

    /** @brief Returns all markers positions in model */
    std::vector<int> getSnapPoints() const;

    /** @brief Returns true if a marker exists at given pos
       Notice that add/remove queries are done in real time (gentime), but this request is made in frame
     */
    Q_INVOKABLE bool hasMarker(int frame) const;
    /** @brief Returns a marker id at frame pos. Returns -1 if no marker exists at that position
     */
    int markerIdAtFrame(int pos) const;
    bool hasMarker(GenTime pos) const;
    CommentedTime marker(GenTime pos) const;
    CommentedTime marker(int frame) const;
    CommentedTime markerById(int mid) const;

    /** @brief Registers a snapModel to the marker model.
       This is intended to be used for a guide model, so that the timelines can register their snapmodel to be updated when the guide moves. This is also used
       on the clip monitor to keep tracking the clip markers
       The snap logic for clips is managed from the Timeline
       Note that no deregistration is necessary, the weak_ptr will be discarded as soon as it becomes invalid.
    */
    void registerSnapModel(const std::weak_ptr<SnapInterface> &snapModel);

    /** @brief Exports the model to json using format above
     *  @param categories will only export selected categories. If param is empty, all categories will be exported
     */
    QString toJson(QList<int> categories = {}) const;

    /** @brief Shows a dialog to edit a marker/guide
       @param pos: position of the marker to edit, or new position for a marker
       @param widget: qt widget that will be the parent of the dialog
       @param createIfNotFound: if true, we create a marker if none is found at pos
       @param clip: pointer to the clip if we are editing a marker
       @return true if dialog was accepted and modification successful
     */
    bool editMarkerGui(const GenTime &pos, QWidget *parent, bool createIfNotFound, ProjectClip *clip = nullptr, bool createOnly = false);
    /** @brief Shows a dialog to change the category of multiple markers/guides
       @param positions: List of the markers positions to edit
       @param widget: qt widget that will be the parent of the dialog
       @return true if dialog was accepted and modification successful
     */
    bool editMultipleMarkersGui(const QList<GenTime> positions, QWidget *parent);
    /** @brief Shows a dialog to add multiple markers/guide
       @param pos: position of the marker to edit, or new position for a marker
       @param widget: qt widget that will be the parent of the dialog
       @param createIfNotFound: if true, we create a marker if none is found at pos
       @param clip: pointer to the clip if we are editing a marker
       @return true if dialog was accepted and modification successful
     */
    bool addMultipleMarkersGui(const GenTime &pos, QWidget *parent, bool createIfNotFound, ProjectClip *clip = nullptr);
    void exportGuidesGui(QWidget *parent, GenTime projectDuration) const;
    /** @brief Load the marker categories from a stringList
     *  @return the list of deleted categories ids (if any)
     */
    void loadCategoriesWithUndo(const QStringList &categories, const QStringList &currentCategories, const QMap<int, int> remapCategories = {});
    QList<int> loadCategories(const QStringList &categories, bool notify = true);
    QStringList guideCategoriesToStringList(const QString &categoriesData);
    /** @brief Returns the marker categories in the form of a stringList for saving */
    const QStringList categoriesToStringList() const;
    const QString categoriesToJSon() const;
    static const QString categoriesListToJSon(const QStringList categories);

    // Mandatory overloads
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public Q_SLOTS:
    /** @brief Imports a list of markers from json data
   The data should be formatted as follows:
   [{"pos":0.2, "comment":"marker 1", "type":1}, {...}, ...]
   return true on success and logs undo object
   @param ignoreConflicts: if set to false, it aborts if the data contains a marker with same position but different comment and/or type. If set to true,
   such markers are overridden silently
   @param pushUndo: if true, create an undo object
 */
    bool importFromJson(const QString &data, bool ignoreConflicts, bool pushUndo = true);
    bool importFromJson(const QString &data, bool ignoreConflicts, Fun &undo, Fun &redo);
    bool importFromTxt(const QString &fileName, Fun &undo, Fun &redo);
    bool importFromFile(const QString &fileData, bool ignoreConflicts);

protected:
    /** @brief Adds a snap point at marker position in the registered snap models
     (those that are still valid)*/
    void addSnapPoint(GenTime pos);

    /** @brief Deletes a snap point at marker position in the registered snap models
       (those that are still valid)*/
    void removeSnapPoint(GenTime pos);

    /** @brief Helper function that generate a lambda to change comment / type of given marker */
    Fun changeComment_lambda(GenTime pos, const QString &comment, int type);

    /** @brief Helper function that generate a lambda to add given marker */
    Fun addMarker_lambda(GenTime pos, const QString &comment, int type);

    /** @brief Helper function that generate a lambda to remove given marker */
    Fun deleteMarker_lambda(GenTime pos);

    /** @brief Helper function that retrieves a pointer to the markermodel, given whether it's a guide model and its clipId*/
    std::shared_ptr<MarkerListModel> getModel(const QString &clipId);

    /** @brief Connects the signals of this object */
    void setup();

private:
    std::weak_ptr<DocUndoStack> m_undoStack;
    /** @brief the Id of the clip this model corresponds to. */
    QString m_clipId;

    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;

    std::map<int, CommentedTime> m_markerList;
    /** @brief A list of {marker frame,marker id}, useful to quickly find a marker */
    QMap<int, int> m_markerPositions;

    std::vector<std::weak_ptr<SnapInterface>> m_registeredSnaps;
    int getRowfromId(int mid) const;
    int getIdFromPos(const GenTime &pos) const;
    int getIdFromPos(int frame) const;

Q_SIGNALS:
    void modelChanged();
    void categoriesChanged();
};
Q_DECLARE_METATYPE(MarkerListModel *)
