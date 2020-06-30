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

    enum { SubtitleRole = Qt::UserRole + 1, StartPosRole, EndPosRole, StartFrameRole, EndFrameRole };
    /** @brief Function that parses through a subtitle file */ 
    void parseSubtitle();
    void addSubtitle(GenTime start,GenTime end, QString str);
    GenTime stringtoTime(QString str);
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;// overide the same function of QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** @brief Returns all subtitles in the model */
    QList<SubtitledTime> getAllSubtitles() const;
    
private:
    std::weak_ptr<DocUndoStack> m_undoStack;
    std::map<GenTime, std::pair<QString, GenTime>> m_subtitleList;
    //To get subtitle file from effects parameter:
    //std::unique_ptr<Mlt::Properties> m_asset;
    //std::shared_ptr<AssetParameterModel> m_model;

protected:
    /** @brief Helper function that retrieves a pointer to the subtitle model*/
    static std::shared_ptr<SubtitleModel> getModel();
    
};
Q_DECLARE_METATYPE(SubtitleModel *)
#endif // SUBTITLEMODEL_HPP