/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/model/assetparametermodel.hpp"
#include "definitions.h"
#include "undohelper.hpp"
#include "utils/gentime.h"

#include <QAbstractListModel>
#include <QReadWriteLock>
#include <QtGlobal>

#include <framework/mlt_version.h>

#include <map>
#include <memory>

class AssetParameterModel;
class DocUndoStack;
class EffectItemModel;

#define mltMinVersion QT_VERSION_CHECK(7, 20, 0)
#define currentVersion QT_VERSION_CHECK(LIBMLT_VERSION_MAJOR, LIBMLT_VERSION_MINOR, 0)

#if currentVersion > mltMinVersion
#define USE_MLT_NEW_KEYFRAMES
enum class KeyframeType {
    Linear = mlt_keyframe_linear,
    Discrete = mlt_keyframe_discrete,
    Curve = mlt_keyframe_smooth,
    CurveSmooth = mlt_keyframe_smooth_natural,
    BounceIn = mlt_keyframe_bounce_in,
    BounceOut = mlt_keyframe_bounce_out,
    CubicIn = mlt_keyframe_cubic_in,
    CubicOut = mlt_keyframe_cubic_out,
    ExponentialIn = mlt_keyframe_exponential_in,
    ExponentialOut = mlt_keyframe_exponential_out,
    CircularIn = mlt_keyframe_circular_in,
    CircularOut = mlt_keyframe_circular_out,
    ElasticIn = mlt_keyframe_elastic_in,
    ElasticOut = mlt_keyframe_elastic_out
};
#else
enum class KeyframeType { Linear = mlt_keyframe_linear, Discrete = mlt_keyframe_discrete, Curve = mlt_keyframe_smooth };
#endif

Q_DECLARE_METATYPE(KeyframeType)
using Keyframe = std::pair<GenTime, KeyframeType>;

/** @class KeyframeModel
    @brief This class is the model for a list of keyframes.
   A keyframe is defined by a time, a type and a value
   We store them in a sorted fashion using a std::map
 */
class KeyframeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /** @brief Construct a keyframe list bound to the given effect
       @param init_value is the value taken by the param at time 0.
       @param model is the asset this parameter belong to
       @param index is the index of this parameter in its model
     */
    explicit KeyframeModel(std::weak_ptr<AssetParameterModel> model, const QModelIndex &index, std::weak_ptr<DocUndoStack> undo_stack, int in = -1,
                           int out = -1, QObject *parent = nullptr);

    enum { TypeRole = Qt::UserRole + 1, PosRole, FrameRole, ValueRole, NormalizedValueRole, SelectedRole, ActiveRole, MoveOnlyRole };
    friend class KeyframeModelList;
    friend class KeyframeWidget;
    friend class KeyframeImport;
    friend class AssetMultiKeyframeCommand;

protected:
    /** @brief These methods should ONLY be called by keyframemodellist to ensure synchronisation
     *  with keyframes from other parameters */
    /** @brief Adds a keyframe at the given position. If there is already one then we update it.
       @param pos defines the position of the keyframe, relative to the clip
       @param type is the type of the keyframe.
     */
    bool addKeyframe(GenTime pos, KeyframeType type, QVariant value);
    bool addKeyframe(int frame, double normalizedValue);
    /** @brief Same function but accumulates undo/redo
       @param notify: if true, send a signal to model
     */
    bool addKeyframe(GenTime pos, KeyframeType type, QVariant value, bool notify, Fun &undo, Fun &redo);

    /** @brief Removes the keyframe at the given position. */
    bool removeKeyframe(int frame);
    bool moveKeyframe(int oldPos, int pos, QVariant newVal);
    /** @brief Duplicate a keyframe at the given position. */
    bool duplicateKeyframe(GenTime srcPos, GenTime dstPos, Fun &undo, Fun &redo);
    bool removeKeyframe(GenTime pos);
    /** @brief Delete all the keyframes of the model */
    bool removeAllKeyframes();
    bool removeAllKeyframes(Fun &undo, Fun &redo);
    bool removeNextKeyframes(GenTime pos, Fun &undo, Fun &redo);
    QList<GenTime> getKeyframePos() const;
    /** @brief Same function but accumulates undo/redo */
    bool removeKeyframe(GenTime pos, Fun &undo, Fun &redo, bool notify = true, bool updateSelection = true, bool allowedToFail = false);

public:
    /** @brief moves a keyframe
       @param oldPos is the old position of the keyframe
       @param pos defines the new position of the keyframe, relative to the clip
       @param logUndo if true, then an undo object is created
    */
    bool moveKeyframe(int oldPos, int pos, bool logUndo);
    bool offsetKeyframes(int oldPos, int pos, bool logUndo);
    bool moveKeyframe(GenTime oldPos, GenTime pos, QVariant newVal, bool logUndo);
    bool moveKeyframe(GenTime oldPos, GenTime pos, const QVariant &newVal, Fun &undo, Fun &redo, bool updateView = true, bool allowedToFail = false);

    /** @brief updates the value of a keyframe
       @param old is the position of the keyframe
       @param value is the new value of the param
    */
    Q_INVOKABLE bool updateKeyframe(int pos, double newVal);
    bool updateKeyframe(GenTime pos, QVariant value);
    bool updateKeyframeType(GenTime pos, int type, Fun &undo, Fun &redo);
    bool updateKeyframe(GenTime pos, const QVariant &value, Fun &undo, Fun &redo, bool update = true);
    /** @brief updates the value of a keyframe, without any management of undo/redo
       @param pos is the position of the keyframe
       @param value is the new value of the param
    */
    bool directUpdateKeyframe(GenTime pos, QVariant value, bool notify = true);

    /** @brief Returns a keyframe data at given pos
       ok is a return parameter, set to true if everything went good
     */
    Keyframe getKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns true if we only have 1 keyframe
     */
    bool singleKeyframe() const;
    /** @brief Returns the keyframe located after given position.
       If there is a keyframe at given position it is ignored.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getNextKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns the keyframe located before given position.
       If there is a keyframe at given position it is ignored.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getPrevKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns the closest keyframe from given position.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getClosestKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns true if a keyframe exists at given pos
       Notice that add/remove queries are done in real time (gentime), but this request is made in frame
     */
    Q_INVOKABLE bool hasKeyframe(int frame) const;
    Q_INVOKABLE bool hasKeyframe(const GenTime &pos) const;
    Q_INVOKABLE QString realValue(double normalizedValue) const;

    /** @brief Read the value from the model and update itself accordingly */
    void refresh(int in = -1, int out = -1);
    /** @brief Reset all values to their default */
    void reset();

    /** @brief Return the interpolated value at given pos */
    QVariant getInterpolatedValue(int pos) const;
    QVariant getInterpolatedValue(const GenTime &pos) const;
    QVariant updateInterpolated(const QVariant &interpValue, double val);
    /** @brief Return the real value from a normalized one */
    QVariant getNormalizedValue(double newVal) const;
    /** @brief Set or add a keyframe to selection */
    Q_INVOKABLE void setSelectedKeyframe(int ix, bool add);
    void setSelectedKeyframes(QVector<int> selection);

    Q_INVOKABLE int activeKeyframe() const;
    Q_INVOKABLE void setActiveKeyframe(int ix);
    int getIndexForPos(const GenTime pos) const;
    GenTime getPosAtIndex(int ix) const;

    // Mandatory overloads
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    static QList<QPoint> getRanges(const QString &animData, const std::shared_ptr<AssetParameterModel> &model);
    static std::shared_ptr<Mlt::Properties> getAnimation(std::shared_ptr<AssetParameterModel> model, const QString &animData, int duration = 0);
    static const QString getAnimationStringWithOffset(std::shared_ptr<AssetParameterModel> model, const QString &animData, int offset, int duration,
                                                      ParamType paramType, bool useOpacity = true);
    static const QString getIconByKeyframeType(KeyframeType type);
    static void initKeyframeTypes();
    static const QMap<KeyframeType, QString> getKeyframeTypes();

protected:
    /** @brief Helper function that generate a lambda to change type / value of given keyframe */
    Fun updateKeyframe_lambda(GenTime pos, KeyframeType type, const QVariant &value, bool notify);

    /** @brief Helper function that generate a lambda to add given keyframe */
    Fun addKeyframe_lambda(GenTime pos, KeyframeType type, const QVariant &value, bool notify);

    /** @brief Helper function that generate a lambda to remove given keyframe */
    Fun deleteKeyframe_lambda(GenTime pos, bool notify);

    /** @brief Connects the signals of this object */
    void setup();

    /** @brief Commit the modification to the model */
    void sendModification();

    /** @brief returns the keyframes as a Mlt Anim Property string.
        It is defined as pairs of frame and value, separated by ;
        Example : "0|=50; 50|=100; 100=200; 200~=60;"
        Spaces are ignored by Mlt.
        |= represents a discrete keyframe, = a linear one and ~= a Catmull-Rom spline
    */
    QString getAnimProperty() const;
    QString getRotoProperty() const;

    /** @brief this function clears all existing keyframes, and reloads its data from the string passed */
    void resetAnimProperty(const QString &prop);
    /** @brief this function does the opposite of getAnimProperty: given a MLT representation of an animation, build the corresponding model */
    void parseAnimProperty(const QString &prop, int in = -1, int out = -1);
    void parseRotoProperty(const QString &prop);

private:
    std::weak_ptr<AssetParameterModel> m_model;
    std::weak_ptr<DocUndoStack> m_undoStack;
    QPersistentModelIndex m_index;
    QString m_lastData;
    ParamType m_paramType;
    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;

    std::map<GenTime, std::pair<KeyframeType, QVariant>> m_keyframeList;
    bool moveOneKeyframe(GenTime oldPos, GenTime pos, QVariant newVal, Fun &undo, Fun &redo, bool updateView = true, bool allowedToFail = false);

Q_SIGNALS:
    void modelChanged();
    void requestModelUpdate(const QModelIndex &, const QModelIndex &, const QVector<int> &);

public:
    // this is to enable for range loops
    auto begin() -> decltype(m_keyframeList.begin()) { return m_keyframeList.begin(); }
    auto end() -> decltype(m_keyframeList.end()) { return m_keyframeList.end(); }
};
// Q_DECLARE_METATYPE(KeyframeModel *)
