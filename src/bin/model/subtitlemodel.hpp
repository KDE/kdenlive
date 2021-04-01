/***************************************************************************
 *   Copyright (C) 2020 by Sashmita Raghav                                 *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef SUBTITLEMODEL_HPP
#define SUBTITLEMODEL_HPP

#include "bin/bin.h"
#include "definitions.h"
#include "gentime.h"
#include "undohelper.hpp"

#include <QAbstractListModel>
#include <QReadWriteLock>

#include <array>
#include <map>
#include <memory>
#include <unordered_set>
#include <mlt++/MltProperties.h>
#include <mlt++/Mlt.h>

class DocUndoStack;
class SnapInterface;
class AssetParameterModel;
class TimelineItemModel;

/** @brief This class is the model for a list of subtitles.
*/

class SubtitleModel:public QAbstractListModel
{
    Q_OBJECT

public:
    /** @brief Construct a subtitle list bound to the timeline */
    explicit SubtitleModel(Mlt::Tractor *tractor = nullptr, std::shared_ptr<TimelineItemModel> timeline = nullptr, QObject *parent = nullptr);

    enum { SubtitleRole = Qt::UserRole + 1, StartPosRole, EndPosRole, StartFrameRole, EndFrameRole, IdRole, SelectedRole, GrabRole };
    /** @brief Function that parses through a subtitle file */ 
    bool addSubtitle(int id, GenTime start,GenTime end, const QString str, bool temporary = false, bool updateFilter = true);
    bool addSubtitle(GenTime start, GenTime end, const QString str, Fun &undo, Fun &redo, bool updateFilter = true);
    /** @brief Converts string of time to GenTime */ 
    GenTime stringtoTime(QString &str);
    /** @brief Return model data item according to the role passed */ 
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;// override the same function of QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** @brief Returns all subtitles in the model */
    QList<SubtitledTime> getAllSubtitles() const;
    
    /** @brief Get subtitle at position */
    SubtitledTime getSubtitle(GenTime startFrame) const;
    /** @brief Returns all subtitle ids in a range */
    std::unordered_set<int> getItemsInRange(int startFrame, int endFrame) const;

    /** @brief Registers a snap model to the subtitle model */
    void registerSnap(const std::weak_ptr<SnapInterface> &snapModel);
    
    /** @brief Edit subtitle end timing
        @param startPos is start timing position of subtitles
        @param oldPos is the old position of the end time
        @param pos defines the new position of the end time
    */
    void editEndPos(GenTime startPos, GenTime newEndPos, bool refreshModel = true);
    bool requestResize(int id, int size, bool right);
    bool requestResize(int id, int size, bool right, Fun &undo, Fun &redo, bool logUndo);

    /** @brief Edit subtitle text
        @param startPos is start timing position of subtitles
        @param newSubtitleText is (new) subtitle text
    */
    void editSubtitle(GenTime startPos, QString newSubtitleText);

    /** @brief Remove subtitle at start position (pos) */
    bool removeSubtitle(int id, bool temporary = false, bool updateFilter = true);

    /** @brief Remove all subtitles from subtitle model */
    void removeAllSubtitles();
    
    /** @brief Update some properties in the view */
    void updateSub(int id, QVector <int> roles);

    /** @brief Move an existing subtitle
        @param subId is the subtitle's ID
        @param newPos is new start position of subtitle
    */
    bool moveSubtitle(int subId, GenTime newPos, bool updateModel, bool updateView);
    void requestSubtitleMove(int clipId, GenTime position);
    
    /** @brief Function that imports a subtitle file */
    void importSubtitle(const QString filePath, int offset = 0, bool externalImport = false);

    /** @brief Exports the subtitle model to json */
    QString toJson();
    /** @brief Returns the path to sub file */
    const QString getUrl();
    /** @brief Get a subtitle Id from its start position*/
    int getIdForStartPos(GenTime startTime) const;
    /** @brief Get the duration of a subtitle by id*/
    int getSubtitlePlaytime(int id) const;
    int getSubtitleEnd(int id) const;
    /** @brief Mark the subtitle item as selected or not*/
    void setSelected(int id, bool select);
    bool isSelected(int id) const;
    /** @brief Cut a subtitle */
    bool cutSubtitle(int position);
    bool cutSubtitle(int position, Fun &undo, Fun &redo);
    QString getText(int id) const;
    int getRowForId(int id) const;
    GenTime getStartPosForId(int id) const;
    int getPreviousSub(int id) const;
    int getNextSub(int id) const;
    /** @brief Copy subtitle file to a new path */
    void copySubtitle(const QString &path, bool checkOverwrite);
    int trackDuration() const;
    void switchDisabled();
    bool isDisabled() const;
    void switchLocked();
    bool isLocked() const;
    /** @brief Load some subtitle filter properties from file */
    void loadProperties(QMap<QString, QString> subProperties);
    /** @brief Add all subtitle items to snaps */
    void allSnaps(std::vector<int> &snaps);
    /** @brief Returns an xml representation of the subtitle with id \@sid */
    QDomElement toXml(int sid, QDomDocument &document);
    /** @brief Returns the size of the space between subtitles */
    int getBlankSizeAtPos(int pos) const;
    /** @brief Switch a subtitle's grab state */
    void switchGrab(int sid);
    /** @brief Ungrab all items */
    void clearGrab();
    /** @brief Release timeline model pointer */
    void unsetModel();

public slots:
    /** @brief Function that parses through a subtitle file */
    void parseSubtitle(const QString subPath = QString());
    
    /** @brief Import model to a temporary subtitle file to which the Subtitle effect is applied*/
    void jsontoSubtitle(const QString &data);
    /** @brief Update a subtitle text*/
    bool setText(int id, const QString text);

private:
    std::shared_ptr<TimelineItemModel> m_timeline;
    std::weak_ptr<DocUndoStack> m_undoStack;
    std::map<GenTime, std::pair<QString, GenTime>> m_subtitleList;

    QString scriptInfoSection, styleSection,eventSection;
    QString styleName;
    QString m_subFilePath;

    //To get subtitle file from effects parameter:
    //std::unique_ptr<Mlt::Properties> m_asset;
    //std::shared_ptr<AssetParameterModel> m_model;
    
    std::vector<std::weak_ptr<SnapInterface>> m_regSnaps;
    mutable QReadWriteLock m_lock;
    std::unique_ptr<Mlt::Filter> m_subtitleFilter;
    Mlt::Tractor *m_tractor;
    QVector <int> m_selected;
    QVector <int> m_grabbedIds;

signals:
    void modelChanged();
    
protected:
    /** @brief Add time as snap in the registered snap model */
    void addSnapPoint(GenTime startpos);
    /** @brief Remove time as snap in the registered snap model */
    void removeSnapPoint(GenTime startpos);
    /** @brief Connect changes in model with signal */
    void setup();

};
Q_DECLARE_METATYPE(SubtitleModel *)
#endif // SUBTITLEMODEL_HPP
