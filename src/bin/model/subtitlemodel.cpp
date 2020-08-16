#include "subtitlemodel.hpp"
#include "bin/bin.h"
#include "core.h"
#include "project/projectmanager.h"
#include "timeline2/model/snapmodel.hpp"

SubtitleModel::SubtitleModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_lock(QReadWriteLock::Recursive)
{
    //qDebug()<< "subtitle constructor";
    setup();
}

void SubtitleModel::setup()
{
    // We connect the signals of the abstractitemmodel to a more generic one.
    connect(this, &SubtitleModel::columnsMoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::columnsRemoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::columnsInserted, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::rowsMoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::rowsRemoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::rowsInserted, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::modelReset, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::dataChanged, this, &SubtitleModel::modelChanged);
}

std::shared_ptr<SubtitleModel> SubtitleModel::getModel()
{
    return pCore->projectManager()->getSubtitleModel();
}

void SubtitleModel::parseSubtitle()
{   qDebug()<<"Parsing started";
    //QModelIndex index;
    //int paramName = m_model->data(index, AssetParameterModel::NameRole).toInt();
    //QString filename(m_asset->get(paramName.toUtf8().constData()));
    QString filePath= "path_to_subtitle_file.srt";
    QString start,end,comment;
    QString timeLine;
    GenTime startPos, endPos;
    int turn = 0,r = 0;
    /*
     * turn = 0 -> Parse next subtitle line [srt] (or) Parse next section [ssa]
     * turn = 1 -> Add string to timeLine
     * turn > 1 -> Add string to completeLine
     */
    if (filePath.isEmpty())
        return;

    if (filePath.contains(".srt")) {
        QFile srtFile(filePath);
        if (!srtFile.exists() && !srtFile.open(QIODevice::ReadOnly)) {
            qDebug() << " File not found " << filePath;
            return;
        }
        qDebug()<< "srt File";
        //parsing srt file
        QTextStream stream(&srtFile);
        QString line;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (line.compare("")) {
                if (!turn) {
                    // index=atoi(line.toStdString().c_str());
                    turn++;
                    continue;
                }
                if (line.contains("-->")) {
                    timeLine += line;
                    QStringList srtTime;
                    srtTime = timeLine.split(' ');
                    start = srtTime[0];
                    startPos= stringtoTime(start);
                    end = srtTime[2];
                    startPos = stringtoTime(start);
                } else {
                    r++;
                    if (comment != "")
                        comment += " ";
                    if (r == 1)
                        comment += line;
                    else
                        comment = comment + "\r" +line;
                }
                turn++;
            } else {
                addSubtitle(startPos,endPos,comment);
                //reinitialize
                comment = timeLine = "";
                turn = 0; r = 0;
            }            
        }  
        srtFile.close();
    } else if (filePath.contains(".ass")) {
        qDebug()<< "ass File";
        QString startTime,endTime,text;
        QString EventFormat, section;
        turn = 0;
        int maxSplit =0;
        QFile assFile(filePath);
        if (!assFile.exists() || !assFile.open(QIODevice::ReadOnly)) {
            qDebug() << " Failed attempt on opening " << filePath;
            return;
        }
        QTextStream stream(&assFile);
        QString line;
        qDebug() << " correct ass file  " << filePath;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (line.compare("")) {
                if (!turn) {
                    //qDebug() << " turn = 0  " << line;
                    //check if it is script info, event,or v4+ style
                    if (line.replace(" ","").contains("ScriptInfo")) {
                        //qDebug()<< "Script Info";
                        section = "Script Info";
                        turn++;
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Styles")) {
                        //qDebug()<< "V4 Styles";
                        section = "V4 Styles";
                        turn++;
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else {
                        turn++;
                        section = "Events";
                        //qDebug()<< "turn" << turn;
                        continue;
                    }
                }
                //qDebug() << "\n turn != 0  " << turn<< line;
                if (section.contains("Events")) {
                    //if it is event
                    QStringList format;
                    if (line.contains("Format:")) {
                        EventFormat += line;
                        format = (EventFormat.split(": ")[1].replace(" ","")).split(',');
                        //qDebug() << format << format.count();
                        maxSplit = format.count();
                        //TIME
                        startTime = format[1];
                        endTime = format[2];
                        // Text
                        text = format[9];
                        //qDebug()<< startTime << endTime << text;
                    } else {
                        QString EventDialogue;
                        QStringList dialogue;
                        start = "";end = "";comment = "";
                        EventDialogue += line;
                        dialogue = EventDialogue.split(": ")[1].split(',');
                        QString remainingStr = "," + EventDialogue.split(": ")[1].section(',', maxSplit);
                        //qDebug()<< dialogue;
                        //TIME
                        start = dialogue[1];
                        startPos= stringtoTime(start);
                        end = dialogue[2];
                        endPos= stringtoTime(end);
                        // Text
                        comment = dialogue[9]+ remainingStr;
                        //qDebug()<<"Start: "<< start << "End: "<<end << comment;
                        addSubtitle(startPos,endPos,comment);
                    }
                }
                turn++;
            } else {
                turn = 0;
                text = startTime = endTime = "";
            }
        }
        assFile.close();
    }
}

GenTime SubtitleModel::stringtoTime(QString &str)
{
    QStringList total,secs;
    double hours = 0, mins = 0, seconds = 0, ms = 0;
    double total_sec = 0;
    GenTime pos;
    total = str.split(':');
    hours = atoi(total[0].toStdString().c_str());
    mins = atoi(total[1].toStdString().c_str());
    if(total[2].contains('.'))
        secs = total[2].split('.'); //ssa file
    else
        secs = total[2].split(','); //srt file
    seconds = atoi(secs[0].toStdString().c_str());
    ms = atoi(secs[1].toStdString().c_str());
    total_sec = hours *3600 + mins *60 + seconds + ms * 0.001 ;
    pos= GenTime(total_sec);
    return pos;
}

void SubtitleModel::addSubtitle(GenTime start, GenTime end, QString &str)
{
    auto model = getModel(); //gets model shared ptr
    //Q_ASSERT(model->m_subtitleList.count(start)==0); //returns warning if sub at start time position already exists ,i.e. count !=0
    if(m_subtitleList[start].first == str){
        qDebug()<<"already present in model"<<"string :"<<m_subtitleList[start].first<<" start time "<<start.frames(pCore->getCurrentFps())<<"end time : "<< m_subtitleList[start].second.frames(pCore->getCurrentFps());
        return;
    }
    if(model->m_subtitleList.count(start) > 0 ){
        qDebug()<<"Start time already in model";
        editSubtitle(start, str, end);
        return;
    }
    auto it= model->m_subtitleList.lower_bound(start); // returns the key and its value *just* greater than start.
    //Q_ASSERT(it->first < model->m_subtitleList.end()->second.second); // returns warning if added subtitle start time is less than last subtitle's end time
    int insertRow= static_cast<int>(model->m_subtitleList.size());//converts the returned unsigned size() to signed int
    /* For adding it in the middle of the list */
    if (it != model->m_subtitleList.end()) { // check if the subtitle greater than added subtitle is not the same as the last one
        insertRow = static_cast<int>(std::distance(model->m_subtitleList.begin(), it));
    }
    model->beginInsertRows(QModelIndex(), insertRow, insertRow);
    model->m_subtitleList[start] = {str, end};
    model->endInsertRows();
    model->addSnapPoint(start);
    qDebug()<<"Added to model";
}

QHash<int, QByteArray> SubtitleModel::roleNames() const 
{
    QHash<int, QByteArray> roles;
    roles[SubtitleRole] = "subtitle";
    roles[StartPosRole] = "startposition";
    roles[EndPosRole] = "endposition";
    roles[StartFrameRole] = "startframe";
    roles[EndFrameRole] = "endframe";
    return roles;
}

QVariant SubtitleModel::data(const QModelIndex& index, int role) const
{   
    if (index.row() < 0 || index.row() >= static_cast<int>(m_subtitleList.size()) || !index.isValid()) {
        return QVariant();
    }
    auto it = m_subtitleList.begin();
    std::advance(it, index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case SubtitleRole:
        return it->second.first;
    case StartPosRole:
        return it->first.seconds();
    case EndPosRole:
        return it->second.second.seconds();
    case StartFrameRole:
            return it->first.frames(pCore->getCurrentFps());
    case EndFrameRole:
        return it->second.second.frames(pCore->getCurrentFps());
    }
    return QVariant();
}

int SubtitleModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_subtitleList.size());
}

QList<SubtitledTime> SubtitleModel::getAllSubtitles() const
{
    QList<SubtitledTime> subtitle;
    for (const auto &subtitles : m_subtitleList) {
        SubtitledTime s(subtitles.first, subtitles.second.first, subtitles.second.second);
        subtitle << s;
    }
    return subtitle;
}

void SubtitleModel::registerSnap(const std::weak_ptr<SnapInterface> &snapModel)
{
    // make sure ptr is valid
    if (auto ptr = snapModel.lock()) {
        // ptr is valid, we store it
        m_regSnaps.push_back(snapModel);
        // we now add the already existing subtitles to the snap
        for (const auto &subtitle : m_subtitleList) {
            qDebug() << " REGISTERING SUBTITLE: " << subtitle.first.frames(pCore->getCurrentFps());
            ptr->addPoint(subtitle.first.frames(pCore->getCurrentFps()));
        }
    } else {
        qDebug() << "Error: added snapmodel for subtitle is null";
        Q_ASSERT(false);
    }
}

void SubtitleModel::addSnapPoint(GenTime startpos)
{
    std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    for (const auto &snapModel : m_regSnaps) {
        if (auto ptr = snapModel.lock()) {
            validSnapModels.push_back(snapModel);
            ptr->addPoint(startpos.frames(pCore->getCurrentFps()));
        }
    }
    // Update the list of snapModel known to be valid
    std::swap(m_regSnaps, validSnapModels);
}

void SubtitleModel::editEndPos(GenTime startPos, GenTime oldEndPos, GenTime newEndPos)
{
    auto model = getModel();
    if(oldEndPos == newEndPos) return;
    int row = static_cast<int>(std::distance(model->m_subtitleList.begin(), model->m_subtitleList.find(startPos)));
    model->m_subtitleList[startPos].second = newEndPos;
    emit model->dataChanged(model->index(row), model->index(row), QVector<int>() << EndPosRole);
    return;
}

void SubtitleModel::editSubtitle(GenTime startPos, QString newSubtitleText, GenTime endPos)
{
    qDebug()<<"Editing existing subtitle in model";
    auto model = getModel();
    int row = static_cast<int>(std::distance(model->m_subtitleList.begin(), model->m_subtitleList.find(startPos)));
    model->m_subtitleList[startPos].first = newSubtitleText ;
    model->m_subtitleList[startPos].second = endPos;
    qDebug()<<startPos.frames(pCore->getCurrentFps())<<m_subtitleList[startPos].first<<m_subtitleList[startPos].second.frames(pCore->getCurrentFps());
    emit model->dataChanged(model->index(row), model->index(row), QVector<int>() << SubtitleRole);
    return;
}

void SubtitleModel::removeSubtitle(GenTime pos)
{
    qDebug()<<"Deleting subtitle in model";
    auto model = getModel();
    if(model->m_subtitleList.count(pos) <= 0){
        qDebug()<<"No Subtitle at pos in model";
        return;
    }
    int row = static_cast<int>(std::distance(model->m_subtitleList.begin(), model->m_subtitleList.find(pos)));
    model->beginRemoveRows(QModelIndex(), row, row);
    model->m_subtitleList.erase(pos);
    model->endRemoveRows();
}

void SubtitleModel::moveSubtitle(GenTime oldPos, GenTime newPos)
{
    qDebug()<<"Moving Subtitle";
    auto model = getModel();
    if(model->m_subtitleList.count(oldPos) <= 0){
        //is not present in model only
        return;
    }
    QString subtitleText = model->m_subtitleList[oldPos].first ;
    GenTime endPos = model->m_subtitleList[oldPos].second;
    removeSubtitle(oldPos);
    addSubtitle(newPos, endPos, subtitleText);
    return;
}
