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

#include "subtitlemodel.hpp"
#include "bin/bin.h"
#include "core.h"
#include "project/projectmanager.h"
#include "doc/kdenlivedoc.h"
#include "timeline2/model/snapmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "macros.hpp"
#include "profiles/profilemodel.hpp"
#include "undohelper.hpp"

#include <mlt++/MltProperties.h>
#include <mlt++/Mlt.h>

#include <KLocalizedString>
#include <KMessageBox>
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextCodec>
#include <utility>

SubtitleModel::SubtitleModel(Mlt::Tractor *tractor, std::shared_ptr<TimelineItemModel> timeline, QObject *parent)
    : QAbstractListModel(parent)
    , m_timeline(timeline)
    , m_lock(QReadWriteLock::Recursive)
    , m_subtitleFilter(new Mlt::Filter(pCore->getCurrentProfile()->profile(), "avfilter.subtitles"))
    , m_tractor(tractor)
{
    qDebug()<< "subtitle constructor";
    qDebug()<<"Filter!";
    if (tractor != nullptr) {
        qDebug()<<"Tractor!";
        m_subtitleFilter->set("internal_added", 237);
    }
    setup();
    QSize frameSize = pCore->getCurrentFrameDisplaySize();
    int fontSize = frameSize.height() / 15;
    int fontMargin = frameSize.height() - (2 *fontSize);
    scriptInfoSection = QString("[Script Info]\n; This is a Sub Station Alpha v4 script.\n;\nScriptType: v4.00\nCollisions: Normal\nPlayResX: %1\nPlayResY: %2\nTimer: 100.0000\n").arg(frameSize.width()).arg(frameSize.height());
    styleSection = QString("[V4 Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\nStyle: Default,Consolas,%1,16777215,65535,255,0,-1,0,1,2,2,6,40,40,%2,0,1\n").arg(fontSize).arg(fontMargin);
    eventSection = QStringLiteral("[Events]\n");
    styleName = QStringLiteral("Default");
    connect(this, &SubtitleModel::modelChanged, [this]() {
        jsontoSubtitle(toJson()); 
    });
    
}

void SubtitleModel::setup()
{
    // We connect the signals of the abstractitemmodel to a more generic one.
    connect(this, &SubtitleModel::columnsMoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::columnsRemoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::columnsInserted, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::rowsMoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::modelReset, this, &SubtitleModel::modelChanged);
}

void SubtitleModel::unsetModel()
{
    m_timeline.reset();
}

void SubtitleModel::importSubtitle(const QString filePath, int offset, bool externalImport)
{
    QString start,end,comment;
    QString timeLine;
    GenTime startPos, endPos;
    int turn = 0,r = 0;
    /*
     * turn = 0 -> Parse next subtitle line [srt] (or) Parse next section [ssa]
     * turn = 1 -> Add string to timeLine
     * turn > 1 -> Add string to completeLine
     */
    if (filePath.isEmpty() || isLocked())
        return;
    Fun redo = []() { return true; };
    Fun undo = [this]() {
        emit modelChanged();
        return true;
    };
    GenTime subtitleOffset(offset, pCore->getCurrentFps());
    if (filePath.endsWith(".srt")) {
        QFile srtFile(filePath);
        if (!srtFile.exists() || !srtFile.open(QIODevice::ReadOnly)) {
            qDebug() << " File not found " << filePath;
            return;
        }
        qDebug()<< "srt File";
        //parsing srt file
        QTextStream stream(&srtFile);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        QString line;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (!line.isEmpty()) {
                if (!turn) {
                    // index=atoi(line.toStdString().c_str());
                    turn++;
                    continue;
                }
                if (line.contains(QLatin1String("-->"))) {
                    timeLine += line;
                    QStringList srtTime = timeLine.split(QLatin1Char(' '));
                    if (srtTime.count() < 3) {
                        // invalid time
                        continue;
                    }
                    start = srtTime.at(0);
                    startPos= stringtoTime(start);
                    end = srtTime.at(2);
                    endPos = stringtoTime(end);
                } else {
                    r++;
                    if (!comment.isEmpty())
                        comment += " ";
                    if (r == 1)
                        comment += line;
                    else
                        comment = comment + "\r" +line;
                }
                turn++;
            } else {
                if (endPos > startPos) {
                    addSubtitle(startPos + subtitleOffset, endPos + subtitleOffset, comment, undo, redo, false);
                }
                //reinitialize
                comment.clear();
                timeLine.clear();
                turn = 0; r = 0;
            }            
        }  
        srtFile.close();
    } else if (filePath.endsWith(QLatin1String(".ass"))) {
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
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        QString line;
        qDebug() << " correct ass file  " << filePath;
        scriptInfoSection.clear();
        styleSection.clear();
        eventSection.clear();
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (!line.isEmpty()) {
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
                    } else if (line.contains("Events")) {
                        turn++;
                        section = "Events";
                        eventSection += line +"\n";
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else {
                        //unknown section
                        
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
                    if (!styleFormat.isEmpty()) {
                        styleName = styleFormat.first();
                    }

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
                        if (maxSplit > 2)
                            startTime = format.at(1);
                        if (maxSplit > 3)
                            endTime = format.at(2);
                        // Text
                        if (maxSplit > 9)
                            text = format.at(9);
                        //qDebug()<< startTime << endTime << text;
                    } else {
                        QString EventDialogue;
                        QStringList dialogue;
                        start = "";end = "";comment = "";
                        EventDialogue += line;
                        dialogue = EventDialogue.split(": ")[1].split(',');
                        if (dialogue.count() > 9) {
                            QString remainingStr = "," + EventDialogue.split(": ")[1].section(',', maxSplit);
                            //qDebug()<< dialogue;
                            //TIME
                            start = dialogue.at(1);
                            startPos= stringtoTime(start);
                            end = dialogue.at(2);
                            endPos= stringtoTime(end);
                            // Text
                            comment = dialogue.at(9)+ remainingStr;
                            //qDebug()<<"Start: "<< start << "End: "<<end << comment;
                            addSubtitle(startPos + subtitleOffset, endPos + subtitleOffset, comment, undo, redo, false);
                        }
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
    Fun update_model= [this]() {
        emit modelChanged();
        return true;
    };
    PUSH_LAMBDA(update_model, redo);
    update_model();
    if (externalImport) {
        pCore->pushUndo(undo, redo, i18n("Edit subtitle"));
    }
}

void SubtitleModel::parseSubtitle(const QString subPath)
{   
	qDebug()<<"Parsing started";
    if (!subPath.isEmpty()) {
        m_subtitleFilter->set("av.filename", subPath.toUtf8().constData());
    }
    QString filePath = m_subtitleFilter->get("av.filename");
    m_subFilePath = filePath;
    importSubtitle(filePath, 0, false);
    //jsontoSubtitle(toJson());
}

const QString SubtitleModel::getUrl()
{
    return m_subtitleFilter->get("av.filename");
}

GenTime SubtitleModel::stringtoTime(QString &str)
{
    QStringList total,secs;
    double hours = 0, mins = 0, seconds = 0, ms = 0;
    double total_sec = 0;
    GenTime pos;
    total = str.split(QLatin1Char(':'));
    if (total.count() != 3) {
        // invalid time found
        return GenTime();
    }
    hours = atoi(total.at(0).toStdString().c_str());
    mins = atoi(total.at(1).toStdString().c_str());
    if (total.at(2).contains(QLatin1Char('.')))
        secs = total.at(2).split(QLatin1Char('.')); //ssa file
    else
        secs = total.at(2).split(QLatin1Char(',')); //srt file
    if (secs.count() < 2) {
        seconds = atoi(total.at(2).toStdString().c_str());
    } else {
        seconds = atoi(secs.at(0).toStdString().c_str());
        ms = atoi(secs.at(1).toStdString().c_str());
    }
    total_sec = hours *3600 + mins *60 + seconds + ms * 0.001 ;
    pos= GenTime(total_sec);
    return pos;
}

bool SubtitleModel::addSubtitle(GenTime start, GenTime end, const QString str, Fun &undo, Fun &redo, bool updateFilter)
{
    if (isLocked()) {
        return false;
    }
    int id = TimelineModel::getNextId();
    Fun local_redo = [this, id, start, end, str, updateFilter]() {
        addSubtitle(id, start, end, str, false, updateFilter);
        QPair<int, int> range = {start.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    Fun local_undo = [this, id, start, end, updateFilter]() {
        removeSubtitle(id, false, updateFilter);
        QPair<int, int> range = {start.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    local_redo();
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool SubtitleModel::addSubtitle(int id, GenTime start, GenTime end, const QString str, bool temporary, bool updateFilter)
{
	if (start.frames(pCore->getCurrentFps()) < 0 || end.frames(pCore->getCurrentFps()) < 0 || isLocked()) {
        qDebug()<<"Time error: is negative";
        return false;
    }
    if (start.frames(pCore->getCurrentFps()) > end.frames(pCore->getCurrentFps())) {
        qDebug()<<"Time error: start should be less than end";
        return false;
    }
    // Don't allow 2 subtitles at same start pos
    if (m_subtitleList.count(start) > 0) {
        qDebug()<<"already present in model"<<"string :"<<m_subtitleList[start].first<<" start time "<<start.frames(pCore->getCurrentFps())<<"end time : "<< m_subtitleList[start].second.frames(pCore->getCurrentFps());
        return false;
    }
    m_timeline->registerSubtitle(id, start, temporary);
    int row = m_timeline->getSubtitleIndex(id);
    beginInsertRows(QModelIndex(), row, row);
    m_subtitleList[start] = {str, end};
    endInsertRows();
    addSnapPoint(start);
    addSnapPoint(end);
    if (!temporary && end.frames(pCore->getCurrentFps()) > m_timeline->duration()) {
        m_timeline->updateDuration();
    }
    qDebug()<<"Added to model";
    if (updateFilter) {
        emit modelChanged();
    }
    return true;
}

QHash<int, QByteArray> SubtitleModel::roleNames() const 
{
    QHash<int, QByteArray> roles;
    roles[SubtitleRole] = "subtitle";
    roles[StartPosRole] = "startposition";
    roles[EndPosRole] = "endposition";
    roles[StartFrameRole] = "startframe";
    roles[EndFrameRole] = "endframe";
    roles[GrabRole] = "grabbed";
    roles[IdRole] = "id";
    roles[SelectedRole] = "selected";
    return roles;
}

QVariant SubtitleModel::data(const QModelIndex& index, int role) const
{   
    if (index.row() < 0 || index.row() >= static_cast<int>(m_subtitleList.size()) || !index.isValid()) {
        return QVariant();
    }
    auto subInfo = m_timeline->getSubtitleIdFromIndex(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
        case SubtitleRole:
            return m_subtitleList.at(subInfo.second).first;
        case IdRole:
            return subInfo.first;
        case StartPosRole:
            return subInfo.second.seconds();
        case EndPosRole:
            return m_subtitleList.at(subInfo.second).second.seconds();
        case StartFrameRole:
            return subInfo.second.frames(pCore->getCurrentFps());
        case EndFrameRole:
            return m_subtitleList.at(subInfo.second).second.frames(pCore->getCurrentFps());
        case SelectedRole:
            return m_selected.contains(subInfo.first);
        case GrabRole:
            return m_grabbedIds.contains(subInfo.first);
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

SubtitledTime SubtitleModel::getSubtitle(GenTime startFrame) const
{
    for (const auto &subtitles : m_subtitleList) {
        if (subtitles.first == startFrame) {
            return SubtitledTime(subtitles.first, subtitles.second.first, subtitles.second.second);
        }
    }
    return SubtitledTime(GenTime(), QString(), GenTime());
}

QString SubtitleModel::getText(int id) const
{
    if (m_timeline->m_allSubtitles.find( id ) == m_timeline->m_allSubtitles.end()) {
        return QString();
    }
    GenTime start = m_timeline->m_allSubtitles.at(id);
    return m_subtitleList.at(start).first;
}

bool SubtitleModel::setText(int id, const QString text)
{
    if (m_timeline->m_allSubtitles.find( id ) == m_timeline->m_allSubtitles.end() || isLocked()) {
        return false;
    }
    GenTime start = m_timeline->m_allSubtitles.at(id);
    GenTime end = m_subtitleList.at(start).second;
    QString oldText = m_subtitleList.at(start).first;
    m_subtitleList[start].first = text;
    Fun local_redo = [this, start, end, text]() {
        editSubtitle(start, text);
        QPair<int, int> range = {start.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    Fun local_undo = [this, start, end, oldText]() {
        editSubtitle(start, oldText);
        QPair<int, int> range = {start.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
    return true;
}

std::unordered_set<int> SubtitleModel::getItemsInRange(int startFrame, int endFrame) const
{
    if (isLocked()) {
        return {};
    }
    GenTime startTime(startFrame, pCore->getCurrentFps());
    GenTime endTime(endFrame, pCore->getCurrentFps());
    std::unordered_set<int> matching;
    for (const auto &subtitles : m_subtitleList) {
        if (endFrame > -1 && subtitles.first > endTime) {
            // Outside range
            continue;
        }
        if (subtitles.first >= startTime || subtitles.second.second >= startTime) {
            int sid = getIdForStartPos(subtitles.first);
            if (sid > -1) {
                matching.emplace(sid);
            } else {
                qDebug()<<"==== FOUND INVALID SUBTILE AT: "<<subtitles.first.frames(pCore->getCurrentFps());
            }
        }
    }
    return matching;
}

bool SubtitleModel::cutSubtitle(int position)
{
    Fun redo = []() { return true; };
    Fun undo = []() { return true; };
    if (cutSubtitle(position, undo, redo)) {
        pCore->pushUndo(undo, redo, i18n("Cut clip"));
        return true;
    }
    return false;
}


bool SubtitleModel::cutSubtitle(int position, Fun &undo, Fun &redo)
{
    if (isLocked()) {
        return false;
    }
    GenTime pos(position, pCore->getCurrentFps());
    GenTime start = GenTime(-1);
    for (const auto &subtitles : m_subtitleList) {
        if (subtitles.first <= pos && subtitles.second.second > pos) {
            start = subtitles.first;
            break;
        }
    }
    if (start >= GenTime()) {
        GenTime end = m_subtitleList.at(start).second;
        QString text = m_subtitleList.at(start).first;
        
        int subId = getIdForStartPos(start);
        int duration = position - start.frames(pCore->getCurrentFps());
        bool res = requestResize(subId, duration, true, undo, redo, false);
        if (res) {
            int id = TimelineModel::getNextId();
            Fun local_redo = [this, id, pos, end, text]() {
                return addSubtitle(id, pos, end, text);
            };
            Fun local_undo = [this, id]() {
                removeSubtitle(id);
                return true;
            };
            if (local_redo()) {
                UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
                return true;
            }
        }
    }
    undo();
    return false;
}

void SubtitleModel::registerSnap(const std::weak_ptr<SnapInterface> &snapModel)
{
    // make sure ptr is valid
    if (auto ptr = snapModel.lock()) {
        // ptr is valid, we store it
        m_regSnaps.push_back(snapModel);
        // we now add the already existing subtitles to the snap
        for (const auto &subtitle : m_subtitleList) {
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

void SubtitleModel::removeSnapPoint(GenTime startpos)
{
    std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    for (const auto &snapModel : m_regSnaps) {
        if (auto ptr = snapModel.lock()) {
            validSnapModels.push_back(snapModel);
            ptr->removePoint(startpos.frames(pCore->getCurrentFps()));
        }
    }
    // Update the list of snapModel known to be valid
    std::swap(m_regSnaps, validSnapModels);
}

void SubtitleModel::editEndPos(GenTime startPos, GenTime newEndPos, bool refreshModel)
{
    qDebug()<<"Changing the sub end timings in model";
    if (m_subtitleList.count(startPos) <= 0) {
        //is not present in model only
        return;
    }
    m_subtitleList[startPos].second = newEndPos;
    // Trigger update of the qml view
    int id = getIdForStartPos(startPos);
    int row = m_timeline->getSubtitleIndex(id);
    emit dataChanged(index(row), index(row), {EndFrameRole});
    if (refreshModel) {
        emit modelChanged();
    }
    qDebug()<<startPos.frames(pCore->getCurrentFps())<<m_subtitleList[startPos].second.frames(pCore->getCurrentFps());
}

void SubtitleModel::switchGrab(int sid)
{
    if (m_grabbedIds.contains(sid)) {
        m_grabbedIds.removeAll(sid);
    } else {
        m_grabbedIds << sid;
    }
    int row = m_timeline->getSubtitleIndex(sid);
    emit dataChanged(index(row), index(row), {GrabRole});
}

void SubtitleModel::clearGrab()
{
    QVector<int> grabbed = m_grabbedIds;
    m_grabbedIds.clear();
    for (int sid : grabbed) {
        int row = m_timeline->getSubtitleIndex(sid);
        emit dataChanged(index(row), index(row), {GrabRole});
    }
}

bool SubtitleModel::requestResize(int id, int size, bool right)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestResize(id, size, right, undo, redo, true);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Resize subtitle"));
        return true;
    } else {
        undo();
        return false;
    }
}

bool SubtitleModel::requestResize(int id, int size, bool right, Fun &undo, Fun &redo, bool logUndo)
{
    if (isLocked()) {
        return false;
    }
    Q_ASSERT(m_timeline->m_allSubtitles.find( id ) != m_timeline->m_allSubtitles.end());
    GenTime startPos = m_timeline->m_allSubtitles.at(id);
    GenTime endPos = m_subtitleList.at(startPos).second;
    Fun operation = []() { return true; };
    Fun reverse = []() { return true; };
    if (right) {
        GenTime newEndPos = startPos + GenTime(size, pCore->getCurrentFps());
        operation = [this, id, startPos, endPos, newEndPos, logUndo]() {
            m_subtitleList[startPos].second = newEndPos;
            removeSnapPoint(endPos);
            addSnapPoint(newEndPos);
            // Trigger update of the qml view
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {EndFrameRole});
            if (logUndo) {
                emit modelChanged();
                QPair<int, int> range;
                if (endPos > newEndPos) {
                    range = {newEndPos.frames(pCore->getCurrentFps()), endPos.frames(pCore->getCurrentFps())};
                } else {
                    range = {endPos.frames(pCore->getCurrentFps()), newEndPos.frames(pCore->getCurrentFps())};
                }
                pCore->invalidateRange(range);
                pCore->refreshProjectRange(range);
            }
            return true;
        };
        reverse = [this, id, startPos, endPos, newEndPos, logUndo]() {
            m_subtitleList[startPos].second = endPos;
            removeSnapPoint(newEndPos);
            addSnapPoint(endPos);
            // Trigger update of the qml view
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {EndFrameRole});
            if (logUndo) {
                emit modelChanged();
                QPair<int, int> range;
                if (endPos > newEndPos) {
                    range = {newEndPos.frames(pCore->getCurrentFps()), endPos.frames(pCore->getCurrentFps())};
                } else {
                    range = {endPos.frames(pCore->getCurrentFps()), newEndPos.frames(pCore->getCurrentFps())};
                }
                pCore->invalidateRange(range);
                pCore->refreshProjectRange(range);
            }
            return true;
        };
    } else {
        GenTime newStartPos = endPos - GenTime(size, pCore->getCurrentFps());
        if (m_subtitleList.count(newStartPos) > 0) {
            // There already is another subtitle at this position, abort
            return false;
        }
        const QString text = m_subtitleList.at(startPos).first;
        operation = [this, id, startPos, newStartPos, endPos, text, logUndo]() {
            m_timeline->m_allSubtitles[id] = newStartPos;
            m_subtitleList.erase(startPos);
            m_subtitleList[newStartPos] = {text, endPos};
            // Trigger update of the qml view
            removeSnapPoint(startPos);
            addSnapPoint(newStartPos);
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {StartFrameRole});
            if (logUndo) {
                emit modelChanged();
                QPair<int, int> range;
                if (startPos > newStartPos) {
                    range = {newStartPos.frames(pCore->getCurrentFps()), startPos.frames(pCore->getCurrentFps())};
                } else {
                    range = {startPos.frames(pCore->getCurrentFps()), newStartPos.frames(pCore->getCurrentFps())};
                }
                pCore->invalidateRange(range);
                pCore->refreshProjectRange(range);
            }
            return true;
        };
        reverse = [this, id, startPos, newStartPos, endPos, text, logUndo]() {
            m_timeline->m_allSubtitles[id] = startPos;
            m_subtitleList.erase(newStartPos);
            m_subtitleList[startPos] = {text, endPos};
            removeSnapPoint(newStartPos);
            addSnapPoint(startPos);
            // Trigger update of the qml view
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {StartFrameRole});
            if (logUndo) {
                emit modelChanged();
                QPair<int, int> range;
                if (startPos > newStartPos) {
                    range = {newStartPos.frames(pCore->getCurrentFps()), startPos.frames(pCore->getCurrentFps())};
                } else {
                    range = {startPos.frames(pCore->getCurrentFps()), newStartPos.frames(pCore->getCurrentFps())};
                }
                pCore->invalidateRange(range);
                pCore->refreshProjectRange(range);
            }
            return true;
        };
    }
    operation();
    UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    return true;
}

void SubtitleModel::editSubtitle(GenTime startPos, QString newSubtitleText)
{
    if (isLocked()) {
        return;
    }
    if(startPos.frames(pCore->getCurrentFps()) < 0) {
        qDebug()<<"Time error: is negative";
        return;
    }
    qDebug()<<"Editing existing subtitle in model";
    m_subtitleList[startPos].first = newSubtitleText ;
    int id = getIdForStartPos(startPos);
    qDebug()<<startPos.frames(pCore->getCurrentFps())<<m_subtitleList[startPos].first<<m_subtitleList[startPos].second.frames(pCore->getCurrentFps());
    int row = m_timeline->getSubtitleIndex(id);
    emit dataChanged(index(row), index(row), QVector<int>() << SubtitleRole);
    emit modelChanged();
    return;
}

bool SubtitleModel::removeSubtitle(int id, bool temporary, bool updateFilter)
{
    qDebug()<<"Deleting subtitle in model";
    if (isLocked()) {
        return false;
    }
    if (m_timeline->m_allSubtitles.find( id ) == m_timeline->m_allSubtitles.end()) {
        qDebug()<<"No Subtitle at pos in model";
        return false;
    }
    GenTime start = m_timeline->m_allSubtitles.at(id);
    if (m_subtitleList.count(start) <= 0) {
        qDebug()<<"No Subtitle at pos in model";
        return false;
    }
    GenTime end = m_subtitleList.at(start).second;
    int row = m_timeline->getSubtitleIndex(id);
    m_timeline->deregisterSubtitle(id, temporary);
    beginRemoveRows(QModelIndex(), row, row);
    bool lastSub = false;
    if (start == m_subtitleList.rbegin()->first) {
        // Check if this is the last subtitle
        lastSub = true;
    }
    m_subtitleList.erase(start);
    endRemoveRows();
    removeSnapPoint(start);
    removeSnapPoint(end);
    if (lastSub) {
        m_timeline->updateDuration();
    }
    if (updateFilter) {
        emit modelChanged();
    }
    return true;
}

void SubtitleModel::removeAllSubtitles()
{
    if (isLocked()) {
        return;
    }
    auto ids = m_timeline->m_allSubtitles;
    for (const auto &p : ids) {
        removeSubtitle(p.first);
    }
}

void SubtitleModel::requestSubtitleMove(int clipId, GenTime position)
{
    
    GenTime oldPos = getStartPosForId(clipId);
    Fun local_redo = [this, clipId, position]() {
        return moveSubtitle(clipId, position, true, true);
    };
    Fun local_undo = [this, clipId, oldPos]() {
        return moveSubtitle(clipId, oldPos, true, true);
    };
    bool res = local_redo();
    if (res) {
        pCore->pushUndo(local_undo, local_redo, i18n("Move subtitle"));
    }
}

bool SubtitleModel::moveSubtitle(int subId, GenTime newPos, bool updateModel, bool updateView)
{
    qDebug()<<"Moving Subtitle";
    if (m_timeline->m_allSubtitles.count(subId) == 0 || isLocked()) {
        return false;
    }
    GenTime oldPos = m_timeline->m_allSubtitles.at(subId);
    if (m_subtitleList.count(oldPos) <= 0 || m_subtitleList.count(newPos) > 0) {
        //is not present in model, or already another one at new position
        qDebug()<<"==== MOVE FAILED";
        return false;
    }
    QString subtitleText = m_subtitleList[oldPos].first ;
    removeSnapPoint(oldPos);
    removeSnapPoint(m_subtitleList[oldPos].second);
    GenTime duration = m_subtitleList[oldPos].second - oldPos;
    GenTime endPos = newPos + duration;
    int id = getIdForStartPos(oldPos);
    m_timeline->m_allSubtitles[id] = newPos;
    m_subtitleList.erase(oldPos);
    m_subtitleList[newPos] = {subtitleText, endPos};
    addSnapPoint(newPos);
    addSnapPoint(endPos);
    if (updateView) {
        updateSub(id, {StartFrameRole, EndFrameRole});
        QPair<int, int> range;
        if (oldPos < newPos) {
            range = {oldPos.frames(pCore->getCurrentFps()), endPos.frames(pCore->getCurrentFps())};
        } else {
            range = {newPos.frames(pCore->getCurrentFps()), (oldPos + duration).frames(pCore->getCurrentFps())};
        }
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
    }
    if (updateModel) {
        // Trigger update of the subtitle file
        emit modelChanged();
        if (newPos == m_subtitleList.rbegin()->first) {
            // Check if this is the last subtitle
            m_timeline->updateDuration();
        }
    }
    return true;
}

int SubtitleModel::getIdForStartPos(GenTime startTime) const
{
    auto findResult = std::find_if(std::begin(m_timeline->m_allSubtitles), std::end(m_timeline->m_allSubtitles), [&](const std::pair<int, GenTime> &pair) {
        return pair.second == startTime;
    });
    if (findResult != std::end(m_timeline->m_allSubtitles)) {
        return findResult->first;
    }
    return -1;
}

GenTime SubtitleModel::getStartPosForId(int id) const
{
    if (m_timeline->m_allSubtitles.count(id) == 0) {
        return GenTime();
    };
    return m_timeline->m_allSubtitles.at(id);
}

int SubtitleModel::getPreviousSub(int id) const
{
    GenTime start = getStartPosForId(id);
    int row = static_cast<int>(std::distance(m_subtitleList.begin(), m_subtitleList.find(start)));
    if (row > 0) {
        row--;
        auto it = m_subtitleList.begin();
        std::advance(it, row);
        const GenTime res = it->first;
        return getIdForStartPos(res);
    }
    return -1;
}

int SubtitleModel::getNextSub(int id) const
{
    GenTime start = getStartPosForId(id);
    int row = static_cast<int>(std::distance(m_subtitleList.begin(), m_subtitleList.find(start)));
    if (row < static_cast<int>(m_subtitleList.size()) - 1) {
        row++;
        auto it = m_subtitleList.begin();
        std::advance(it, row);
        const GenTime res = it->first;
        return getIdForStartPos(res);
    }
    return -1;
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

void SubtitleModel::copySubtitle(const QString &path, bool checkOverwrite)
{
    QFile srcFile(pCore->currentDoc()->subTitlePath(false));
    if (srcFile.exists()) {
        QFile prev(path);
        if (prev.exists()) {
            if (checkOverwrite || !path.endsWith(QStringLiteral(".srt"))) {
                if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("File %1 already exists.\nDo you want to overwrite it?", path)) == KMessageBox::No) {
                    return;
                }
            }
            prev.remove();
        }
        srcFile.copy(path);
    }
}


void SubtitleModel::jsontoSubtitle(const QString &data)
{
    QString outFile = pCore->currentDoc()->subTitlePath(false);
    QString masterFile = m_subtitleFilter->get("av.filename");
    if (masterFile.isEmpty()) {
        m_subtitleFilter->set("av.filename", outFile.toUtf8().constData());
    }
    bool assFormat = outFile.endsWith(".ass");
    if (!assFormat) {
        qDebug()<< "srt file import"; // if imported file isn't .ass, it is .srt format
    }
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
        out.setCodec("UTF-8");
        if (assFormat) {
            out<<scriptInfoSection<<'\n';
            out<<styleSection<<'\n';
            out<<eventSection;
        }
        for (const auto &entry : qAsConst(list)) {
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
            int millisec = int(startPos * 1000);
            int seconds = millisec / 1000;
            millisec %=1000;
            int minutes = seconds / 60;
            seconds %= 60;
            int hours = minutes /60;
            minutes %= 60;
            int milli_2 = millisec / 10;
            QString startTimeString = QString("%1:%2:%3.%4")
              .arg(hours, 2, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(milli_2,2,10,QChar('0'));
            QString startTimeStringSRT = QString("%1:%2:%3,%4")
              .arg(hours, 2, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(millisec,3,10,QChar('0'));
            QString dialogue = entryObj[QLatin1String("dialogue")].toString();
            double endPos = entryObj[QLatin1String("endPos")].toDouble();
            millisec = int(endPos * 1000);
            seconds = millisec / 1000;
            millisec %=1000;
            minutes = seconds / 60;
            seconds %= 60;
            hours = minutes /60;
            minutes %= 60;

            milli_2 = millisec / 10; // to limit ms to 2 digits (for .ass)
            QString endTimeString = QString("%1:%2:%3.%4")
              .arg(hours, 2, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(milli_2,2,10,QChar('0'));

            QString endTimeStringSRT = QString("%1:%2:%3,%4")
              .arg(hours, 2, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(millisec,3,10,QChar('0'));
            line++;
            if (assFormat) {
                //Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text
                out <<"Dialogue: 0,"<<startTimeString<<","<<endTimeString<<","<<styleName<<",,0000,0000,0000,,"<<dialogue<<'\n';
            } else {
                out<<line<<"\n"<<startTimeStringSRT<<" --> "<<endTimeStringSRT<<"\n"<<dialogue<<"\n"<<'\n';
            }
            
            //qDebug() << "ADDING SUBTITLE to FILE AT START POS: " << startPos <<" END POS: "<<endPos;//<< ", FPS: " << pCore->getCurrentFps();
        }
        outF.close();
    }
    qDebug()<<"Saving subtitle filter: "<<outFile;
    if (line > 0) {
        m_subtitleFilter->set("av.filename", outFile.toUtf8().constData());
        m_tractor->attach(*m_subtitleFilter.get());
    } else {
        m_tractor->detach(*m_subtitleFilter.get());
    }
}

void SubtitleModel::updateSub(int id, QVector <int> roles)
{
    int row = m_timeline->getSubtitleIndex(id);
    emit dataChanged(index(row), index(row), roles);
}

int SubtitleModel::getRowForId(int id) const
{
    return m_timeline->getSubtitleIndex(id);
}

int SubtitleModel::getSubtitlePlaytime(int id) const
{
    GenTime startPos = m_timeline->m_allSubtitles.at(id);
    return m_subtitleList.at(startPos).second.frames(pCore->getCurrentFps()) - startPos.frames(pCore->getCurrentFps());
}

int SubtitleModel::getSubtitleEnd(int id) const
{
    GenTime startPos = m_timeline->m_allSubtitles.at(id);
    return m_subtitleList.at(startPos).second.frames(pCore->getCurrentFps());
}

void SubtitleModel::setSelected(int id, bool select)
{
    if (isLocked()) {
        return;
    }
    if (select) {
        m_selected << id;
    } else {
        m_selected.removeAll(id);
    }
    updateSub(id, {SelectedRole});
}

bool SubtitleModel::isSelected(int id) const
{
    return m_selected.contains(id);
}

int SubtitleModel::trackDuration() const
{
    if (m_subtitleList.empty()) {
        return 0;
    }
    return m_subtitleList.rbegin()->second.second.frames(pCore->getCurrentFps());
}

void SubtitleModel::switchDisabled()
{
    m_subtitleFilter->set("disable", 1 - m_subtitleFilter->get_int("disable"));
}

void SubtitleModel::switchLocked()
{
    bool isLocked = m_subtitleFilter->get_int("kdenlive:locked") == 1;
    m_subtitleFilter->set("kdenlive:locked", isLocked ? 0 : 1);
    
    // En/disable snapping on lock
    /*std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    for (const auto &snapModel : m_regSnaps) {
        if (auto ptr = snapModel.lock()) {
            validSnapModels.push_back(snapModel);
            if (isLocked) {
                for (const auto &subtitle : m_subtitleList) {
                    ptr->addPoint(subtitle.first.frames(pCore->getCurrentFps()));
                    ptr->addPoint(subtitle.second.second.frames(pCore->getCurrentFps()));
                }
            } else {
                for (const auto &subtitle : m_subtitleList) {
                    ptr->removePoint(subtitle.first.frames(pCore->getCurrentFps()));
                    ptr->removePoint(subtitle.second.second.frames(pCore->getCurrentFps()));
                }
            }
        }
    }
    // Update the list of snapModel known to be valid
    std::swap(m_regSnaps, validSnapModels);
    if (!isLocked) {
        // Clear selection
        while (!m_selected.isEmpty()) {
            int id = m_selected.takeFirst();
            updateSub(id, {SelectedRole});
        }
    }*/
}


bool SubtitleModel::isDisabled() const
{
    return m_subtitleFilter->get_int("disable") == 1;
}

bool SubtitleModel::isLocked() const
{
    return m_subtitleFilter->get_int("kdenlive:locked") == 1;
}

void SubtitleModel::loadProperties(QMap<QString, QString> subProperties)
{
    QMap<QString, QString>::const_iterator i = subProperties.constBegin();
    while (i != subProperties.constEnd()) {
        if (!i.value().isEmpty()) {
            m_subtitleFilter->set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
        }
        ++i;
    }
}

void SubtitleModel::allSnaps(std::vector<int> &snaps)
{
    for (const auto &subtitle : m_subtitleList) {
        snaps.push_back(subtitle.first.frames(pCore->getCurrentFps()));
        snaps.push_back(subtitle.second.second.frames(pCore->getCurrentFps()));
    }
}

QDomElement SubtitleModel::toXml(int sid, QDomDocument &document)
{
    GenTime startPos = m_timeline->m_allSubtitles.at(sid);
    int endPos = m_subtitleList.at(startPos).second.frames(pCore->getCurrentFps());
    QDomElement container = document.createElement(QStringLiteral("subtitle"));
    container.setAttribute(QStringLiteral("in"), startPos.frames(pCore->getCurrentFps()));
    container.setAttribute(QStringLiteral("out"), endPos);
    container.setAttribute(QStringLiteral("text"), m_subtitleList.at(startPos).first);
    return container;
}


int SubtitleModel::getBlankSizeAtPos(int pos) const
{
    GenTime matchPos(pos, pCore->getCurrentFps());
    std::unordered_set<int> matching;
    GenTime min;
    GenTime max;
    for (const auto &subtitles : m_subtitleList) {
        if (subtitles.first > matchPos && (max == GenTime() ||  subtitles.first < max)) {
            // Outside range
            max = subtitles.first;
        }
        if (subtitles.second.second < matchPos && (min == GenTime() ||  subtitles.second.second > min)) {
            min = subtitles.second.second;
        }
    }
    return max.frames(pCore->getCurrentFps()) - min.frames(pCore->getCurrentFps());
}
