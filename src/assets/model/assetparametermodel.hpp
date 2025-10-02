/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "core.h"
#include "definitions.h"
#include "klocalizedstring.h"
#include <QAbstractListModel>
#include <QDomElement>
#include <QJsonDocument>
#include <unordered_map>

#include <memory>
#include <mlt++/MltProperties.h>

class KeyframeModelList;

typedef QVector<QPair<QString, QVariant>> paramVector;

enum class ParamType {
    Double,
    List,               // Value can be chosen from a list of pre-defined ones
    ListWithDependency, // Value can be chosen from a list of pre-defined ones. Some values might not be available due to missing dependencies
    UrlList,            // File can be chosen from a list of pre-defined ones or a custom file can be used (like url)
    Bool,
    Switch,
    EffectButtons,
    MultiSwitch,
    AnimatedRect,     // Animated rects have X, Y, width, height, and opacity (in [0,1])
    AnimatedFakeRect, // Contains 4 parameters that make a rect, animated
    FakeRect,         // Contains 4 parameters that make a rect
    Geometry,
    KeyframeParam,
    Color,
    FixedColor, // Non animated color
    ColorWheel,
    Position,
    Curve,
    Bezier_spline,
    Roto_spline,
    Wipe,
    Url,
    Keywords,
    Fontfamily,
    Filterjob,
    Readonly,
    Hidden,
    Unknown
};
Q_DECLARE_METATYPE(ParamType)

struct AssetRectInfo
{
    QString destName;
    int position{0};
    double defaultValue;
    double minimum;
    double maximum;
    double factor{1};
    double from{0};
    bool fromBorder;
    explicit AssetRectInfo(const QString &name, int pos, const QString &value, const QString &min, const QString &max, const QString &fac = QString(),
                           bool border = false, const QString &fr = QString())
        : destName(name)
        , position(pos)
        , fromBorder(border)
    {
        if (value == QLatin1String("%width")) {
            defaultValue = pCore->getCurrentFrameSize().width();
        } else if (value == QLatin1String("%height")) {
            defaultValue = pCore->getCurrentFrameSize().height();
        } else {
            defaultValue = value.toDouble();
        }
        if (min == QLatin1String("%width")) {
            minimum = pCore->getCurrentFrameSize().width();
        } else if (min == QLatin1String("%height")) {
            minimum = pCore->getCurrentFrameSize().height();
        } else {
            minimum = min.toDouble();
        }
        if (max == QLatin1String("%width")) {
            maximum = pCore->getCurrentFrameSize().width();
        } else if (max == QLatin1String("%height")) {
            maximum = pCore->getCurrentFrameSize().height();
        } else {
            maximum = max.toDouble();
        }
        if (fac == QLatin1String("%width")) {
            factor = pCore->getCurrentFrameSize().width();
        } else if (fac == QLatin1String("%height")) {
            factor = pCore->getCurrentFrameSize().height();
        } else {
            if (fac.isEmpty()) {
                factor = 1.0;
            } else {
                factor = fac.toDouble();
            }
        }
        if (fr == QLatin1String("%width")) {
            from = pCore->getCurrentFrameSize().width();
        } else if (fr == QLatin1String("%height")) {
            from = pCore->getCurrentFrameSize().height();
        } else {
            from = fr.toDouble();
        }
    }
    explicit AssetRectInfo() {}
    double getValue(double val) const
    {
        if (from != 0) {
            val = from - val;
        }
        if (factor != 1.) {
            val /= factor;
        }
        return val;
    }
    double getValue(const QStringList &vals) const
    {
        double val = vals.at(position).toDouble();
        if (fromBorder) {
            if (position == 2) {
                // Width
                val += vals.at(0).toDouble();
            } else if (position == 3) {
                // Width
                val += vals.at(1).toDouble();
            }
        }

        if (from != 0) {
            val = from - val;
        }
        if (factor != 1.) {
            val /= factor;
        }
        return val;
    }
    double getValue(const mlt_rect rect) const
    {
        double val = 0.;
        switch (position) {
        case 0:
            val = rect.x;
            break;
        case 1:
            val = rect.y;
            break;
        case 2:
            val = rect.w;
            break;
        case 3:
            val = rect.h;
            break;
        default:
            break;
        }
        if (fromBorder) {
            if (position == 2) {
                // Width
                val += rect.x;
            } else if (position == 3) {
                // Width
                val += rect.y;
            }
        }

        if (from != 0) {
            val = from - val;
        }
        if (factor != 1.) {
            val /= factor;
        }
        return val;
    }
};

/** @class AssetParameterModel
    @brief This class is the model for a list of parameters.
   The behaviour of a transition or an effect is typically  controlled by several parameters. This class exposes this parameters as a list that can be rendered
   using the relevant widgets.
   Note that internally parameters are not sorted in any ways, because some effects like sox need a precise order
    */
class AssetParameterModel : public QAbstractListModel, public enable_shared_from_this_virtual<AssetParameterModel>
{
    Q_OBJECT

    friend class KeyframeModelList;
    friend class KeyframeModel;

public:
    /**
     *
     * @param asset
     * @param assetXml XML to parse, from project file
     * @param assetId
     * @param ownerId
     * @param originalDecimalPoint If a decimal point other than “.” was used, try to replace all occurrences by a “.”
     * so numbers are parsed correctly.
     * @param parent
     */
    explicit AssetParameterModel(std::unique_ptr<Mlt::Properties> asset, const QDomElement &assetXml, const QString &assetId, ObjectId ownerId,
                                 const QString &originalDecimalPoint = QString(), QObject *parent = nullptr);
    ~AssetParameterModel() override;
    enum DataRoles {
        NameRole = Qt::UserRole + 1,
        TypeRole,
        CommentRole,
        AlternateNameRole,
        MinRole,
        VisualMinRole,
        VisualMaxRole,
        MaxRole,
        DefaultRole,
        SuffixRole,
        DecimalsRole,
        CompactRole,
        OddRole,
        ValueRole,
        AlphaRole,
        ListValuesRole,
        ListNamesRole,
        ListDependenciesRole,
        NewStuffRole,
        ModeRole,
        FactorRole,
        FilterRole,
        FilterJobParamsRole,
        FilterProgressRole,
        FilterParamsRole,
        FilterConsumerParamsRole,
        ScaleRole,
        OpacityRole,
        RelativePosRole,
        // The filter in/out should always be 0 - full length
        RequiresInOut,
        ToTimePosRole,
        // Don't display this param in timeline keyframes
        ShowInTimelineRole,
        InRole,
        OutRole,
        ParentInRole,
        ParentPositionRole,
        ParentDurationRole,
        HideKeyframesFirstRole,
        // Obtain the real (distinct) parameters for a fake rect (x, y, w, h)
        FakeRectRole,
        List1Role,
        List2Role,
        Enum1Role,
        Enum2Role,
        Enum3Role,
        Enum4Role,
        Enum5Role,
        Enum6Role,
        Enum7Role,
        Enum8Role,
        Enum9Role,
        Enum10Role,
        Enum11Role,
        Enum12Role,
        Enum13Role,
        Enum14Role,
        Enum15Role,
        Enum16Role
    };

    /** @brief Returns true if @param type is animated */
    static bool isAnimated(ParamType type);

    /** @brief Returns the id of the asset represented by this object */
    QString getAssetId() const;
    const QString getAssetMltId();
    const QString getAssetMltService();
    void setActive(bool active);
    bool isActive() const;

    /** @brief Set the parameter with given name to the given value
     */
    Q_INVOKABLE void setParameter(const QString &name, const QString &paramValue, bool update = true, QModelIndex paramIndex = QModelIndex(),
                                  bool groupedCommand = false);
    void setParameter(const QString &name, int value, bool update = true);

    /** @brief Return all the parameters as pairs (parameter name, parameter value) */
    QVector<QPair<QString, QVariant>> getAllParameters() const;
    /** @brief Get a parameter value from its name */
    const QVariant getParamFromName(const QString &paramName);
    /** @brief Get a parameter index from its name */
    const QModelIndex getParamIndexFromName(const QString &paramName);
    /** @brief Returns a json definition of the effect with all param values
     *  @param selection only export keyframes matching the selected index, or all keyframes if empty
     *  @param includeFixed if true, also export the fixed (non user editable) parameters for this effect
     *  @param percentageExport if true, the animated rect parameters will be exported in percentage instead of pixel value
     */
    QJsonDocument toJson(QVector<int> selection = {}, bool includeFixed = true, bool percentageExport = false) const;
    /** @brief Returns the interpolated value at the given position with all param values as json*/
    QJsonDocument valueAsJson(int pos, bool includeFixed = true) const;

    void savePreset(const QString &presetFile, const QString &presetName);
    void deletePreset(const QString &presetFile, const QString &presetName);
    const QStringList getPresetList(const QString &presetFile) const;
    const QVector<QPair<QString, QVariant>> loadPreset(const QString &presetFile, const QString &presetName);

    /* Which monitor is attached to this asset (clip/project)
     */
    Kdenlive::MonitorId monitorId;

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** @brief Returns the id of the actual object associated with this asset */
    ObjectId getOwnerId() const;

    /** @brief Returns the keyframe model associated with this asset
       Return empty ptr if there is no keyframable parameter in the asset or if prepareKeyframes was not called
     */
    Q_INVOKABLE std::shared_ptr<KeyframeModelList> getKeyframeModel();

    /** @brief Must be called before using the keyframes of this model */
    void prepareKeyframes(int in = -1, int out = -1);
    void resetAsset(std::unique_ptr<Mlt::Properties> asset);
    /** @brief Returns true if the effect has more than one keyframe */
    bool hasMoreThanOneKeyframe() const;
    int time_to_frames(const QString &time) const;
    void passProperties(Mlt::Properties &target);
    /** @brief Returns a list of the parameter names that are keyframable, along with param type and opacity use */
    QMap<QString, std::pair<ParamType, bool>> getKeyframableParameters() const;

    /** @brief Returns the current value of an effect parameter */
    const QString getParam(const QString &paramName);
    /** @brief Returns the current asset */
    Mlt::Properties *getAsset();
    /** @brief Returns a frame time as click time (00:00:00.000) */
    const QString framesToTime(int t) const;
    /** @brief Given an animation keyframe string, find out the keyframe type */
    static const QChar getKeyframeType(const QString keyframeString);

public Q_SLOTS:
    /** @brief Sets the value of a list of parameters
       @param params contains the pairs (parameter name, parameter value)
     */
    void setParameters(const paramVector &params, bool update = true);
    /** @brief Sets the value of a list of parameters. If the parameters have keyframes, ensuring all keyframable params get the keyframes
       @param params contains the pairs (parameter name, parameter value)
     */
    void setParametersFromTask(const paramVector &params);
    /** @brief Set a filter job's progress */
    void setProgress(int progress);

protected:
    /** @brief Helper function to retrieve the type of a parameter given the string corresponding to it*/
    static ParamType paramTypeFromStr(const QString &type);

    static QString getDefaultKeyframes(int start, const QString &defaultValue, bool linearOnly);

    /** @brief Helper function to get an attribute from a dom element, given its name.
       The function additionally parses following keywords:
       - %width and %height that are replaced with profile's height and width.
       If keywords are found, mathematical operations are supported for double type params. For example "%width -1" is a valid value.
    */
    QVariant parseAttribute(const QString &attribute, const QDomElement &element, QVariant defaultValue = QVariant()) const;
    QVariant parseSubAttributes(const QString &attribute, const QDomElement &element) const;

    /** @brief Helper function to register one more parameter that is keyframable.
       @param index is the index corresponding to this parameter
    */
    void addKeyframeParam(const QModelIndex &index, int in, int out);

    /** @brief Check if all parameters for this asset are set to the default */
    bool isDefault() const;

    struct ParamRow
    {
        ParamType type;
        QDomElement xml;
        QVariant value;
        QString name;
    };

    QString m_assetId;
    ObjectId m_ownerId;
    bool m_active;
    bool m_builtIn{false};
    /** @brief Keep track of parameter order, important for sox */
    std::vector<QString> m_paramOrder;
    /** @brief Store all parameters by name */
    std::unordered_map<QString, ParamRow> m_params;
    /** @brief We store values of fixed parameters aside */
    std::unordered_map<QString, QVariant> m_fixedParams;
    /** @brief We store the params name in order of parsing. The order is important (cf some effects like sox) */
    QVector<QString> m_rows;

    std::unique_ptr<Mlt::Properties> m_asset;

    std::shared_ptr<KeyframeModelList> m_keyframes;
    QVector<int> m_selectedKeyframes;
    int m_activeKeyframe;
    /** @brief if true, keyframe tools will be hidden by default */
    bool m_hideKeyframesByDefault;
    /** @brief if true, the effect's in/out will always be synced to clip in/out */
    bool m_requiresInOut;
    /** @brief true if this is an audio effect, used to prevent unnecessary monitor refresh / timeline invalidate */
    bool m_isAudio;
    /** @brief Store a filter's job progress */
    int m_filterProgress;

    /** @brief Set the parameter with given name to the given value. This should be called when first
     *  building an effect in the constructor, so that we don't call shared_from_this
     */
    void internalSetParameter(const QString name, const QString paramValue, const QModelIndex &paramIndex = QModelIndex());
    /** @brief Convert an animated geometry param to percentages
     */
    const QString animationToPercentage(const QString &inputValue) const;

Q_SIGNALS:
    void modelChanged();
    /** @brief inform child effects (in case of bin effect with timeline producers)
     *  that a change occurred and a param update is needed **/
    void updateChildren(const QStringList &names);
    void compositionTrackChanged();
    void replugEffect(std::shared_ptr<AssetParameterModel> asset);
    void rebuildEffect(std::shared_ptr<AssetParameterModel> asset);
    /** @brief Emitted when a builtin effect status changes between enabled/disabled */
    void enabledChange(bool enabled);
    void hideKeyframesChange(bool);
    void showEffectZone(ObjectId id, QPair<int, int> inOut, bool checked);
};
