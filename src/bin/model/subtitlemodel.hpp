/*
    SPDX-FileCopyrightText: 2020 Sashmita Raghav
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "bin/bin.h"
#include "definitions.h"
#include "undohelper.hpp"
#include "utils/gentime.h"

#include <QAbstractListModel>
#include <QReadWriteLock>

#include <array>
#include <map>
#include <memory>
#include <mlt++/Mlt.h>
#include <mlt++/MltProperties.h>
#include <unordered_set>

class DocUndoStack;
class SnapInterface;
class AssetParameterModel;
class TimelineItemModel;

/** @class SubtitleModel
    @brief This class is the model for a list of subtitles.
*/
class SubtitleModel : public QAbstractListModel
{
    Q_OBJECT

public:
    static const int RAZOR_MODE_DUPLICATE = 0;
    static const int RAZOR_MODE_AFTER_FIRST_LINE = 1;

    /** @brief Construct a subtitle list bound to the timeline */
    explicit SubtitleModel(std::shared_ptr<TimelineItemModel> timeline = nullptr,
                           const std::weak_ptr<SnapInterface> &snapModel = std::weak_ptr<SnapInterface>(), QObject *parent = nullptr);

    enum { SubtitleRole = Qt::UserRole + 1, StartPosRole, EndPosRole, StartFrameRole, EndFrameRole, IdRole, SelectedRole, GrabRole };
    /** @brief Function that parses through a subtitle file */
    bool addSubtitle(int id, GenTime start, GenTime end, const QString &str, bool temporary = false, bool updateFilter = true);
    bool addSubtitle(GenTime start, GenTime end, const QString &str, Fun &undo, Fun &redo, bool updateFilter = true);
    /** @brief Converts string of time to GenTime */
    GenTime stringtoTime(QString &str, const double factor = 1.);
    /** @brief Return model data item according to the role passed */
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override; // override the same function of QAbstractListModel
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
        @param id the model id of the subtitle
        @param newSubtitleText is (new) subtitle text
    */
    bool editSubtitle(int id, const QString &newSubtitleText);

    /** @brief Remove subtitle at start position (pos) */
    bool removeSubtitle(int id, bool temporary = false, bool updateFilter = true);

    /** @brief Remove all subtitles from subtitle model */
    void removeAllSubtitles();

    /** @brief Update some properties in the view */
    void updateSub(int id, const QVector<int> &roles);

    /** @brief Move an existing subtitle
        @param subId is the subtitle's ID
        @param newPos is new start position of subtitle
    */
    bool moveSubtitle(int subId, GenTime newPos, bool updateModel, bool updateView);
    void requestSubtitleMove(int clipId, GenTime position);

    /** @brief Guess the text encoding of the file at the provided path
     * @param file The path to the text file
     * @return The name of the text encoding, as guessed by KEncodingProber, or
     * "" if an error occurred
    */
    static QByteArray guessFileEncoding(const QString &file, bool *confidence);
    /** @brief Function that imports a subtitle file */
    void importSubtitle(const QString &filePath, int offset = 0, bool externalImport = false, float startFramerate = 30.00, float targetFramerate = 30.00, const QByteArray &encoding = "UTF-8");

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
    /** @brief Cut a subtitle, return the id of newly created subtitle */
    int cutSubtitle(int position, Fun &undo, Fun &redo);
    QString getText(int id) const;
    int getRowForId(int id) const;
    GenTime getStartPosForId(int id) const;
    int getPreviousSub(int id) const;
    int getNextSub(int id) const;
    /** @brief Copy subtitle file to a new path
     * @param path the new subtitle path
     * @param checkOverwrite if true, warn before overwriting an existing subtitle file
     * @param updateFilter if true, the subtitle filter will be updated to us the new name, useful when saving the project file */
    void copySubtitle(const QString &path, int ix, bool checkOverwrite, bool updateFilter = false);
    /** @brief Use the tmp work file for the subtitle filter after saving the project */
    void restoreTmpFile(int ix);
    int trackDuration() const;
    void switchDisabled();
    bool isDisabled() const;
    void switchLocked();
    bool isLocked() const;
    /** @brief Load some subtitle filter properties from file */
    void loadProperties(const QMap<QString, QString> &subProperties);
    /** @brief Add all subtitle items to snaps */
    void allSnaps(std::vector<int> &snaps);
    /** @brief Returns an xml representation of the subtitle with id \@sid */
    QDomElement toXml(int sid, QDomDocument &document);
    /** @brief Returns the position of the first blank frame before a position */
    int getBlankStart(int pos) const;
    /** @brief Returns the position of the first subtitle after the blank at @position */
    int getBlankEnd(int pos) const;
    /** @brief Returns the duration of the blank at @position */
    int getBlankSizeAtPos(int frame) const;
    /** @brief If pos is blank, returns the position of the blank start. Otherwise returns the position of the next blank frame */
    int getNextBlankStart(int pos) const;
    /** @brief Returns true is track is empty at pos */
    bool isBlankAt(int pos) const;
    /** @brief Switch a subtitle's grab state */
    void switchGrab(int sid);
    /** @brief Returns true is an item is grabbed */
    bool isGrabbed(int id) const;
    /** @brief Ungrab all items */
    void clearGrab();
    /** @brief Release timeline model pointer */
    void unsetModel();
    /** @brief Get in/out of a subtitle item */
    QPair<int, int> getInOut(int sid) const;
    /** @brief Set subtitle style (font, color, etc) */
    void setStyle(const QString &style);
    const QString getStyle() const;
    void subtitleFileFromZone(int in, int out, const QString &outFile);
    /** @brief Edit the subtitle text*/
    Q_INVOKABLE void editSubtitle(int id, const QString &newText, const QString &oldText);
    /** @brief Edit the subtitle end */
    Q_INVOKABLE void resizeSubtitle(int startFrame, int endFrame, int oldEndFrame, bool refreshModel);
    /** @brief Add subtitle clip at cursor's position in timeline */
    Q_INVOKABLE void addSubtitle(int startframe = -1, QString text = QString());
    /** @brief Delete subtitle clip with frame as start position*/
    Q_INVOKABLE void deleteSubtitle(int frameframe, int endframe, const QString &text);
    /** @brief Cut a subtitle and split the text at \@param pos */
    void doCutSubtitle(int id, int cursorPos);
    /** @brief Return a list of the existing subtitles in this model */
    QMap<std::pair<int, QString>, QString> getSubtitlesList() const;
    /** @brief Load the current subtitles list in this model */
    void setSubtitlesList(QMap<std::pair<int, QString>, QString> list);
    /** @brief Update a subtitle name */
    void updateModelName(int ix, const QString &name);
    /** @brief Create a new subtitle track and return its subtitle index */
    int createNewSubtitle(const QString subtitleName = QString(), int id = -1);
    bool deleteSubtitle(int ix);
    void activateSubtitle(int ix);
    /** @brief Get JSON data for all subtitles in this model */
    const QString subtitlesFilesToJson();

public Q_SLOTS:
    /** @brief Function that parses through a subtitle file */
    void parseSubtitle(const QString &workPath);

    /** @brief Import model to a temporary subtitle file to which the Subtitle effect is applied*/
    void jsontoSubtitle(const QString &data);
    /** @brief Update a subtitle text*/
    bool setText(int id, const QString &text);

private:
    std::shared_ptr<TimelineItemModel> m_timeline;
    std::weak_ptr<DocUndoStack> m_undoStack;
    /** @brief A list of subtitles as: start time, text, end time */
    std::map<GenTime, std::pair<QString, GenTime>> m_subtitleList;
    /** @brief A list of all available subtitle files for this timeline
     *  in the form: ({id, name}, path) where id for a subtitle never changes
     */
    QMap<std::pair<int, QString>, QString> m_subtitlesList;
    QString scriptInfoSection, styleSection, eventSection;
    QString styleName;

    // To get subtitle file from effects parameter:
    // std::unique_ptr<Mlt::Properties> m_asset;
    // std::shared_ptr<AssetParameterModel> m_model;

    std::vector<std::weak_ptr<SnapInterface>> m_regSnaps;
    mutable QReadWriteLock m_lock;
    std::unique_ptr<Mlt::Filter> m_subtitleFilter;
    QVector<int> m_selected;
    QVector<int> m_grabbedIds;
    int saveSubtitleData(const QString &data, const QString &outFile);

Q_SIGNALS:
    void modelChanged();
    void updateSubtitleStyle(const QString);

protected:
    /** @brief Add time as snap in the registered snap model */
    void addSnapPoint(GenTime startpos);
    /** @brief Remove time as snap in the registered snap model */
    void removeSnapPoint(GenTime startpos);
    /** @brief Connect changes in model with signal */
    void setup();
};
Q_DECLARE_METATYPE(SubtitleModel *)
