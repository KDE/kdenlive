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

    enum {
        SubtitleRole = Qt::UserRole + 1,
        IdRole,
        StartPosRole,
        EndPosRole,
        StartFrameRole,
        FakeStartFrameRole,
        EndFrameRole,
        LayerRole,
        StyleNameRole,
        NameRole,
        MarginLRole,
        MarginRRole,
        MarginVRole,
        EffectRole,
        IsDialogueRole,
        SelectedRole,
        GrabRole
    };

    /** @brief Function that parses through a subtitle file */
    bool addSubtitle(int id, std::pair<int, GenTime> start, const SubtitleEvent &event, bool temporary = false, bool updateFilter = true);
    bool addSubtitle(std::pair<int, GenTime> start, const SubtitleEvent &event, Fun &undo, Fun &redo, bool updateFilter = true);
    /** @brief Return model data item according to the role passed */
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override; // override the same function of QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** @brief Returns all subtitles in the model as a list of layer, start position and SubtitleEvent */
    const QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> getAllSubtitles() const;

    /** @brief Get subtitle event at position */
    SubtitleEvent getSubtitle(int layer, GenTime startPos) const;
    /** @brief Get a subtitle's position */
    GenTime getSubtitlePosition(int sid) const;
    int getSubtitleFakePosition(int sid) const;
    /** @brief Returns all subtitle ids in a range */
    std::unordered_set<int> getItemsInRange(int layer, int startFrame, int endFrame) const;

    /** @brief Registers a snap model to the subtitle model */
    void registerSnap(const std::weak_ptr<SnapInterface> &snapModel);

    /** @brief Edit subtitle end timing
        @param startPos is start position of subtitles
        @param newEndPos is the new position of the end time
    */
    void editEndPos(int layer, GenTime startPos, GenTime newEndPos, bool refreshModel = true);
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
    void updateSub(int startRow, int endRow, const QVector<int> &roles);

    /** @brief Move an existing subtitle
        @param subId is the subtitle's ID
        @param newPos is new start position of subtitle
    */
    bool moveSubtitle(int subId, int newLayer, GenTime newPos, bool updateModel, bool updateView);
    void requestSubtitleMove(int clipId, int newLayer, GenTime position);

    /** @brief Guess the text encoding of the file at the provided path
     * @param file The path to the text file
     * @return The name of the text encoding, as guessed by KEncodingProber, or
     * "" if an error occurred
    */
    static QByteArray guessFileEncoding(const QString &file, bool *confidence);
    /** @brief Function that imports a subtitle file */
    void importSubtitle(const QString &filePath, int offset = 0, bool externalImport = false, float startFramerate = 30.00, float targetFramerate = 30.00, const QByteArray &encoding = "UTF-8");

    /** @brief Exports the subtitle model to json */
    const QJsonArray toJson();
    /** @brief Returns the path to sub file */
    const QString getUrl();
    /** @brief Get a subtitle Id from its start position*/
    int getIdForStartPos(int layer, GenTime startTime) const;
    /** @brief Get the duration of a subtitle by id*/
    int getSubtitlePlaytime(int id) const;
    int getSubtitleEnd(int id) const;
    /** @brief Mark the subtitle item as selected or not*/
    void setSelected(int id, bool select);
    bool isSelected(int id) const;
    /** @brief Cut a subtitle */
    bool cutSubtitle(int layer, int position);
    /** @brief Cut a subtitle, return the id of newly created subtitle */
    int cutSubtitle(int layer, int position, Fun &undo, Fun &redo);
    QString getText(int id) const;
    int getRowForId(int id) const;
    int getLayerForId(int id) const;
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
    int getBlankStart(int layer, int pos) const;
    /** @brief Returns the position of the first subtitle after the blank at @position */
    int getBlankEnd(int layer, int pos) const;
    /** @brief Returns the duration of the blank at @position */
    int getBlankSizeAtPos(int layer, int frame) const;
    /** @brief If pos is blank, returns the position of the blank start. Otherwise returns the position of the next blank frame */
    int getNextBlankStart(int layer, int pos) const;
    /** @brief Returns true is track is empty at pos */
    bool isBlankAt(int layer, int pos) const;
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
    void setForceStyle(const QString &style);
    const QString getForceStyle() const;
    void subtitleFileFromZone(int in, int out, const QString &outFile);
    /** @brief Edit the subtitle text*/
    Q_INVOKABLE void editSubtitle(int id, const QString &newText, const QString &oldText);
    /** @brief Edit the subtitle end */
    Q_INVOKABLE void resizeSubtitle(int layer, int startFrame, int endFrame, int oldEndFrame, bool refreshModel);
    /** @brief Add subtitle clip at cursor's position in timeline */
    Q_INVOKABLE void addSubtitle(int startframe = -1, int layer = 0, QString text = QString());
    /** @brief Delete subtitle clip with frame as start position*/
    Q_INVOKABLE void deleteSubtitle(int layer, int frameframe, int endframe, const QString &text);
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
    /** @brief Get JSON data for all global styles in this model*/
    const QString globalStylesToJson();
    bool hasSubtitle(int id) const;
    int count() const;
    GenTime getPosition(int id) const;
    int getSubtitleIndex(int subId) const;
    std::pair<int, std::pair<int, GenTime>> getSubtitleIdFromIndex(int index) const;
    int getSubtitleIdByPosition(int layer, int pos);
    int getSubtitleIdAtPosition(int layer, int pos);
    void cleanupSubtitleFakePos();
    /** @brief Get / Set a fake position for a subtitle (while moving) */
    int getSubtitleFakePosFromIndex(int index) const;
    void setSubtitleFakePosFromIndex(int index, int pos);
    std::unordered_set<int> getAllSubIds() const;
    /** @brief Set the max layer in the subtitle model */
    void setMaxLayer(int layer);
    /** @brief Get the max layer in the subtitle model */
    int getMaxLayer() const;
    void requestDeleteLayer(int layer);
    void requestCreateLayer(int copiedLayer = -1, int position = -1);
    /** @brief Delete a layer from the subtitle model */
    void deleteLayer(int layer);
    /** @brief Create a new layer in the subtitle model */
    void createLayer(int position = -1);
    /** @brief Reload the subtitle model */
    void reload();
    /** @brief Get all subtitle styles */
    const std::map<QString, SubtitleStyle> &getAllSubtitleStyles(bool global = false) const;
    /** @brief Get a subtitle style by name */
    const SubtitleStyle &getSubtitleStyle(const QString &name, bool global = false) const;
    /** @brief Set a subtitle style */
    void setSubtitleStyle(const QString &name, const SubtitleStyle &style, bool global = false);
    /** @brief Delete a subtitle style */
    void deleteSubtitleStyle(const QString &name, bool global = false);
    /** @brief Get true if a subtitle is dialogue */
    bool getIsDialogue(int id) const;
    /** @brief Set a subtitle as dialogue */
    void setIsDialogue(int id, bool isDialogue, bool refreshModel = true);
    /** @brief Get the end time of a subtitle */
    QString getStyleName(int id) const;
    /** @brief Set the style name of a subtitle */
    void setStyleName(int id, const QString &styleName, bool refreshModel = true);
    /** @brief Get the name of a subtitle */
    QString getName(int id) const;
    /** @brief Set the name of a subtitle */
    void setName(int id, const QString &name, bool refreshModel = true);
    /** @brief Get the left margin of a subtitle */
    int getMarginL(int id) const;
    /** @brief Set the left margin of a subtitle */
    void setMarginL(int id, int marginL, bool refreshModel = true);
    /** @brief Get the right margin of a subtitle */
    int getMarginR(int id) const;
    /** @brief Set the right margin of a subtitle */
    void setMarginR(int id, int marginR, bool refreshModel = true);
    /** @brief Get the vertical margin of a subtitle */
    int getMarginV(int id) const;
    /** @brief Set the vertical margin of a subtitle */
    void setMarginV(int id, int marginV, bool refreshModel = true);
    /** @brief Get the effect of a subtitle */
    QString getEffects(int id) const;
    /** @brief Set the effect of a subtitle */
    void setEffects(int id, const QString &effects, bool refreshModel = true);
    /** @brief Get the active subtitle layer */
    int activeSubLayer() const;
    /** @brief Set the active subtitle layer */
    void setActiveSubLayer(int layer);
    /** @brief Get the preview subtitle path */
    QString previewSubtitlePath() const;
    /** @brief Get the script info */
    const std::map<QString, QString> &scriptInfo() const;
    /** @brief Set default style for subtitle layer */
    void setLayerDefaultStyle(int layer, const QString styleName);
    /** @brief Get default styles for subtitle layers */
    const QString getLayerDefaultStyle(int layer) const;
    int saveSubtitleData(const QJsonArray &data, const QString &outFile);

public Q_SLOTS:
    /** @brief Function that parses through a subtitle file */
    void parseSubtitle(const QString &workPath);

    /** @brief Import model to a temporary subtitle file to which the Subtitle effect is applied*/
    void jsontoSubtitle(const QJsonArray &data);
    /** @brief Update a subtitle text*/
    bool setText(int id, const QString &text);

private:
    std::shared_ptr<TimelineItemModel> m_timeline;
    std::weak_ptr<DocUndoStack> m_undoStack;
    /** @brief A list of subtitles as: layer, start time, events */
    std::map<std::pair<int, GenTime>, SubtitleEvent> m_subtitleList;
    /** @brief A list of all available subtitle files for this timeline
     *  in the form: ({id, name}, path) where id for a subtitle never changes
     */
    QMap<std::pair<int, QString>, QString> m_subtitlesList;
    /** @brief A list of subtitles as: item id, layer, start time */
    std::map<int, std::pair<int, GenTime>> m_allSubtitles;
    /** @brief The max layer in the subtitle model */
    int m_maxLayer{0};
    /** @brief Default styles for subtitle layers */
    QStringList m_defaultStyles;
    /** @brief A list of subtitles as: item index, fake start time */
    std::map<int, int> m_subtitlesFakePos;
    /** @brief A list of all available styles for the subtitles as: style name, style */
    std::map<QString, SubtitleStyle> m_subtitleStyles;
    /** @brief A list of subtitle styles storing in the subtitle file, can be used for all subtitles */
    std::map<QString, SubtitleStyle> m_globalSubtitleStyles;
    /** @brief A list of properties in the script info section */
    std::map<QString, QString> m_scriptInfo;
    QString fontSection;

    // To get subtitle file from effects parameter:
    // std::unique_ptr<Mlt::Properties> m_asset;
    // std::shared_ptr<AssetParameterModel> m_model;

    std::vector<std::weak_ptr<SnapInterface>> m_regSnaps;
    mutable QReadWriteLock m_lock;
    std::unique_ptr<Mlt::Filter> m_subtitleFilter;
    QVector<int> m_selected;
    QVector<int> m_grabbedIds;
    int m_activeSubLayer{0};

Q_SIGNALS:
    void modelChanged();
    void updateSubtitleStyle(const QString);
    void activeSubLayerChanged();
    void showSubtitleManager(int page);

protected:
    /** @brief Add time as snap in the registered snap model */
    void addSnapPoint(GenTime startpos);
    /** @brief Remove time as snap in the registered snap model */
    void removeSnapPoint(GenTime startpos);
    /** @brief Connect changes in model with signal */
    void setup();
    void registerSubtitle(int id, std::pair<int, GenTime> startpos, bool temporary = false);
    void deregisterSubtitle(int id, bool temporary = false);
    /** @brief Returns the index for a subtitle's id (it's position in the list
     */
    int positionForIndex(int id) const;
};
Q_DECLARE_METATYPE(SubtitleModel *)
