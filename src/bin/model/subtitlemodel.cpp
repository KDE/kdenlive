#include "subtitlemodel.hpp"
#include "core.h"
#include "project/projectmanager.h"


SubtitleModel::SubtitleModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
{

}

std::shared_ptr<SubtitleModel> SubtitleModel::getModel()
{
    return pCore->projectManager()->getSubtitleModel();
}
void SubtitleModel::parseSubtitle()
{
    //QModelIndex index;
    //int paramName = m_model->data(index, AssetParameterModel::NameRole).toInt();
    //QString filename(m_asset->get(paramName.toUtf8().constData()));
    QString filePath= "path_to_subtitle_file.srt";
    QString start,end,comment;
    GenTime startPos, endPos;
    QString timeLine;
    int index = 0, turn = 0,r = 0;
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
                    index=atoi(line.toStdString().c_str());
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
                    qDebug() << " turn = 0  " << line;
                    //check if it is script info, event,or v4+ style
                    if (line.replace(" ","").contains("ScriptInfo")) {
                        qDebug()<< "Script Info";
                        section = "Script Info";
                        turn++;
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Styles")) {
                        qDebug()<< "V4 Styles";
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
                qDebug() << "\n turn != 0  " << turn<< line;
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
    }
}

GenTime SubtitleModel::stringtoTime(QString str)
{
    QStringList total,secs;
    double hours = 0, mins = 0, seconds = 0, ms = 0;
    double total_sec = 0;
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
    GenTime pos= GenTime(total_sec);
    return pos;
}

void SubtitleModel::addSubtitle(GenTime start, GenTime end, QString str)
{
    auto model = getModel(); //gets model shared ptr
    Q_ASSERT(model->m_subtitleList.count(start)==0); //returns warning if sub at start time position already exists ,i.e. count !=0
    auto it= model->m_subtitleList.lower_bound(start); // returns the key and its value *just* greater than start.
    Q_ASSERT(it->first < model->m_subtitleList.end()->second.second); // returns warning if added subtitle start time is less than last subtitle's end time
    int insertRow= static_cast<int>(model->m_subtitleList.size());//converts the returned unsigned size() to signed int
    /* For adding it in the middle of the list */
    if (it != model->m_subtitleList.end()) { // check if the subtitle greater than added subtitle is not the same as the last one
        insertRow = static_cast<int>(std::distance(model->m_subtitleList.begin(), it));
    }
    model->beginInsertRows(QModelIndex(), insertRow, insertRow);
    model->m_subtitleList[start] = {str, end};
    model->endInsertRows();
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
    case Qt::UserRole:
        return it->first.frames(pCore->getCurrentFps());
    case EndFrameRole:
        return it->second.second.frames(pCore->getCurrentFps());
    }
    return QVariant();
}