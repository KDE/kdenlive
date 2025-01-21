/*
    SPDX-FileCopyrightText: 2020 Sashmita Raghav
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "subtitlemodel.hpp"
#include "bin/bin.h"
#include "config-kdenlive.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/snapmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "undohelper.hpp"

#include <mlt++/Mlt.h>
#include <mlt++/MltProperties.h>

#include <KEncodingProber>
#include <KLocalizedString>
#include <KMessageBox>
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringConverter>
#include <utility>

SubtitleModel::SubtitleModel(std::shared_ptr<TimelineItemModel> timeline, const std::weak_ptr<SnapInterface> &snapModel, QObject *parent)
    : QAbstractListModel(parent)
    , m_timeline(timeline)
    , m_lock(QReadWriteLock::Recursive)
    , m_subtitleFilter(new Mlt::Filter(pCore->getProjectProfile(), "avfilter.subtitles"))
{
    qDebug() << "Subtitle constructor";
    // Ensure the subtitle also covers transparent zones (useful for timeline sequences)
    m_subtitleFilter->set("av.alpha", 1);
    if (m_timeline->tractor() != nullptr) {
        qDebug() << "Tractor!";
        m_subtitleFilter->set("internal_added", 237);
    }
    setup();
    connect(this, &SubtitleModel::modelChanged, [this]() { jsontoSubtitle(toJson()); });

    const QUuid timelineUuid = timeline->uuid();
    int id = pCore->currentDoc()->getSequenceProperty(timelineUuid, QStringLiteral("kdenlive:activeSubtitleIndex"), QStringLiteral("0")).toInt();
    const QString subPath = pCore->currentDoc()->subTitlePath(timelineUuid, id, true);
    const QString workPath = pCore->currentDoc()->subTitlePath(timelineUuid, id, false);
    registerSnap(snapModel);

    // Load global subtitle styles
    m_globalSubtitleStyles = pCore->currentDoc()->globalSubtitleStyles(timeline->uuid());

    QFile subFile(subPath);
    if (pCore->currentDoc()->m_restoreFromBackup) {
        QString tmpWorkPath = pCore->currentDoc()->subTitlePath(timelineUuid, id, false, true);
        QFile prevSub(tmpWorkPath);
        if (prevSub.exists()) {
            if (!subFile.exists()) {
                prevSub.copy(subPath);
            }
            prevSub.copy(workPath);
        }
    } else {
        if (subFile.exists()) {
            subFile.copy(workPath);
        }
    }

    if (!QFile::exists(workPath)) {
        QFile newSub(workPath);
        newSub.open(QIODevice::WriteOnly);
        newSub.close();
        qDebug() << "MISSING SUBTITLE FILE, create tmp: " << workPath;
    }

    registerSnap(snapModel);

    // Load multiple subtitle data
    QMap<std::pair<int, QString>, QString> multiSubs = pCore->currentDoc()->multiSubtitlePath(timelineUuid);
    m_subtitlesList = multiSubs;
    if (m_subtitlesList.isEmpty()) {
        m_subtitlesList.insert({0, i18n("Subtitles")}, subPath);
    }

    parseSubtitle(workPath);
}

void SubtitleModel::setForceStyle(const QString &style)
{
    QString oldStyle = m_subtitleFilter->get("av.force_style");
    Fun redo = [this, style]() {
        m_subtitleFilter->set("av.force_style", style.toUtf8().constData());
        // Force refresh to show the new style
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    Fun undo = [this, oldStyle]() {
        m_subtitleFilter->set("av.force_style", oldStyle.toUtf8().constData());
        // Force refresh to show the new style
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, i18n("Edit global subtitle style"));
}

const QString SubtitleModel::getForceStyle() const
{
    const QString style = m_subtitleFilter->get("av.force_style");
    return style;
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

QByteArray SubtitleModel::guessFileEncoding(const QString &file, bool *confidence)
{
    QFile textFile{file};
    if (!textFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open" << file;
        return "";
    }
    KEncodingProber prober;
    QByteArray sample = textFile.read(1024);
    if (sample.isEmpty()) {
        qWarning() << "Tried to guess the encoding of an empty file";
        return "";
    }
    auto state = prober.feed(sample);
    *confidence = false;
    switch (state) {
        case KEncodingProber::ProberState::FoundIt:
            qDebug() << "Guessed subtitle file encoding to be " << prober.encoding() << ", confidence: " << prober.confidence();
            if (prober.confidence() < 0.6) {
                return QByteArray("UTF-8");
            }
            *confidence = true;
            break;
        case KEncodingProber::ProberState::NotMe:
            qWarning() << "Subtitle file encoding not recognized";
            return QByteArray("UTF-8");
        case KEncodingProber::ProberState::Probing:
            qWarning() << "Subtitle file encoding indeterminate, confidence is" << prober.confidence() << ", ENCODING: " << prober.encoding();
            if (prober.confidence() < 0.5) {
                // Encoding cannot be guessed, default to UTF-8
                return QByteArray(QByteArray("UTF-8"));
            }
            break;
    }
    return prober.encoding();
}

void SubtitleModel::importSubtitle(const QString &filePath, int offset, bool externalImport, float startFramerate, float targetFramerate, const QByteArray &encoding)
{
    QString start, end, comment;
    QString timeLine;
    GenTime startPos, endPos;
    int turn = 0, r = 0, endIndex = 1, defaultTurn = 0;
    double transformMult = targetFramerate/startFramerate;
    /*
     * turn = 0 -> Parse next subtitle line [srt] (or) [vtt] (or) [sbv] (or) Parse next section [ssa]
     * turn = 1 -> Add string to timeLine
     * turn > 1 -> Add string to completeLine
     */
    if (filePath.isEmpty() || isLocked()) return;
    Fun redo = []() { return true; };
    Fun undo = [this]() {
        Q_EMIT modelChanged();
        return true;
    };
    ulong initialCount = m_subtitleList.size();
    GenTime subtitleOffset(offset, pCore->getCurrentFps());

    if (!externalImport) {
        // initialize the subtitle
        m_scriptInfo.clear();
        m_subtitleStyles.clear();
        fontSection.clear();
        m_defaultStyles.clear();
        setMaxLayer(0);

        QSize frameSize = pCore->getCurrentFrameDisplaySize();
        double fontSize = frameSize.height() / 18;
        double fontMargin = frameSize.height() / 27;

        m_scriptInfo["ScriptType"] = "v4.00+";
        m_scriptInfo["PlayResX"] = QString::number(frameSize.width());
        m_scriptInfo["PlayResY"] = QString::number(frameSize.height());
        m_scriptInfo["LayoutResX"] = QString::number(frameSize.width());
        m_scriptInfo["LayoutResY"] = QString::number(frameSize.height());
        m_scriptInfo["WrapStyle"] = "0";
        m_scriptInfo["ScaledBorderAndShadow"] = "yes";
        m_scriptInfo["YCbCr Matrix"] = "None";

        m_subtitleStyles["Default"] = SubtitleStyle();
        m_subtitleStyles["Default"].setFontSize(fontSize);
        m_subtitleStyles["Default"].setMarginV(fontMargin);
    }

    if (filePath.endsWith(".srt") || filePath.endsWith(".vtt") || filePath.endsWith(".sbv")) {
        // if (!filePath.endsWith(".vtt") || !filePath.endsWith(".sbv")) {defaultTurn = -10;}
        if (filePath.endsWith(".vtt") || filePath.endsWith(".sbv")) {
            defaultTurn = -10;
            turn = defaultTurn;
        }
        endIndex = filePath.endsWith(".sbv") ? 1 : 2;
        QFile srtFile(filePath);
        if (!srtFile.exists() || !srtFile.open(QIODevice::ReadOnly)) {
            qDebug() << " File not found " << filePath;
            return;
        }

        qDebug() << "srt/vtt/sbv File";
        //parsing srt file
        QTextStream stream(&srtFile);
        std::optional<QStringConverter::Encoding> inputEncoding = QStringConverter::encodingForName(encoding.data());
        if (inputEncoding) {
            stream.setEncoding(inputEncoding.value());
        }
        // else: UTF8 is the default
        QString line;
        QStringList srtTime;
        static const QRegularExpression rx("([0-9]{1,2}):([0-9]{2})");
        QLatin1Char separator = filePath.endsWith(".sbv") ? QLatin1Char(',') : QLatin1Char(' ');
        while (stream.readLineInto(&line)) {
            line = line.trimmed();
            // qDebug()<<"Turn: "<<turn;
            // qDebug()<<"Line: "<<line;
            if (!line.isEmpty()) {
                if (!turn) {
                    // index=atoi(line.toStdString().c_str());
                    turn++;
                    continue;
                }
                // Check if position has already been read
                if (endPos == startPos && (line.contains(QLatin1String("-->")) || line.contains(rx))) {
                    timeLine += line;
                    srtTime = timeLine.split(separator);
                    if (srtTime.count() > endIndex) {
                        start = srtTime.at(0);
                        startPos = SubtitleEvent::stringtoTime(start, pCore->getCurrentFps(), transformMult);
                        end = srtTime.at(endIndex);
                        endPos = SubtitleEvent::stringtoTime(end, pCore->getCurrentFps(), transformMult);
                    } else {
                        continue;
                    }
                } else {
                    r++;
                    if (!comment.isEmpty()) comment += " ";
                    if (r == 1)
                        comment += line;
                    else
                        comment = comment + "\n" + line;
                }
                turn++;
            } else {
                if (endPos > startPos) {
                    addSubtitle({0, startPos + subtitleOffset}, SubtitleEvent(true, endPos + subtitleOffset, "Default", "", 0, 0, 0, "", comment), undo, redo,
                                false);
                    // qDebug() << "Adding Subtitle: \n  Start time: " << start << "\n  End time: " << end << "\n  Text: " << comment;
                } else {
                    qDebug() << "===== INVALID SUBTITLE FOUND: " << start << "-" << end << ", " << comment;
                }
                // reinitialize
                comment.clear();
                timeLine.clear();
                startPos = endPos;
                r = 0;
                turn = defaultTurn;
            }
        }
        // Ensure last subtitle is read
        if (endPos > startPos && !comment.isEmpty()) {
            addSubtitle({0, startPos + subtitleOffset}, SubtitleEvent(true, endPos + subtitleOffset, "Default", "", 0, 0, 0, "", comment), undo, redo, false);
        }
        srtFile.close();
    } else if (filePath.endsWith(QLatin1String(".ass"))) {
        qDebug() << "ass File";
        QString startTime, endTime;
        QString section;
        turn = 0;
        QFile assFile(filePath);
        if (!assFile.exists() || !assFile.open(QIODevice::ReadOnly)) {
            qDebug() << " Failed attempt on opening " << filePath;
            return;
        }
        QTextStream stream(&assFile);
        std::optional<QStringConverter::Encoding> inputEncoding = QStringConverter::encodingForName(encoding.data());
        if (inputEncoding) {
            stream.setEncoding(inputEncoding.value());
        }
        // else: UTF8 is the default
        QString line;
        qDebug() << " correct ass file  " << filePath;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (!line.isEmpty()) {
                if (!turn) {
                    // qDebug() << " turn = 0  " << line;
                    // check if it is script info, event,or v4+ style
                    QString linespace = line;
                    if (linespace.replace(" ", "").contains("ScriptInfo")) {
                        // qDebug()<< "Script Info";
                        section = "Script Info";
                        turn++;
                        // qDebug()<< "turn" << turn;
                        continue;
                    } else if (linespace.contains("KdenliveExtradata")) {
                        // qDebug()<< "KdenliveExtradata";
                        section = "Kdenlive Extradata";
                        turn++;
                        // qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Styles")) {
                        // qDebug()<< "V4+ Styles";
                        section = "V4+ Styles";
                        turn++;
                        // qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Fonts")) {
                        section = "Fonts";
                        turn++;
                        // qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Events")) {
                        turn++;
                        section = "Events";
                        // qDebug()<< "turn" << turn;
                        continue;
                    } else {
                        // unknown section
                        section.clear();
                    }
                }
                if (section.contains("Script Info")) {
                    QStringList scriptInfo = line.split(":");
                    if (scriptInfo.count() > 1) {
                        m_scriptInfo[line.section(":", 0, 0).trimmed()] = line.section(":", 1).trimmed();
                    }
                }
                if (section.contains("Kdenlive Extradata")) {
                    QStringList extraData = line.split(":");
                    if (extraData[0].trimmed() == "MaxLayer") {
                        if (extraData[1].toInt() > m_maxLayer) setMaxLayer(extraData[1].toInt());
                    } else if (extraData[0].trimmed() == "DefaultStyles") {
                        const QStringList defaultStyles = extraData[1].trimmed().split(",");
                        m_defaultStyles.clear();
                        for (const QString &style : defaultStyles) {
                            m_defaultStyles << style;
                        }
                        // Fill the default styles with "Default" if not enough styles are defined
                        for (int i = m_defaultStyles.size(); i < m_maxLayer + 1; i++) {
                            m_defaultStyles << "Default";
                        }
                    }
                }
                if (section.contains("V4+ Styles")) {
                    if (!line.contains("Format:")) {
                        m_subtitleStyles[line.section(":", 1).trimmed().split(",").at(0)] = SubtitleStyle(line);
                    }
                }
                // qDebug() << "\n turn != 0  " << turn<< line;
                if (section.contains("Fonts")) {
                    fontSection += line + "\n";
                }
                if (section.contains("Events")) {
                    // if it is event
                    if (!line.contains("Format:")) {
                        start.clear();
                        end.clear();
                        comment.clear();
                        std::pair<int, GenTime> start;
                        line.replace(QStringLiteral("\\N"), QStringLiteral("\n"));
                        SubtitleEvent event(line, pCore->getCurrentFps(), transformMult, &start);
                        // check if event successfully parsed
                        if (event.endTime() != GenTime()) {
                            if (start.first > m_maxLayer) {
                                setMaxLayer(start.first);
                            }
                            start.second += subtitleOffset;
                            event.setEndTime(event.endTime() + subtitleOffset);
                            addSubtitle(start, event, undo, redo, false);
                        } else {
                            qDebug() << "==== FOUND INVALID SUBTITLE ITEM: " << line;
                        }
                    }
                }
                turn++;
            } else {
                turn = 0;
                startTime = endTime = QString();
            }
        }
        assFile.close();
    } else {
        if (endPos > startPos) {
            // addSubtitle({0, startPos + subtitleOffset}, endPos + subtitleOffset, event, undo, redo, false);
        } else {
            // qDebug() << "===== INVALID VTT SUBTITLE FOUND: " << start << "-" << end << ", " << event;
        }
        //   reinitialize for next comment:
        comment.clear();
        timeLine.clear();
        // event.clear();
        turn = 0;
        r = 0;
    }
    if (initialCount == m_subtitleList.size() && externalImport) {
        // Nothing imported
        pCore->displayMessage(i18n("The selected file %1 is invalid.", filePath), ErrorMessage);
        return;
    }
    Fun update_model = [this]() {
        Q_EMIT modelChanged();
        return true;
    };
    PUSH_LAMBDA(update_model, redo);
    update_model();
    if (externalImport) {
        pCore->pushUndo(undo, redo, i18n("Edit subtitle"));
    }
}

void SubtitleModel::parseSubtitle(const QString &workPath)
{
    qDebug() << "Parsing started for: " << workPath;
    if (!workPath.isEmpty()) {
        m_subtitleFilter->set("av.filename", workPath.toUtf8().constData());
    }
    QString filePath = m_subtitleFilter->get("av.filename");
    importSubtitle(filePath, 0, false);
    // jsontoSubtitle(toJson());
}

const QString SubtitleModel::getUrl()
{
    return m_subtitleFilter->get("av.filename");
}

bool SubtitleModel::addSubtitle(std::pair<int, GenTime> start, const SubtitleEvent &event, Fun &undo, Fun &redo, bool updateFilter)
{
    if (isLocked()) {
        return false;
    }
    int id = TimelineModel::getNextId();
    Fun local_redo = [this, id, start, event, updateFilter]() {
        addSubtitle(id, start, event, false, updateFilter);
        QPair<int, int> range = {start.second.frames(pCore->getCurrentFps()), event.endTime().frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    Fun local_undo = [this, id, start, event, updateFilter]() {
        removeSubtitle(id, false, updateFilter);
        QPair<int, int> range = {start.second.frames(pCore->getCurrentFps()), event.endTime().frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    local_redo();
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool SubtitleModel::addSubtitle(int id, std::pair<int, GenTime> start, const SubtitleEvent &event, bool temporary, bool updateFilter)
{
    if (start.second.frames(pCore->getCurrentFps()) < 0 || event.endTime().frames(pCore->getCurrentFps()) < 0 || isLocked()) {
        qDebug() << "Time error: is negative";
        return false;
    }
    if (start.second.frames(pCore->getCurrentFps()) > event.endTime().frames(pCore->getCurrentFps())) {
        qDebug() << "Time error: start should be less than end";
        return false;
    }
    // Don't allow 2 subtitles at same start pos
    if (m_subtitleList.count(start) > 0) {
        qDebug() << "already present in model"
                 << "string :" << m_subtitleList[start].text() << " start time " << start.second.frames(pCore->getCurrentFps())
                 << "end time : " << m_subtitleList[start].endTime().frames(pCore->getCurrentFps());
        return false;
    }
    if (start.first > m_maxLayer) {
        qDebug() << "Layer not allowed";
        return false;
    }
    if (start.first < 0) {
        qDebug() << "negative layer";
        return false;
    }
    registerSubtitle(id, start, temporary);
    int row = getSubtitleIndex(id);
    beginInsertRows(QModelIndex(), row, row);
    m_subtitleList[start] = event;
    endInsertRows();
    addSnapPoint(start.second);
    addSnapPoint(event.endTime()); // {layer, end}
    if (!temporary && event.endTime().frames(pCore->getCurrentFps()) > m_timeline->duration()) {
        m_timeline->updateDuration();
    }
    // qDebug() << "Added to model";
    if (updateFilter) {
        Q_EMIT modelChanged();
    }
    return true;
}

QHash<int, QByteArray> SubtitleModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SubtitleRole] = "subtitle";
    roles[IdRole] = "id";
    roles[StartPosRole] = "startposition";
    roles[EndPosRole] = "endposition";
    roles[StartFrameRole] = "startframe";
    roles[FakeStartFrameRole] = "fakeStart";
    roles[EndFrameRole] = "endframe";
    roles[LayerRole] = "layer";
    roles[StyleNameRole] = "styleName";
    roles[NameRole] = "name";
    roles[MarginLRole] = "marginL";
    roles[MarginRRole] = "marginR";
    roles[MarginVRole] = "marginV";
    roles[EffectRole] = "effect";
    roles[IsDialogueRole] = "isDialogue";
    roles[GrabRole] = "grabbed";
    roles[SelectedRole] = "selected";
    return roles;
}

QVariant SubtitleModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= static_cast<int>(m_subtitleList.size()) || !index.isValid()) {
        return QVariant();
    }
    auto subInfo = getSubtitleIdFromIndex(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case SubtitleRole:
        return m_subtitleList.at(subInfo.second).text();
    case IdRole:
        return subInfo.first;
    case LayerRole:
        return subInfo.second.first;
    case StartPosRole:
        return subInfo.second.second.seconds();
    case EndPosRole:
        return m_subtitleList.at(subInfo.second).endTime().seconds();
    case StartFrameRole:
        return subInfo.second.second.frames(pCore->getCurrentFps());
    case EndFrameRole:
        return m_subtitleList.at(subInfo.second).endTime().frames(pCore->getCurrentFps());
    case StyleNameRole:
        return m_subtitleList.at(subInfo.second).styleName();
    case NameRole:
        return m_subtitleList.at(subInfo.second).name();
    case MarginLRole:
        return m_subtitleList.at(subInfo.second).marginL();
    case MarginRRole:
        return m_subtitleList.at(subInfo.second).marginR();
    case MarginVRole:
        return m_subtitleList.at(subInfo.second).marginV();
    case EffectRole:
        return m_subtitleList.at(subInfo.second).effect();
    case IsDialogueRole:
        return m_subtitleList.at(subInfo.second).isDialogue();
    case SelectedRole:
        return m_selected.contains(subInfo.first);
    case FakeStartFrameRole:
        return getSubtitleFakePosFromIndex(index.row());
    case GrabRole:
        return m_grabbedIds.contains(subInfo.first);
    }
    return QVariant();
}

int SubtitleModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_subtitleList.size());
}

const QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> SubtitleModel::getAllSubtitles() const
{
    QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubtitles;
    for (const auto &subtitles : m_subtitleList) {
        allSubtitles << subtitles;
    }
    return allSubtitles;
}

SubtitleEvent SubtitleModel::getSubtitle(int layer, GenTime startpos) const
{
    for (const auto &subtitles : m_subtitleList) {
        if (subtitles.first == std::make_pair(layer, startpos)) {
            return subtitles.second;
        }
    }
    return SubtitleEvent();
}

QString SubtitleModel::getText(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) {
        return QString();
    }
    std::pair<int, GenTime> start = m_allSubtitles.at(id);
    return m_subtitleList.at(start).text();
}

bool SubtitleModel::setText(int id, const QString &text)
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end() || isLocked()) {
        return false;
    }
    std::pair<int, GenTime> start = m_allSubtitles.at(id);
    GenTime end = m_subtitleList.at(start).endTime();
    QString oldText = m_subtitleList.at(start).text();
    m_subtitleList[start].setText(text);
    Fun local_redo = [this, start, id, end, text]() {
        editSubtitle(id, text);
        QPair<int, int> range = {start.second.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    Fun local_undo = [this, start, id, end, oldText]() {
        editSubtitle(id, oldText);
        QPair<int, int> range = {start.second.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
    return true;
}

std::unordered_set<int> SubtitleModel::getItemsInRange(int layer, int startFrame, int endFrame) const
{
    if (isLocked()) {
        return {};
    }
    GenTime startTime(startFrame, pCore->getCurrentFps());
    GenTime endTime(endFrame, pCore->getCurrentFps());
    std::unordered_set<int> matching;
    for (const auto &subtitles : m_subtitleList) {
        // if layer is -1, we check all layers
        if ((endFrame > -1 && subtitles.first.second > endTime) || (layer != -1 && subtitles.first.first != layer)) {
            // Outside range
            continue;
        }
        if (subtitles.first.second >= startTime || subtitles.second.endTime() > startTime) {
            int sid = getIdForStartPos(layer, subtitles.first.second);
            if (sid > -1) {
                matching.emplace(sid);
            } else {
                qDebug() << "==== FOUND INVALID SUBTILE AT: " << subtitles.first.second.frames(pCore->getCurrentFps());
            }
        }
    }
    return matching;
}

bool SubtitleModel::cutSubtitle(int layer, int position)
{
    Fun redo = []() { return true; };
    Fun undo = []() { return true; };
    if (cutSubtitle(layer, position, undo, redo) > -1) {
        pCore->pushUndo(undo, redo, i18n("Cut clip"));
        return true;
    }
    return false;
}

int SubtitleModel::cutSubtitle(int layer, int position, Fun &undo, Fun &redo)
{
    if (isLocked()) {
        return -1;
    }
    GenTime pos(position, pCore->getCurrentFps());
    GenTime start = GenTime(-1);
    for (const auto &subtitles : m_subtitleList) {
        if (subtitles.first.second <= pos && subtitles.second.endTime() > pos) {
            start = subtitles.first.second;
            break;
        }
    }
    if (start >= GenTime()) {
        const SubtitleEvent originalEvent = m_subtitleList.at({layer, start});
        QString originalText = originalEvent.text();
        QString leftText, rightText;

        if (KdenliveSettings::subtitle_razor_mode() == RAZOR_MODE_DUPLICATE) {
            leftText = originalText;
            rightText = originalText;
        } else if (KdenliveSettings::subtitle_razor_mode() == RAZOR_MODE_AFTER_FIRST_LINE) {
            static const QRegularExpression newlineRe("\\n");
            QRegularExpressionMatch newlineMatch = newlineRe.match(originalText);
            if (!newlineMatch.hasMatch()) {
                undo();
                return -1;
            } else {
                leftText = originalText;
                leftText.truncate(newlineMatch.capturedStart());
                rightText = originalText.right(originalText.length() - newlineMatch.capturedEnd());
            }
        } else {
            undo();
            return -1;
        }

        int subId = getIdForStartPos(layer, start);
        int duration = position - start.frames(pCore->getCurrentFps());
        bool res = requestResize(subId, duration, true, undo, redo, false);
        if (res) {
            int id = TimelineModel::getNextId();
            Fun local_redo = [this, id, layer, pos, originalEvent, subId, leftText, rightText]() {
                editSubtitle(subId, leftText);
                return addSubtitle(id, {layer, pos},
                                   SubtitleEvent(originalEvent.isDialogue(), originalEvent.endTime(), originalEvent.styleName(), originalEvent.name(),
                                                 originalEvent.marginL(), originalEvent.marginR(), originalEvent.marginV(), originalEvent.effect(), rightText));
            };
            Fun local_undo = [this, id, subId, originalText]() {
                editSubtitle(subId, originalText);
                removeSubtitle(id);
                return true;
            };
            if (local_redo()) {
                UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
                return id;
            }
        }
    }
    undo();
    return -1;
}

void SubtitleModel::registerSnap(const std::weak_ptr<SnapInterface> &snapModel)
{
    // make sure ptr is valid
    if (auto ptr = snapModel.lock()) {
        // ptr is valid, we store it
        m_regSnaps.push_back(snapModel);
        // we now add the already existing subtitles to the snap
        for (const auto &subtitle : m_subtitleList) {
            ptr->addPoint(subtitle.first.second.frames(pCore->getCurrentFps()));
        }
    } else {
        qDebug() << "Error: added snapmodel for subtitle is null";
        Q_ASSERT(false);
    }
}

void SubtitleModel::addSnapPoint(GenTime startpos)
{
    std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    Q_ASSERT(m_regSnaps.size() > 0);
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

void SubtitleModel::editEndPos(int layer, GenTime startPos, GenTime newEndPos, bool refreshModel)
{
    qDebug() << "Changing the sub end timings in model";
    if (m_subtitleList.count({layer, startPos}) <= 0) {
        // is not present in model only
        return;
    }
    m_subtitleList[{layer, startPos}].setEndTime(newEndPos);
    // Trigger update of the qml view
    int id = getIdForStartPos(layer, startPos);
    int row = getSubtitleIndex(id);
    Q_EMIT dataChanged(index(row), index(row), {EndFrameRole});
    if (refreshModel) {
        Q_EMIT modelChanged();
    }
    qDebug() << startPos.frames(pCore->getCurrentFps()) << m_subtitleList[{layer, startPos}].endTime().frames(pCore->getCurrentFps());
}

void SubtitleModel::switchGrab(int sid)
{
    if (m_grabbedIds.contains(sid)) {
        m_grabbedIds.removeAll(sid);
    } else {
        m_grabbedIds << sid;
    }
    int row = getSubtitleIndex(sid);
    Q_EMIT dataChanged(index(row), index(row), {GrabRole});
}

void SubtitleModel::clearGrab()
{
    QVector<int> grabbed = m_grabbedIds;
    m_grabbedIds.clear();
    for (int sid : grabbed) {
        int row = getSubtitleIndex(sid);
        Q_EMIT dataChanged(index(row), index(row), {GrabRole});
    }
}

bool SubtitleModel::isGrabbed(int id) const
{
    return m_grabbedIds.contains(id);
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
    Q_ASSERT(m_allSubtitles.find(id) != m_allSubtitles.end());
    std::pair<int, GenTime> startPos = m_allSubtitles.at(id);
    GenTime endPos = m_subtitleList.at(startPos).endTime();
    Fun operation = []() { return true; };
    Fun reverse = []() { return true; };
    if (right) {
        GenTime newEndPos = startPos.second + GenTime(size, pCore->getCurrentFps());
        operation = [this, id, startPos, endPos, newEndPos, logUndo]() {
            m_subtitleList[startPos].setEndTime(newEndPos);
            removeSnapPoint(endPos);
            addSnapPoint(newEndPos);
            // Trigger update of the qml view
            int row = getSubtitleIndex(id);
            Q_EMIT dataChanged(index(row), index(row), {EndFrameRole});
            if (logUndo) {
                Q_EMIT modelChanged();
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
            m_subtitleList[startPos].setEndTime(endPos);
            removeSnapPoint(newEndPos);
            addSnapPoint(endPos);
            // Trigger update of the qml view
            int row = getSubtitleIndex(id);
            Q_EMIT dataChanged(index(row), index(row), {EndFrameRole});
            if (logUndo) {
                Q_EMIT modelChanged();
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
        std::pair<int, GenTime> newStartPos = {startPos.first, endPos - GenTime(size, pCore->getCurrentFps())};
        if (m_subtitleList.count(newStartPos) > 0) {
            // There already is another subtitle at this position, abort
            return false;
        }
        const SubtitleEvent event = m_subtitleList.at(startPos);
        operation = [this, id, startPos, newStartPos, event, logUndo]() {
            m_allSubtitles[id] = newStartPos;
            m_subtitleList.erase(startPos);
            m_subtitleList[newStartPos] = event;
            // Trigger update of the qml view
            removeSnapPoint(startPos.second);
            addSnapPoint(newStartPos.second);
            int row = getSubtitleIndex(id);
            Q_EMIT dataChanged(index(row), index(row), {StartFrameRole});
            if (logUndo) {
                Q_EMIT modelChanged();
                QPair<int, int> range;
                if (startPos > newStartPos) {
                    range = {newStartPos.second.frames(pCore->getCurrentFps()), startPos.second.frames(pCore->getCurrentFps())};
                } else {
                    range = {startPos.second.frames(pCore->getCurrentFps()), newStartPos.second.frames(pCore->getCurrentFps())};
                }
                pCore->invalidateRange(range);
                pCore->refreshProjectRange(range);
            }
            return true;
        };
        reverse = [this, id, startPos, newStartPos, event, logUndo]() {
            m_allSubtitles[id] = startPos;
            m_subtitleList.erase(newStartPos);
            m_subtitleList[startPos] = event;
            removeSnapPoint(newStartPos.second);
            addSnapPoint(startPos.second);
            // Trigger update of the qml view
            int row = getSubtitleIndex(id);
            Q_EMIT dataChanged(index(row), index(row), {StartFrameRole});
            if (logUndo) {
                Q_EMIT modelChanged();
                QPair<int, int> range;
                if (startPos > newStartPos) {
                    range = {newStartPos.second.frames(pCore->getCurrentFps()), startPos.second.frames(pCore->getCurrentFps())};
                } else {
                    range = {startPos.second.frames(pCore->getCurrentFps()), newStartPos.second.frames(pCore->getCurrentFps())};
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

bool SubtitleModel::editSubtitle(int id, const QString &newSubtitleText)
{
    if (isLocked()) {
        return false;
    }
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) {
        qDebug() << "No Subtitle at pos in model";
        return false;
    }
    std::pair<int, GenTime> start = m_allSubtitles.at(id);
    if (m_subtitleList.count(start) <= 0) {
        qDebug() << "No Subtitle at pos in model";
        return false;
    }

    qDebug() << "Editing existing subtitle in model";
    m_subtitleList[start].setText(newSubtitleText);
    int row = getSubtitleIndex(id);
    // TODO

    Q_EMIT dataChanged(index(row), index(row), QVector<int>() << SubtitleRole);
    Q_EMIT modelChanged();
    return true;
}

bool SubtitleModel::removeSubtitle(int id, bool temporary, bool updateFilter)
{
    qDebug() << "Deleting subtitle in model";
    if (isLocked()) {
        return false;
    }
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) {
        qDebug() << "No Subtitle at pos in model";
        return false;
    }
    std::pair<int, GenTime> start = m_allSubtitles.at(id);
    if (m_subtitleList.count(start) <= 0) {
        qDebug() << "No Subtitle at pos in model";
        return false;
    }
    GenTime end = m_subtitleList.at(start).endTime();
    int row = getSubtitleIndex(id);
    deregisterSubtitle(id, temporary);
    beginRemoveRows(QModelIndex(), row, row);
    bool lastSub = false;
    if (start == m_subtitleList.rbegin()->first) {
        // Check if this is the last subtitle
        lastSub = true;
    }
    m_subtitleList.erase(start);
    endRemoveRows();
    removeSnapPoint(start.second);
    removeSnapPoint(end);
    if (lastSub) {
        m_timeline->updateDuration();
    }
    if (updateFilter) {
        Q_EMIT modelChanged();
    }
    return true;
}

void SubtitleModel::removeAllSubtitles()
{
    if (isLocked()) {
        return;
    }
    auto ids = m_allSubtitles;
    for (const auto &p : ids) {
        removeSubtitle(p.first);
    }
}

void SubtitleModel::requestSubtitleMove(int clipId, int newLayer, GenTime position)
{
    int oldLayer = getLayerForId(clipId);
    GenTime oldPos = getStartPosForId(clipId);
    Fun local_redo = [this, newLayer, clipId, position]() { return moveSubtitle(clipId, newLayer, position, true, true); };
    Fun local_undo = [this, oldLayer, clipId, oldPos]() { return moveSubtitle(clipId, oldLayer, oldPos, true, true); };
    bool res = local_redo();
    if (res) {
        pCore->pushUndo(local_undo, local_redo, i18n("Move subtitle"));
    }
}

bool SubtitleModel::moveSubtitle(int subId, int newLayer, GenTime newPos, bool updateModel, bool updateView)
{
    if (m_allSubtitles.count(subId) == 0 || isLocked()) {
        return false;
    }
    int oldLayer = getLayerForId(subId);
    GenTime oldPos = getStartPosForId(subId);
    if (m_subtitleList.count({oldLayer, oldPos}) <= 0 || m_subtitleList.count({newLayer, newPos}) > 0) {
        // is not present in model, or already another one at new position
        qDebug() << "==== MOVE FAILED";
        return false;
    }
    const SubtitleEvent event = m_subtitleList[{oldLayer, oldPos}];
    removeSnapPoint(oldPos);
    removeSnapPoint(event.endTime());
    GenTime duration = event.endTime() - oldPos;
    GenTime endPos = newPos + duration;
    int id = getIdForStartPos(oldLayer, oldPos);
    if (newLayer > m_maxLayer) {
        setMaxLayer(newLayer);
    }
    m_allSubtitles[id] = {newLayer, newPos};
    m_subtitleList.erase({oldLayer, oldPos});
    m_subtitleList[{newLayer, newPos}] = event;
    m_subtitleList[{newLayer, newPos}].setEndTime(endPos);
    addSnapPoint(newPos);
    addSnapPoint(endPos);
    setActiveSubLayer(newLayer);
    if (updateView) {
        updateSub(id, {StartFrameRole, EndFrameRole, LayerRole});
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
        Q_EMIT modelChanged();
        if (newPos == m_subtitleList.rbegin()->first.second) {
            // Check if this is the last subtitle
            m_timeline->updateDuration();
        }
    }
    return true;
}

int SubtitleModel::getIdForStartPos(int layer, GenTime startTime) const
{
    auto findResult = std::find_if(std::begin(m_allSubtitles), std::end(m_allSubtitles), [&](const std::pair<int, std::pair<int, GenTime>> &pair) {
        return pair.second.second == startTime && (pair.second.first == layer || layer == -1);
    });
    if (findResult != std::end(m_allSubtitles)) {
        return findResult->first;
    }
    return -1;
}

int SubtitleModel::getLayerForId(int id) const
{
    if (m_allSubtitles.count(id) == 0) {
        return -1;
    }
    return m_allSubtitles.at(id).first;
}

GenTime SubtitleModel::getStartPosForId(int id) const
{
    if (m_allSubtitles.count(id) == 0) {
        return GenTime();
    };
    return m_allSubtitles.at(id).second;
}

int SubtitleModel::getPreviousSub(int id) const
{
    GenTime start = getStartPosForId(id);
    int layer = getLayerForId(id);
    int row = static_cast<int>(std::distance(m_subtitleList.begin(), m_subtitleList.find({layer, start})));
    if (row > 0) {
        row--;
        auto it = m_subtitleList.begin();
        std::advance(it, row);
        const GenTime res = it->first.second;
        return getIdForStartPos(layer, res);
    }
    return -1;
}

int SubtitleModel::getNextSub(int id) const
{
    GenTime start = getStartPosForId(id);
    int layer = getLayerForId(id);
    int row = static_cast<int>(std::distance(m_subtitleList.begin(), m_subtitleList.find({layer, start})));
    if (row < static_cast<int>(m_subtitleList.size()) - 1) {
        row++;
        auto it = m_subtitleList.begin();
        std::advance(it, row);
        const GenTime res = it->first.second;
        return getIdForStartPos(layer, res);
    }
    return -1;
}

void SubtitleModel::subtitleFileFromZone(int in, int out, const QString &outFile)
{
    QJsonArray list;
    double fps = pCore->getCurrentFps();
    GenTime zoneIn(in, fps);
    GenTime zoneOut(out, fps);
    for (const auto &subtitle : m_subtitleList) {
        int layer = subtitle.first.first;
        GenTime inTime = subtitle.first.second;
        GenTime outTime = subtitle.second.endTime();
        if (outTime < zoneIn) {
            // Outside zone
            continue;
        }
        if (zoneOut > GenTime() && inTime > zoneOut) {
            // Outside zone
            continue;
        }
        if (inTime < zoneIn) {
            inTime = zoneIn;
        }
        if (zoneOut > GenTime() && outTime > zoneOut) {
            outTime = zoneOut;
        }
        inTime -= zoneIn;
        outTime -= zoneIn;

        QJsonObject currentSubtitle;
        currentSubtitle.insert(QLatin1String("layer"), QJsonValue(layer));
        currentSubtitle.insert(QLatin1String("startPos"), QJsonValue(inTime.seconds()));
        currentSubtitle.insert(QLatin1String("dialogue"), QJsonValue(subtitle.second.toString(layer, inTime)));
        list.push_back(currentSubtitle);
        // qDebug()<<subtitle.first.seconds();
    }
    saveSubtitleData(list, outFile);
}

const QJsonArray SubtitleModel::toJson()
{
    // qDebug()<< "to JSON";
    QJsonArray list;
    for (const auto &subtitle : m_subtitleList) {
        QJsonObject currentSubtitle;
        currentSubtitle.insert(QLatin1String("layer"), QJsonValue(subtitle.first.first));
        currentSubtitle.insert(QLatin1String("startPos"), QJsonValue(subtitle.first.second.seconds()));
        currentSubtitle.insert(QLatin1String("dialogue"), QJsonValue(subtitle.second.toString(subtitle.first.first, subtitle.first.second)));
        list.push_back(currentSubtitle);
        // qDebug()<<subtitle.first.seconds();
    }
    return list;
}

void SubtitleModel::copySubtitle(const QString &path, int ix, bool checkOverwrite, bool updateFilter)
{
    QFile srcFile(pCore->currentDoc()->subTitlePath(m_timeline->uuid(), ix, false));
    if (srcFile.exists()) {
        QFile prev(path);
        if (prev.exists()) {
            if (checkOverwrite || !path.endsWith(QStringLiteral(".ass"))) {
                if (KMessageBox::questionTwoActions(QApplication::activeWindow(), i18n("File %1 already exists.\nDo you want to overwrite it?", path), {},
                                                    KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction) {
                    return;
                }
            }
            prev.remove();
        }
        srcFile.copy(path);
        if (updateFilter) {
            m_subtitleFilter->set("av.filename", path.toUtf8().constData());
        }
    } else {
        qDebug() << "/// SUB FILE " << srcFile.fileName() << " NOT FOUND!!!";
    }
}

void SubtitleModel::restoreTmpFile(int ix)
{

    QString outFile = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), ix, false);
    m_subtitleFilter->set("av.filename", outFile.toUtf8().constData());
}

void SubtitleModel::jsontoSubtitle(const QJsonArray &data)
{
    int ix = pCore->currentDoc()->getSequenceProperty(m_timeline->uuid(), QStringLiteral("kdenlive:activeSubtitleIndex"), QStringLiteral("0")).toInt();
    QString outFile = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), ix, false);
    QString masterFile = m_subtitleFilter->get("av.filename");
    if (masterFile.isEmpty()) {
        m_subtitleFilter->set("av.filename", outFile.toUtf8().constData());
    }
    int line = saveSubtitleData(data, outFile);
    qDebug() << "Saving subtitle filter: " << outFile;
    if (line > 0) {
        m_subtitleFilter->set("av.filename", outFile.toUtf8().constData());
        m_timeline->tractor()->attach(*m_subtitleFilter.get());
    } else {
        m_timeline->tractor()->detach(*m_subtitleFilter.get());
    }
}

int SubtitleModel::saveSubtitleData(const QJsonArray &list, const QString &outFile)
{
    bool assFormat = outFile.endsWith(".ass");
    if (!assFormat) {
        qDebug() << "srt/vtt/sbv file import"; // if imported file isn't .ass, it is .srt format
    }
    QFile outF(outFile);

    // qDebug()<< "Import from JSON";
    QWriteLocker locker(&m_lock);
    if (list.isEmpty()) {
        qDebug() << "Error : Json file should be an array";
        return 0;
    }
    int line = 0;
    if (outF.open(QIODevice::WriteOnly)) {
        QTextStream out(&outF);
        if (assFormat) {
            out << QStringLiteral("[Script Info]\n; Script generated by Kdenlive %1\n").arg(KDENLIVE_VERSION);
            for (const auto &entry : std::as_const(m_scriptInfo)) {
                out << entry.first + ": " + entry.second + '\n';
            }
            out << '\n';

            out << "[Kdenlive Extradata]\n";
            out << "MaxLayer: " + QString::number(getMaxLayer()) + '\n';
            QString defaultStyles;
            for (const auto &style : std::as_const(m_defaultStyles)) {
                defaultStyles += style + ',';
            }
            defaultStyles.chop(1);
            out << "DefaultStyles: " + defaultStyles + '\n';

            out << '\n';

            out << QStringLiteral("[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, "
                                  "Italic, Underline, StrikeOut, "
                                  "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n");
            for (const auto &entry : std::as_const(m_subtitleStyles)) {
                out << entry.second.toString(entry.first) << '\n';
            }
            out << '\n';

            if (!fontSection.isEmpty()) out << fontSection << '\n';

            out << QStringLiteral("[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n");
        }
        for (const auto &entry : std::as_const(list)) {
            if (!entry.isObject()) {
                qDebug() << "Warning : Skipping invalid subtitle data";
                continue;
            }
            auto entryObj = entry.toObject();
            if (!entryObj.contains(QLatin1String("startPos"))) {
                qDebug() << "Warning : Skipping invalid subtitle data (does not contain position)";
                continue;
            }
            GenTime startPos(entryObj[QLatin1String("startPos")].toDouble());
            QString dialogue = entryObj[QLatin1String("dialogue")].toString();
            // Ensure line breaks are correctly handled
            SubtitleEvent sub(dialogue, pCore->getCurrentFps());
            GenTime endPos = sub.endTime();
            line++;
            if (assFormat) {
                dialogue.replace(QLatin1Char('\n'), QStringLiteral("\\N"));
                // Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text
                // TODO : (after adjusting the structure of m_subtitleList)
                out << dialogue << '\n';
            } else {
                QString startTimeStringSRT = SubtitleEvent::timeToString(startPos, 1);
                QString endTimeStringSRT = SubtitleEvent::timeToString(endPos, 1);
                out << line << "\n" << startTimeStringSRT << " --> " << endTimeStringSRT << "\n" << sub.text() << "\n" << '\n';
                qDebug() << sub.text() << '\n';
            }

            // qDebug() << "ADDING SUBTITLE to FILE AT START POS: " << startPos <<" END POS: "<<endPos;//<< ", FPS: " << pCore->getCurrentFps();
        }
        outF.close();
    }
    return line;
}

void SubtitleModel::updateSub(int id, const QVector<int> &roles)
{
    int row = getSubtitleIndex(id);
    Q_EMIT dataChanged(index(row), index(row), roles);
}

void SubtitleModel::updateSub(int startRow, int endRow, const QVector<int> &roles)
{
    Q_EMIT dataChanged(index(startRow), index(endRow), roles);
}

int SubtitleModel::getRowForId(int id) const
{
    return getSubtitleIndex(id);
}

int SubtitleModel::getSubtitlePlaytime(int id) const
{
    std::pair<int, GenTime> startPos = m_allSubtitles.at(id);
    return m_subtitleList.at(startPos).endTime().frames(pCore->getCurrentFps()) - startPos.second.frames(pCore->getCurrentFps());
}

GenTime SubtitleModel::getSubtitlePosition(int sid) const
{
    Q_ASSERT(m_allSubtitles.find(sid) != m_allSubtitles.end());
    return m_allSubtitles.at(sid).second;
}

int SubtitleModel::getSubtitleFakePosition(int sid) const
{
    int ix = getSubtitleIndex(sid);
    return getSubtitleFakePosFromIndex(ix);
}

int SubtitleModel::getSubtitleEnd(int id) const
{
    std::pair<int, GenTime> startPos = m_allSubtitles.at(id);
    return m_subtitleList.at(startPos).endTime().frames(pCore->getCurrentFps());
}

QPair<int, int> SubtitleModel::getInOut(int sid) const
{
    std::pair<int, GenTime> startPos = m_allSubtitles.at(sid);
    return {startPos.second.frames(pCore->getCurrentFps()), m_subtitleList.at(startPos).endTime().frames(pCore->getCurrentFps())};
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
    return m_subtitleList.rbegin()->second.endTime().frames(pCore->getCurrentFps());
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

void SubtitleModel::loadProperties(const QMap<QString, QString> &subProperties)
{
    if (subProperties.isEmpty()) {
        if (m_subtitleFilter->property_exists("av.force_style")) {
            const QString style = m_subtitleFilter->get("av.force_style");
            Q_EMIT updateSubtitleStyle(style);
        } else {
            Q_EMIT updateSubtitleStyle(QString());
        }
        return;
    }
    QMap<QString, QString>::const_iterator i = subProperties.constBegin();
    while (i != subProperties.constEnd()) {
        if (!i.value().isEmpty()) {
            m_subtitleFilter->set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
        }
        ++i;
    }
    if (subProperties.contains(QLatin1String("av.force_style"))) {
        Q_EMIT updateSubtitleStyle(subProperties.value(QLatin1String("av.force_style")));
    } else {
        Q_EMIT updateSubtitleStyle(QString());
    }
    qDebug() << "::::: LOADED SUB PROPS " << subProperties;
}

void SubtitleModel::allSnaps(std::vector<int> &snaps)
{
    for (const auto &subtitle : m_subtitleList) {
        snaps.push_back(subtitle.first.second.frames(pCore->getCurrentFps()));
        snaps.push_back(subtitle.second.endTime().frames(pCore->getCurrentFps()));
    }
}

QDomElement SubtitleModel::toXml(int sid, QDomDocument &document)
{
    std::pair<int, GenTime> startPos = m_allSubtitles.at(sid);
    GenTime endPos = m_subtitleList.at(startPos).endTime();
    QDomElement container = document.createElement(QStringLiteral("subtitle"));
    container.setAttribute(QStringLiteral("layer"), startPos.first);
    container.setAttribute(QStringLiteral("in"), startPos.second.frames(pCore->getCurrentFps()));
    container.setAttribute(QStringLiteral("out"), endPos.frames(pCore->getCurrentFps()));
    container.setAttribute(QStringLiteral("event_text"), m_subtitleList.at(startPos).toString(startPos.first, startPos.second));
    return container;
}

bool SubtitleModel::isBlankAt(int layer, int pos) const
{
    GenTime matchPos(pos, pCore->getCurrentFps());
    for (const auto &subtitles : m_subtitleList) {
        // if layer is -1, we check all layers
        if (layer == -1 || subtitles.first.first == layer) {
            if (subtitles.first.second > matchPos) {
                continue;
            }
            if (subtitles.second.endTime() > matchPos) {
                return false;
            }
        }
    }
    return true;
}

int SubtitleModel::getBlankEnd(int layer, int pos) const
{
    GenTime matchPos(pos, pCore->getCurrentFps());
    bool found = false;
    GenTime min;
    for (const auto &subtitles : m_subtitleList) {
        // if layer is -1, we check all layers
        if ((layer == -1 || subtitles.first.first == layer) && subtitles.first.second > matchPos && (min == GenTime() || subtitles.first.second < min)) {
            min = subtitles.first.second;
            found = true;
        }
    }
    return found ? min.frames(pCore->getCurrentFps()) : 0;
}

int SubtitleModel::getBlankSizeAtPos(int layer, int frame) const
{
    int bkStart = getBlankStart(layer, frame);
    int bkEnd = getBlankEnd(layer, frame);
    return bkEnd - bkStart;
}

int SubtitleModel::getBlankStart(int layer, int pos) const
{
    GenTime matchPos(pos, pCore->getCurrentFps());
    bool found = false;
    GenTime min;
    for (const auto &subtitles : m_subtitleList) {
        // if layer is -1, we check all layers
        if ((layer == -1 || subtitles.first.first == layer) && subtitles.second.endTime() <= matchPos &&
            (min == GenTime() || subtitles.second.endTime() > min)) {
            min = subtitles.second.endTime();
            found = true;
        }
    }
    return found ? min.frames(pCore->getCurrentFps()) : 0;
}

int SubtitleModel::getNextBlankStart(int layer, int pos) const
{
    while (!isBlankAt(layer, pos)) {
        std::unordered_set<int> matches = getItemsInRange(layer, pos, pos);
        if (matches.size() == 0) {
            if (isBlankAt(layer, pos)) {
                break;
            } else {
                // We are at the end of the track, abort
                return -1;
            }
        } else {
            for (int id : matches) {
                pos = qMax(pos, getSubtitleEnd(id));
            }
        }
    }
    return getBlankStart(layer, pos);
}

void SubtitleModel::editSubtitle(int id, const QString &newText, const QString &oldText)
{
    qDebug() << "Editing existing subtitle :" << id;
    if (oldText == newText) {
        return;
    }
    Fun local_redo = [this, id, newText]() {
        editSubtitle(id, newText);
        QPair<int, int> range = getInOut(id);
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    Fun local_undo = [this, id, oldText]() {
        editSubtitle(id, oldText);
        QPair<int, int> range = getInOut(id);
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
}

void SubtitleModel::resizeSubtitle(int layer, int startFrame, int endFrame, int oldEndFrame, bool refreshModel)
{
    qDebug() << "Editing existing subtitle in controller at:" << startFrame;
    int max = qMax(endFrame, oldEndFrame);
    Fun local_redo = [this, layer, startFrame, endFrame, max, refreshModel]() {
        editEndPos(layer, GenTime(startFrame, pCore->getCurrentFps()), GenTime(endFrame, pCore->getCurrentFps()), refreshModel);
        pCore->refreshProjectRange({startFrame, max});
        return true;
    };
    Fun local_undo = [this, layer, startFrame, oldEndFrame, max, refreshModel]() {
        editEndPos(layer, GenTime(startFrame, pCore->getCurrentFps()), GenTime(oldEndFrame, pCore->getCurrentFps()), refreshModel);
        pCore->refreshProjectRange({startFrame, max});
        return true;
    };
    local_redo();
    if (refreshModel) {
        pCore->pushUndo(local_undo, local_redo, i18n("Resize subtitle"));
    }
}

void SubtitleModel::addSubtitle(int startframe, int layer, QString text)
{
    if (startframe == -1) {
        startframe = pCore->getMonitorPosition();
    }
    int endframe = startframe + pCore->getDurationFromString(KdenliveSettings::subtitle_duration());
    int id = TimelineModel::getNextId();
    if (text.isEmpty()) {
        text = i18n("Add Text");
    }
    Fun local_undo = [this, id, startframe, endframe]() {
        removeSubtitle(id);
        QPair<int, int> range = {startframe, endframe};
        pCore->invalidateRange(range);
        pCore->refreshProjectRange(range);
        return true;
    };
    Fun local_redo = [this, layer, id, startframe, endframe, text]() {
        if (addSubtitle(id, {layer, GenTime(startframe, pCore->getCurrentFps())},
                        SubtitleEvent(true, GenTime(endframe, pCore->getCurrentFps()), getLayerDefaultStyle(layer), "", 0, 0, 0, "", text))) {
            QPair<int, int> range = {startframe, endframe};
            pCore->invalidateRange(range);
            pCore->refreshProjectRange(range);

            m_timeline->requestAddToSelection(id, true);
            int index = positionForIndex(id);
            if (index > -1) {
                Q_EMIT m_timeline->highlightSub(index);
            }
        }
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Add subtitle"));
}

void SubtitleModel::doCutSubtitle(int id, int cursorPos)
{
    Q_ASSERT(m_timeline->isSubTitle(id));
    // Cut subtitle at edit position
    int timelinePos = pCore->getMonitorPosition();
    GenTime position(timelinePos, pCore->getCurrentFps());
    GenTime start = getStartPosForId(id);
    int layer = getLayerForId(id);
    SubtitleEvent event = getSubtitle(layer, start);
    if (position > start && position < event.endTime()) {
        QString originalText = event.text();
        QString firstText = originalText;
        QString secondText = originalText.right(originalText.length() - cursorPos);
        firstText.truncate(cursorPos);
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        int newId = cutSubtitle(layer, timelinePos, undo, redo);
        if (newId > -1) {
            Fun local_redo = [this, id, newId, firstText, secondText]() {
                editSubtitle(id, firstText);
                editSubtitle(newId, secondText);
                return true;
            };
            Fun local_undo = [this, id, originalText]() {
                editSubtitle(id, originalText);
                return true;
            };
            local_redo();
            UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
            pCore->pushUndo(undo, redo, i18n("Cut clip"));
        }
    }
}

void SubtitleModel::deleteSubtitle(int layer, int startframe, int endframe, const QString &text)
{
    int id = getIdForStartPos(layer, GenTime(startframe, pCore->getCurrentFps()));
    Fun local_redo = [this, id, startframe, endframe]() {
        removeSubtitle(id);
        pCore->refreshProjectRange({startframe, endframe});
        return true;
    };
    Fun local_undo = [this, layer, id, startframe, endframe, text]() {
        addSubtitle(id, {layer, GenTime(startframe, pCore->getCurrentFps())},
                    SubtitleEvent(true, GenTime(endframe, pCore->getCurrentFps()), "Default", "", 0, 0, 0, "", text));
        pCore->refreshProjectRange({startframe, endframe});
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Delete subtitle"));
}

QMap<std::pair<int, QString>, QString> SubtitleModel::getSubtitlesList() const
{
    return m_subtitlesList;
}

void SubtitleModel::setSubtitlesList(QMap<std::pair<int, QString>, QString> list)
{
    m_subtitlesList = list;
}

void SubtitleModel::updateModelName(int ix, const QString &name)
{
    QMapIterator<std::pair<int, QString>, QString> i(m_subtitlesList);
    std::pair<int, QString> oldSub = {-1, QString()};
    QString path;
    while (i.hasNext()) {
        i.next();
        if (i.key().first == ix) {
            // match
            oldSub = i.key();
            path = i.value();
            break;
        }
    }
    if (oldSub.first > -1) {
        int ix = oldSub.first;
        m_subtitlesList.remove(oldSub);
        m_subtitlesList.insert({ix, name}, path);
    } else {
        qDebug() << "COULD NOT FIND SUBTITLE TO EDIT, CNT:" << m_subtitlesList.size();
    }
    Q_EMIT m_timeline->subtitlesListChanged();
}

int SubtitleModel::getMaxLayer() const
{
    return m_maxLayer;
}

void SubtitleModel::requestDeleteLayer(int layer)
{
    std::map<std::pair<int, GenTime>, SubtitleEvent> oldSubtitles;
    for (auto it = m_subtitleList.begin(); it != m_subtitleList.end(); ++it) {
        if (it->first.first == layer) {
            oldSubtitles[it->first] = it->second;
        }
    }

    std::map<std::pair<int, GenTime>, int> oldIds;
    for (const auto &sub : oldSubtitles) {
        oldIds[sub.first] = getIdForStartPos(sub.first.first, sub.first.second);
    }

    Fun redo = [this, layer]() {
        deleteLayer(layer);
        return true;
    };

    Fun undo = [this, layer, oldSubtitles, oldIds]() {
        createLayer(layer);
        // Re-add all subtitles
        for (const auto &sub : oldSubtitles) {
            addSubtitle(oldIds.at(sub.first), sub.first, sub.second);
        }
        Q_EMIT modelChanged();
        return true;
    };

    redo();
    pCore->pushUndo(undo, redo, i18n("Delete subtitle layer"));
}
void SubtitleModel::requestCreateLayer(int copiedLayer, int position)
{
    if (position < 0) {
        position = getMaxLayer() + 1;
    }

    Fun undo = [this, position]() {
        deleteLayer(position);
        return true;
    };

    Fun redo = [this, position]() {
        createLayer(position);
        return true;
    };

    redo();
    if (copiedLayer >= 0) {
        auto subs = m_subtitleList;
        for (const auto &sub : subs) {
            if (sub.first.first == copiedLayer) {
                addSubtitle({position, sub.first.second}, sub.second, undo, redo);
            }
        }
    }
    pCore->pushUndo(undo, redo, i18n("Create subtitle layer"));
}

void SubtitleModel::deleteLayer(int layer)
{
    if (layer < 0) {
        layer = getMaxLayer();
    }
    // Delete all subtitles on this layer, and make the subtitles behind it move up
    auto subs = m_allSubtitles;
    for (const auto &sub : subs) {
        if (sub.second.first == layer) {
            removeSubtitle(sub.first);
        } else if (sub.second.first > layer) {
            moveSubtitle(sub.first, sub.second.first - 1, sub.second.second, false, true);
        }
    }

    setMaxLayer(getMaxLayer() - 1);
    Q_EMIT modelChanged();
    return;
}

void SubtitleModel::createLayer(int position)
{
    setMaxLayer(getMaxLayer() + 1);
    // Move all following subtitles the new layer
    // Prevent subtitles from failing to move when there is a subtitle at the same position on the next layer.
    for (int layer = getMaxLayer() - 1; layer >= position; layer--) {
        auto subs = m_allSubtitles;
        for (const auto &sub : subs) {
            if (sub.second.first == layer) {
                moveSubtitle(sub.first, sub.second.first + 1, sub.second.second, false, true);
            }
        }
    }
    Q_EMIT modelChanged();
    return;
}

void SubtitleModel::reload()
{
    int currentIx = pCore->currentDoc()->getSequenceProperty(m_timeline->uuid(), QStringLiteral("kdenlive:activeSubtitleIndex"), QStringLiteral("0")).toInt();
    activateSubtitle(currentIx);
}

const std::map<QString, SubtitleStyle> &SubtitleModel::getAllSubtitleStyles(bool global) const
{
    if (global) {
        return m_globalSubtitleStyles;
    } else {
        return m_subtitleStyles;
    }
}

const SubtitleStyle &SubtitleModel::getSubtitleStyle(const QString &name, bool global) const
{
    if (global) {
        return m_globalSubtitleStyles.at(name);
    } else {
        return m_subtitleStyles.at(name);
    }
}

void SubtitleModel::setSubtitleStyle(const QString &name, const SubtitleStyle &style, bool global)
{
    if (global) {
        m_globalSubtitleStyles[name] = style;
    } else {
        m_subtitleStyles[name] = style;
        Q_EMIT modelChanged();
    }
}

void SubtitleModel::deleteSubtitleStyle(const QString &name, bool global)
{
    if (global) {
        m_globalSubtitleStyles.erase(name);
    } else {
        // find all subtitles using this style and set them to default
        for (auto &subtitle : m_subtitleList) {
            if (subtitle.second.styleName() == name) {
                subtitle.second.setStyleName("Default");
            }
        }
        m_subtitleStyles.erase(name);
        Q_EMIT modelChanged();
    }
}

int SubtitleModel::createNewSubtitle(const QString subtitleName, int id)
{
    // Create new subtitle file
    QList<std::pair<int, QString>> keys = m_subtitlesList.keys();
    QStringList existingNames;
    int maxIx = 0;
    for (auto &l : keys) {
        existingNames << l.second;
        if (l.first > maxIx) {
            maxIx = l.first;
        }
    }
    maxIx++;
    int ix = m_subtitlesList.size() + 1;
    QString newName = subtitleName;
    if (newName.isEmpty()) {
        newName = i18nc("@item:inlistbox subtitle track name", "Subtitle %1", ix);
        while (existingNames.contains(newName)) {
            ix++;
            newName = i18nc("@item:inlistbox subtitle track name", "Subtitle %1", ix);
        }
    }
    const QString newPath = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), maxIx, true);
    m_subtitlesList.insert({maxIx, newName}, newPath);
    if (id >= 0) {
        // Duplicate existing subtitle
        QString source = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), id, false);
        if (!QFile::exists(source)) {
            source = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), id, true);
        }
        QFile::copy(source, newPath);
    }
    Q_EMIT m_timeline->subtitlesListChanged();
    return m_subtitlesList.size();
}

bool SubtitleModel::deleteSubtitle(int ix)
{
    QMapIterator<std::pair<int, QString>, QString> i(m_subtitlesList);
    bool success = false;
    std::pair<int, QString> matchingItem = {-1, QString()};
    while (i.hasNext()) {
        i.next();
        if (i.key().first == ix) {
            // Found a match
            matchingItem = i.key();
            success = true;
            break;
        }
    }
    if (success && matchingItem.first > -1) {
        // Delete subtitle files
        const QString workPath = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), matchingItem.first, false);
        const QString finalPath = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), matchingItem.first, true);
        QFile::remove(workPath);
        QFile::remove(finalPath);
        // Remove entry from our subtitles list
        m_subtitlesList.remove(matchingItem);
    }
    Q_EMIT m_timeline->subtitlesListChanged();
    return success;
}

const QString SubtitleModel::subtitlesFilesToJson()
{
    QJsonArray list;
    QMapIterator<std::pair<int, QString>, QString> i(m_subtitlesList);
    std::pair<int, QString> oldSub = {-1, QString()};
    while (i.hasNext()) {
        i.next();
        QJsonObject currentSubtitle;
        currentSubtitle.insert(QLatin1String("name"), QJsonValue(i.key().second));
        currentSubtitle.insert(QLatin1String("id"), QJsonValue(i.key().first));
        currentSubtitle.insert(QLatin1String("file"), QJsonValue(i.value()));
        list.push_back(currentSubtitle);
    }
    QJsonDocument json(list);
    return QString::fromUtf8(json.toJson());
}

const QString SubtitleModel::globalStylesToJson()
{
    QJsonArray list;
    for (const auto &style : m_globalSubtitleStyles) {
        QJsonObject currentStyle;
        currentStyle.insert(QLatin1String("style"), QJsonValue(style.second.toString(style.first)));
        list.push_back(currentStyle);
    }
    QJsonDocument json(list);
    return QString::fromUtf8(json.toJson());
}

void SubtitleModel::activateSubtitle(int ix)
{
    // int currentIx = pCore->currentDoc()->getSequenceProperty(m_timeline->uuid(), QStringLiteral("kdenlive:activeSubtitleIndex"),
    // QStringLiteral("0")).toInt(); if (currentIx == ix) {
    //     return;
    // }
    const QString workPath = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), ix, false);
    const QString finalPath = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), ix, true);
    if (!QFile::exists(workPath) && QFile::exists(finalPath)) {
        QFile::copy(finalPath, workPath);
    }
    QFile file(workPath);
    if (!file.exists()) {
        // Create work file
        file.open(QIODevice::WriteOnly);
        file.close();
    }
    beginRemoveRows(QModelIndex(), 0, m_allSubtitles.size());
    m_subtitleList.clear();
    m_subtitleStyles.clear();
    m_scriptInfo.clear();
    m_maxLayer = 0;
    m_defaultStyles.clear();
    while (!m_allSubtitles.empty()) {
        deregisterSubtitle(m_allSubtitles.begin()->first);
    }
    endRemoveRows();
    pCore->currentDoc()->setSequenceProperty(m_timeline->uuid(), QStringLiteral("kdenlive:activeSubtitleIndex"), ix);
    parseSubtitle(workPath);
}

void SubtitleModel::setMaxLayer(int layer)
{
    m_maxLayer = layer;
    for (int i = m_defaultStyles.size(); i < m_maxLayer + 1; i++) {
        m_defaultStyles << "Default";
    }
    Q_EMIT m_timeline->maxSubLayerChanged();
}

void SubtitleModel::registerSubtitle(int id, std::pair<int, GenTime> startpos, bool temporary)
{
    Q_ASSERT(m_allSubtitles.count(id) == 0);
    m_allSubtitles.emplace(id, startpos);
    if (!temporary) {
        m_timeline->m_groups->createGroupItem(id);
    }
}

void SubtitleModel::deregisterSubtitle(int id, bool temporary)
{
    Q_ASSERT(m_allSubtitles.count(id) > 0);
    if (!temporary && isSelected(id)) {
        m_timeline->requestClearSelection(true);
    }
    m_allSubtitles.erase(id);
    if (!temporary) {
        m_timeline->m_groups->destructGroupItem(id);
    }
}

int SubtitleModel::positionForIndex(int id) const
{
    return int(std::distance(m_allSubtitles.begin(), m_allSubtitles.find(id)));
}

bool SubtitleModel::hasSubtitle(int id) const
{
    return m_allSubtitles.find(id) != m_allSubtitles.end();
}

int SubtitleModel::count() const
{
    return m_allSubtitles.size();
}

GenTime SubtitleModel::getPosition(int id) const
{
    return m_allSubtitles.at(id).second;
}

int SubtitleModel::getSubtitleIndex(int subId) const
{
    if (m_allSubtitles.count(subId) == 0) {
        return -1;
    }
    auto it = m_allSubtitles.find(subId);
    return int(std::distance(m_allSubtitles.begin(), it));
}

std::pair<int, std::pair<int, GenTime>> SubtitleModel::getSubtitleIdFromIndex(int index) const
{
    if (index >= static_cast<int>(m_allSubtitles.size())) {
        return {-1, {-1, GenTime()}};
    }
    auto it = m_allSubtitles.begin();
    std::advance(it, index);
    return {it->first, it->second};
}

int SubtitleModel::getSubtitleIdByPosition(int layer, int pos)
{
    GenTime startTime(pos, pCore->getCurrentFps());
    auto findResult = std::find_if(std::begin(m_allSubtitles), std::end(m_allSubtitles), [&](const std::pair<int, std::pair<int, GenTime>> &pair) {
        return pair.second.second == startTime && pair.second.first == layer;
    });
    if (findResult != std::end(m_allSubtitles)) {
        return findResult->first;
    }
    return -1;
}

int SubtitleModel::getSubtitleIdAtPosition(int layer, int pos)
{
    std::unordered_set<int> sids = getItemsInRange(layer, pos, pos);
    if (!sids.empty()) {
        return *sids.begin();
    }
    return -1;
}

void SubtitleModel::cleanupSubtitleFakePos()
{
    std::vector<int> itemKeys;
    for (const auto &sub : m_subtitlesFakePos) {
        itemKeys.push_back(sub.first);
    }
    m_subtitlesFakePos.clear();
    for (auto &i : itemKeys) {
        Q_EMIT dataChanged(index(i), index(i), {SubtitleModel::FakeStartFrameRole});
    }
}

int SubtitleModel::getSubtitleFakePosFromIndex(int index) const
{
    auto search = m_subtitlesFakePos.find(index);
    if (search != m_subtitlesFakePos.end()) {
        return search->second;
    }
    return -1;
}

void SubtitleModel::setSubtitleFakePosFromIndex(int index, int pos)
{
    m_subtitlesFakePos[index] = pos;
}

std::unordered_set<int> SubtitleModel::getAllSubIds() const
{
    std::unordered_set<int> ids;
    for (std::map<int, std::pair<int, GenTime>>::const_iterator it = m_allSubtitles.cbegin(); it != m_allSubtitles.cend(); ++it) {
        ids.emplace(it->first);
    }
    return ids;
}

bool SubtitleModel::getIsDialogue(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return false;
    return m_subtitleList.at(m_allSubtitles.at(id)).isDialogue();
}

void SubtitleModel::setIsDialogue(int id, bool isDialogue, bool refreshModel)
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return;
    bool oldIsDialogue = getIsDialogue(id);
    if (oldIsDialogue == isDialogue) {
        return;
    }
    Fun local_redo = [this, id, isDialogue, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setIsDialogue(isDialogue);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {IsDialogueRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    Fun local_undo = [this, id, oldIsDialogue, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setIsDialogue(oldIsDialogue);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {IsDialogueRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
}

QString SubtitleModel::getStyleName(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return QString();
    return m_subtitleList.at(m_allSubtitles.at(id)).styleName();
}

void SubtitleModel::setStyleName(const int id, const QString &style, bool refreshModel)
{
    auto sub = m_subtitleList.find(m_allSubtitles.at(id));
    if (sub == m_subtitleList.end() || style == sub->second.styleName()) {
        return;
    }
    QString oldStyle = sub->second.styleName();
    if (oldStyle == style) {
        return;
    }
    Fun redo = [sub, this, style, refreshModel, id]() {
        sub->second.setStyleName(style);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {StyleNameRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    Fun undo = [sub, this, oldStyle, refreshModel, id]() {
        sub->second.setStyleName(oldStyle);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {StyleNameRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, i18n("Assign a new style to a subtitle."));
}

QString SubtitleModel::getName(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return QString();
    return m_subtitleList.at(m_allSubtitles.at(id)).name();
}

void SubtitleModel::setName(int id, const QString &name, bool refreshModel)
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return;
    QString oldName = getName(id);
    if (oldName == name) {
        return;
    }
    Fun local_redo = [this, id, name, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setName(name);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {NameRole});
        if (refreshModel) Q_EMIT modelChanged();
        return true;
    };
    Fun local_undo = [this, id, oldName, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setName(oldName);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {NameRole});
        if (refreshModel) Q_EMIT modelChanged();
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
}

int SubtitleModel::getMarginL(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return 0;
    return m_subtitleList.at(m_allSubtitles.at(id)).marginL();
}

void SubtitleModel::setMarginL(int id, int marginL, bool refreshModel)
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return;
    int oldMarginL = getMarginL(id);
    if (oldMarginL == marginL) {
        return;
    }
    Fun local_redo = [this, id, marginL, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setMarginL(marginL);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {MarginLRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    Fun local_undo = [this, id, oldMarginL, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setMarginL(oldMarginL);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {MarginLRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
}

int SubtitleModel::getMarginR(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return 0;
    return m_subtitleList.at(m_allSubtitles.at(id)).marginR();
}

void SubtitleModel::setMarginR(int id, int marginR, bool refreshModel)
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return;
    int oldMarginR = getMarginR(id);
    if (oldMarginR == marginR) {
        return;
    }
    Fun local_redo = [this, id, marginR, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setMarginR(marginR);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {MarginRRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    Fun local_undo = [this, id, oldMarginR, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setMarginR(oldMarginR);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {MarginRRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
}

int SubtitleModel::getMarginV(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return 0;
    return m_subtitleList.at(m_allSubtitles.at(id)).marginV();
}

void SubtitleModel::setMarginV(int id, int marginV, bool refreshModel)
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return;
    int oldMarginV = getMarginV(id);
    if (oldMarginV == marginV) {
        return;
    }
    Fun local_redo = [this, id, marginV, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setMarginV(marginV);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {MarginVRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    Fun local_undo = [this, id, oldMarginV, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setMarginV(oldMarginV);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {MarginVRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
}

QString SubtitleModel::getEffects(int id) const
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return QString();
    return m_subtitleList.at(m_allSubtitles.at(id)).effect();
}

void SubtitleModel::setEffects(int id, const QString &effects, bool refreshModel)
{
    if (m_allSubtitles.find(id) == m_allSubtitles.end()) return;
    QString oldEffects = getEffects(id);
    if (oldEffects == effects) {
        return;
    }
    Fun local_redo = [this, id, effects, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setEffect(effects);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {EffectRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    Fun local_undo = [this, id, oldEffects, refreshModel]() {
        m_subtitleList.at(m_allSubtitles.at(id)).setEffect(oldEffects);
        int row = getSubtitleIndex(id);
        Q_EMIT dataChanged(index(row), index(row), {EffectRole});
        if (refreshModel) Q_EMIT modelChanged();
        pCore->refreshProjectMonitorOnce();
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
}

int SubtitleModel::activeSubLayer() const
{
    return m_activeSubLayer;
}

void SubtitleModel::setActiveSubLayer(int layer)
{
    m_activeSubLayer = layer;
    Q_EMIT activeSubLayerChanged();
}

QString SubtitleModel::previewSubtitlePath() const
{
    QString path = pCore->currentDoc()->subTitlePath(m_timeline->uuid(), 0, false);
    path = path.left(path.lastIndexOf(QStringLiteral(".ass")));
    path += "-preview.ass";
    return path;
}

const std::map<QString, QString> &SubtitleModel::scriptInfo() const
{
    return m_scriptInfo;
}

void SubtitleModel::setLayerDefaultStyle(int layer, const QString styleName)
{
    if (m_defaultStyles.size() > layer) {
        m_defaultStyles[layer] = styleName;
    }
}

const QString SubtitleModel::getLayerDefaultStyle(int layer) const
{
    if (m_defaultStyles.size() > layer) {
        return m_defaultStyles.at(layer);
    } else {
        return QStringLiteral("Default");
    }
}
