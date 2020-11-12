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
#include <mlt++/MltProperties.h>
#include <mlt++/Mlt.h>

class DocUndoStack;
class SnapInterface;
class AssetParameterModel;

/* @brief This class is the model for a list of subtitles.
*/

class SubtitleModel:public QAbstractListModel
{
    Q_OBJECT

public:
    /* @brief Construct a subtitle list bound to the timeline */
    explicit SubtitleModel(Mlt::Tractor *tractor = nullptr, QObject *parent = nullptr);

    enum { SubtitleRole = Qt::UserRole + 1, StartPosRole, EndPosRole, StartFrameRole, EndFrameRole };
    /** @brief Function that parses through a subtitle file */ 
    void addSubtitle(GenTime start,GenTime end, const QString str);
    /** @brief Converts string of time to GenTime */ 
    GenTime stringtoTime(QString &str);
    /** @brief Return model data item according to the role passed */ 
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;// overide the same function of QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** @brief Returns all subtitles in the model */
    QList<SubtitledTime> getAllSubtitles() const;
    
    /** @brief Get subtitle at position */
    SubtitledTime getSubtitle(GenTime startFrame) const;

    /** @brief Registers a snap model to the subtitle model */
    void registerSnap(const std::weak_ptr<SnapInterface> &snapModel);
    
    /** @brief Edit subtitle end timing
        @param startPos is start timing position of subtitles
        @param oldPos is the old position of the end time
        @param pos defines the new position of the end time
    */
    void editEndPos(GenTime startPos, GenTime newEndPos, bool refreshModel = true);

    /** @brief Edit subtitle , i.e. text and/or end time
        @param startPos is start timing position of subtitles
        @param newSubtitleText is (new) subtitle text
        @param endPos defines the (new) position of the end time
    */
    void editSubtitle(GenTime startPos, QString newSubtitleText, GenTime endPos);

    /** @brief Remove subtitle at start position (pos) */
    void removeSubtitle(GenTime pos);

    /** @brief Remove all subtitles from subtitle model */
    void removeAllSubtitles();

    /** @brief Move an existing subtitle
        @param oldPos is original start position of subtitle
        @param newPos is new start position of subtitle
    */
    void moveSubtitle(GenTime oldPos, GenTime newPos);
    
    /** @brief Function that imports a subtitle file */
    void importSubtitle(const QString filePath);

    /** @brief Exports the subtitle model to json */
    QString toJson();

public slots:
    /** @brief Function that parses through a subtitle file */
    void parseSubtitle(const QString subPath = QString());
    
    /** @brief Import model to a temporary subtitle file to which the Subtitle effect is applied*/
    void jsontoSubtitle(const QString &data, QString updatedFileName = QString());

private:
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

signals:
    void modelChanged();
    
protected:
    /** @brief Helper function that retrieves a pointer to the subtitle model*/
    static std::shared_ptr<SubtitleModel> getModel();
    /** @brief Add start time as snap in the registered snap model */
    void addSnapPoint(GenTime startpos);
    /** @brief Connect changes in model with signal */
    void setup();

};
Q_DECLARE_METATYPE(SubtitleModel *)
#endif // SUBTITLEMODEL_HPP
