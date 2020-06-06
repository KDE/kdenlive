#ifndef SUBTITLEMODEL_HPP
#define SUBTITLEMODEL_HPP

#include "definitions.h"
#include "gentime.h"
#include "undohelper.hpp"

#include <QAbstractListModel>
#include <QReadWriteLock>

#include <array>
#include <map>
#include <memory>
#include <mlt++/MltProperties.h>

class DocUndoStack;
class AssetParameterModel;

/* @brief This class is the model for a list of subtitles.
*/

class SubtitleModel:public QAbstractListModel
{
    Q_OBJECT

public:
    /* @brief Construct a subtitle list bound to the timeline */
    SubtitleModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent = nullptr);
    
    /** @brief Function that parses through a subtitle file */ 
    void parseSubtitle();
    void addSubtitle(int ix,GenTime start,GenTime end, QString str);
    GenTime stringtoTime(QString str);

private:
    std::weak_ptr<DocUndoStack> m_undoStack;
    std::map<GenTime, std::pair<QString, GenTime>> m_subtitleList;
    //To get subtitle file from effects parameter:
    //std::unique_ptr<Mlt::Properties> m_asset;
    //std::shared_ptr<AssetParameterModel> m_model;

};
Q_DECLARE_METATYPE(SubtitleModel *)
#endif // SUBTITLEMODEL_HPP