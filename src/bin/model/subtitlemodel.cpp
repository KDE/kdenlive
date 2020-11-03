#include "subtitlemodel.hpp"
#include "bin/bin.h"
#include "core.h"
#include "project/projectmanager.h"
#include "timeline2/model/snapmodel.hpp"
#include "profiles/profilemodel.hpp"
#include <mlt++/MltProperties.h>
#include <mlt++/Mlt.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

SubtitleModel::SubtitleModel(Mlt::Tractor *tractor, QObject *parent)
    : QAbstractListModel(parent)
    , m_tractor(tractor)
    , m_lock(QReadWriteLock::Recursive)
{
    qDebug()<< "subtitle constructor";
    m_subtitleFilter = nullptr;
    m_subtitleFilter.reset(new Mlt::Filter(pCore->getCurrentProfile()->profile(), "avfilter.subtitles"));
    qDebug()<<"Filter!";
    if (tractor != nullptr) {
        qDebug()<<"Tractor!";
        m_tractor->attach(*m_subtitleFilter.get());
    }
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
{   
	qDebug()<<"Parsing started";
    QString filePath; //"path_to_subtitle_file.srt";
    m_subFilePath = filePath;
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
                    QString linespace = line;
                    if (linespace.replace(" ","").contains("ScriptInfo")) {
                        //qDebug()<< "Script Info";
                        section = "Script Info";
                        scriptInfoSection += line+"\n";
                        turn++;
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Styles")) {
                        //qDebug()<< "V4 Styles";
                        section = "V4 Styles";
                        styleSection += line + "\n";
                        turn++;
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else {
                        turn++;
                        section = "Events";
                        eventSection += line +"\n";
                        //qDebug()<< "turn" << turn;
                        continue;
                    }
                }
                if (section.contains("Script Info")) {
                    scriptInfoSection += line + "\n";
                }
                if (section.contains("V4 Styles")) {
                    QStringList styleFormat;
                    styleSection +=line + "\n";
                    //Style: Name,Fontname,Fontsize,PrimaryColour,SecondaryColour,OutlineColour,BackColour,Bold,Italic,Underline,StrikeOut,ScaleX,ScaleY,Spacing,Angle,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding
                    styleFormat = (line.split(": ")[1].replace(" ","")).split(',');
                    styleName = styleFormat[0];

                }
                //qDebug() << "\n turn != 0  " << turn<< line;
                if (section.contains("Events")) {
                    //if it is event
                    QStringList format;
                    if (line.contains("Format:")) {
                    	eventSection += line +"\n";
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
    toJson();
    jsontoSubtitle(toJson());
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
    if (total[2].contains('.'))
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
	if (start.frames(pCore->getCurrentFps()) < 0 || end.frames(pCore->getCurrentFps()) < 0) {
        qDebug()<<"Time error: is negative";
        return;
    }
    if (start.frames(pCore->getCurrentFps()) > end.frames(pCore->getCurrentFps())) {
        qDebug()<<"Time error: start should be less than end";
        return;
    }
    auto model = getModel(); //gets model shared ptr
    //Q_ASSERT(model->m_subtitleList.count(start)==0); //returns warning if sub at start time position already exists ,i.e. count !=0
    if (m_subtitleList[start].first == str) {
        qDebug()<<"already present in model"<<"string :"<<m_subtitleList[start].first<<" start time "<<start.frames(pCore->getCurrentFps())<<"end time : "<< m_subtitleList[start].second.frames(pCore->getCurrentFps());
        return;
    }
    /*if (model->m_subtitleList.count(start) > 0) {
        qDebug()<<"Start time already in model";
        editSubtitle(start, str, end);
        return;
    }*/
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

void SubtitleModel::editEndPos(GenTime startPos, GenTime newEndPos)
{
    qDebug()<<"Changing the sub end timings in model";
    auto model = getModel();
    if (model->m_subtitleList.count(startPos) <= 0) {
        //is not present in model only
        return;
    }
    int row = static_cast<int>(std::distance(model->m_subtitleList.begin(), model->m_subtitleList.find(startPos)));
    model->m_subtitleList[startPos].second = newEndPos;
    emit model->dataChanged(model->index(row), model->index(row), QVector<int>() << EndPosRole);
    qDebug()<<startPos.frames(pCore->getCurrentFps())<<m_subtitleList[startPos].second.frames(pCore->getCurrentFps());
    return;
}

void SubtitleModel::editSubtitle(GenTime startPos, QString newSubtitleText, GenTime endPos)
{
    if(startPos.frames(pCore->getCurrentFps()) < 0 || endPos.frames(pCore->getCurrentFps()) < 0) {
        qDebug()<<"Time error: is negative";
        return;
    }
    if(startPos.frames(pCore->getCurrentFps()) > endPos.frames(pCore->getCurrentFps())) {
        qDebug()<<"Time error: start should be less than end";
        return;
    }
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
    if (model->m_subtitleList.count(pos) <= 0) {
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
    if (model->m_subtitleList.count(oldPos) <= 0) {
        //is not present in model only
        return;
    }
    QString subtitleText = model->m_subtitleList[oldPos].first ;
    GenTime endPos = model->m_subtitleList[oldPos].second;
    removeSubtitle(oldPos);
    addSubtitle(newPos, endPos, subtitleText);
    return;
}

QString SubtitleModel::toJson()
{
    //qDebug()<< "to JSON";
    QJsonArray list;
    for (const auto &subtitle : m_subtitleList) {
        QJsonObject currentSubtitle;
        currentSubtitle.insert(QLatin1String("startPos"), QJsonValue(subtitle.first.seconds()));
        currentSubtitle.insert(QLatin1String("dialogue"), QJsonValue(subtitle.second.first));
        currentSubtitle.insert(QLatin1String("endPos"), QJsonValue(subtitle.second.second.seconds()));
        list.push_back(currentSubtitle);
        //qDebug()<<subtitle.first.seconds();
    }
    QJsonDocument jsonDoc(list);
    //qDebug()<<QString(jsonDoc.toJson());
    return QString(jsonDoc.toJson());
}

void SubtitleModel::jsontoSubtitle(const QString &data)
{
	QString outFile = "path_to_temp_Subtitle.srt"; // use srt format as default unless file is imported (m_subFilePath)
    if (!outFile.contains(".ass"))
        qDebug()<< "srt file import"; // if imported file isn't .ass, it is .srt format
    QFile outF(outFile);

    //qDebug()<< "Import from JSON";
    QWriteLocker locker(&m_lock);
    auto json = QJsonDocument::fromJson(data.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return;
    }
    int line=0;
    auto list = json.array();
    if (outF.open(QIODevice::WriteOnly)) {
        QTextStream out(&outF);
        if (outFile.contains(".ass")) {
        	out<<scriptInfoSection<<endl;
        	out<<styleSection<<endl;
        	out<<eventSection;
        }
        for (const auto &entry : list) {
            if (!entry.isObject()) {
                qDebug() << "Warning : Skipping invalid subtitle data";
                continue;
            }
            auto entryObj = entry.toObject();
            if (!entryObj.contains(QLatin1String("startPos"))) {
                qDebug() << "Warning : Skipping invalid subtitle data (does not contain position)";
                continue;
            }
            double startPos = entryObj[QLatin1String("startPos")].toDouble();
            //convert seconds to FORMAT= hh:mm:ss.SS (in .ass) and hh:mm:ss,SSS (in .srt)
            int millisec = startPos * 1000;
            int seconds = millisec / 1000;
            millisec %=1000;
            int minutes = seconds / 60;
            seconds %= 60;
            int hours = minutes /60;
            minutes %= 60;
            int milli_2 = millisec / 10;
            QString startTimeString = QString("%1:%2:%3.%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(milli_2,2,10,QChar('0'));
            QString startTimeStringSRT = QString("%1:%2:%3,%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(millisec,3,10,QChar('0'));
            QString dialogue = entryObj[QLatin1String("dialogue")].toString();
            double endPos = entryObj[QLatin1String("endPos")].toDouble();
            millisec = endPos * 1000;
            seconds = millisec / 1000;
            millisec %=1000;
            minutes = seconds / 60;
            seconds %= 60;
            hours = minutes /60;
            minutes %= 60;

            milli_2 = millisec / 10; // to limit ms to 2 digits (for .ass)
            QString endTimeString = QString("%1:%2:%3.%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(milli_2,2,10,QChar('0'));

            QString endTimeStringSRT = QString("%1:%2:%3,%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(millisec,3,10,QChar('0'));
            line++;
            if (outFile.contains(".ass")) {
            	//Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text
            	out <<"Dialogue: 0,"<<startTimeString<<","<<endTimeString<<","<<styleName<<",,0000,0000,0000,,"<<dialogue<<endl;
            }
            if (outFile.contains(".srt"))
                out<<line<<"\n"<<startTimeStringSRT<<" --> "<<endTimeStringSRT<<"\n"<<dialogue<<"\n"<<endl;
            
            //qDebug() << "ADDING SUBTITLE to FILE AT START POS: " << startPos <<" END POS: "<<endPos;//<< ", FPS: " << pCore->getCurrentFps();
        }
    }
    qDebug()<<"Setting subtitle filter";
    m_subtitleFilter->set("av.filename", outFile.toUtf8().constData());
    m_tractor->attach(*m_subtitleFilter.get());
}