/*
   SPDX-FileCopyrightText: 2017 Nicolas Carion
   SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
   This file is part of Kdenlive. See www.kdenlive.org.
*/

#include "renderpresetmodel.hpp"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "renderpresetrepository.hpp"

#include <mlt++/MltRepository.h>

#include <KLocalizedString>
#include <KMessageWidget>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <memory>

QString RenderPresetParams::toString()
{
    QString string;
    RenderPresetParams::const_iterator i = this->constBegin();
    while (i != this->constEnd()) {
        string.append(QStringLiteral("%1=%2 ").arg(i.key(), i.value()));
        ++i;
    }
    return string.simplified();
}

void RenderPresetParams::insertFromString(const QString &params, bool overwrite)
{
    // split only at white spaces followed by a new parameter
    // to avoid splitting values that contain whitespaces
    static const QRegularExpression regexp(R"(\s+(?=\S*=))");

    QStringList paramList = params.split(regexp, Qt::SkipEmptyParts);
    for (QString param : paramList) {
        if (!param.contains(QLatin1Char('='))) {
            // invalid param, skip
            continue;
        }
        const QString key = param.section(QLatin1Char('='), 0, 0);
        const QString value = param.section(QLatin1Char('='), 1);
        if (contains(key) && !overwrite) {
            // don't overwrite existing params
            continue;
        }
        insert(key, value);
    }
}

void RenderPresetParams::replacePlaceholder(const QString &placeholder, const QString &newValue)
{
    RenderPresetParams newParams;
    RenderPresetParams::const_iterator i = this->constBegin();
    while (i != this->constEnd()) {
        QString value = i.value();
        newParams.insert(i.key(), value.replace(placeholder, newValue));
        ++i;
    }

    clear();
    QMap<QString, QString>::const_iterator j = newParams.constBegin();
    while (j != newParams.constEnd()) {
        insert(j.key(), j.value());
        ++j;
    }
}

void RenderPresetParams::refreshX265Params()
{
    if (!isX265()) {
        remove(QStringLiteral("x265-params"));
        return;
    }
    QStringList x265Params;
    if (contains(QStringLiteral("crf"))) {
        x265Params.append(QStringLiteral("crf=%1").arg(value(QStringLiteral("crf"))));
    }
    if (videoRateControl() == RenderPresetParams::Constant && contains(QStringLiteral("vb"))) {
        QString vb = value(QStringLiteral("vb"));
        QString newVal;
        if (vb == QStringLiteral("%bitrate") || vb == QStringLiteral("%bitrate+'k")) {
            newVal = QStringLiteral("%bitrate");
        } else {
            newVal = vb.replace('k', QLatin1String("")).replace('M', QStringLiteral("000"));
        }
        x265Params.append(QStringLiteral("bitrate=%1").arg(newVal));
        x265Params.append(QStringLiteral("vbv-maxrate=%1").arg(newVal));
    } else if (videoRateControl() == RenderPresetParams::Constrained && contains(QStringLiteral("vmaxrate"))) {
        QString vmax = value(QStringLiteral("vmaxrate"));
        QString newVal;
        if (vmax == QStringLiteral("%bitrate") || vmax == QStringLiteral("%bitrate+'k")) {
            newVal = QStringLiteral("%bitrate");
        } else {
            newVal = vmax.replace('k', QLatin1String("")).replace('M', QStringLiteral("000"));
        }
        x265Params.append(QStringLiteral("vbv-maxrate=%1").arg(newVal));
    }
    if (contains(QStringLiteral("vbufsize"))) {
        int val = value(QStringLiteral("vbufsize")).toInt();
        x265Params.append(QStringLiteral("vbv-bufsize=%1").arg(val / 1024));
    }
    if (contains(QStringLiteral("bf"))) {
        x265Params.append(QStringLiteral("bframes=%1").arg(value(QStringLiteral("bf"))));
    }
    if (contains(QStringLiteral("g"))) {
        x265Params.append(QStringLiteral("keyint=%1").arg(value(QStringLiteral("g"))));
    }
    if (contains(QStringLiteral("top_field_first")) && value(QStringLiteral("progressive")).toInt() == 0) {
        int val = value(QStringLiteral("top_field_first")).toInt();
        x265Params.append(QStringLiteral("interlace=%1").arg(val == 1 ? QStringLiteral("tff") : QStringLiteral("bff")));
    }
    if (contains(QStringLiteral("sc_threshold")) && value(QStringLiteral("sc_threshold")).toInt() == 0) {
        x265Params.append(QStringLiteral("scenecut=0"));
    }
    if (contains(QStringLiteral("x265-params"))) {
        x265Params.append(value(QStringLiteral("x265-params")));
    }

    if (x265Params.isEmpty()) {
        remove(QStringLiteral("x265-params"));
    } else {
        insert(QStringLiteral("x265-params"), x265Params.join(QStringLiteral(":")));
    }
}

RenderPresetParams::RateControl RenderPresetParams::videoRateControl() const
{
    QString vbufsize = value(QStringLiteral("vbufsize"));
    QString vcodec = value(QStringLiteral("vcodec"));
    if (contains(QStringLiteral("crf"))) {
        return !vbufsize.isEmpty() ? (vcodec.endsWith("_videotoolbox") ? RateControl::Average : RateControl::Quality) : RateControl::Constrained;
    }
    if (contains(QStringLiteral("vq")) || contains(QStringLiteral("vqp")) || contains(QStringLiteral("vglobal_quality")) ||
        contains(QStringLiteral("qscale"))) {
        return vbufsize.isEmpty() ? RateControl::Quality : RateControl::Constrained;
    } else if (!vbufsize.isEmpty()) {
        return RateControl::Constant;
    }
    QString param = value(QStringLiteral("vb"));
    param = param.replace('+', "").replace('k', "").replace('k', "").replace('M', "000");
    if (!param.isEmpty()) {
        return (param.contains(QStringLiteral("%bitrate")) || param.toInt() > 0) ? RateControl::Average : RateControl::Quality;
    }
    return RateControl::Unknown;
}

bool RenderPresetParams::hasAlpha()
{
    const QStringList alphaFormats = {QLatin1String("argb"), QLatin1String("abgr"), QLatin1String("bgra"), QLatin1String("rgba"),
                                      QLatin1String("gbra"), QLatin1String("yuva"), QLatin1String("ya"),   QLatin1String("yuva")};
    const QString selected = value(QStringLiteral("pix_fmt"));
    for (auto &f : alphaFormats) {
        if (selected.startsWith(f)) {
            return true;
        }
    }
    return false;
}

bool RenderPresetParams::isImageSequence()
{
    return value(QStringLiteral("properties")).startsWith("stills/") || value(QStringLiteral("f")).startsWith(QLatin1String("image"));
}

bool RenderPresetParams::isX265()
{
    return value(QStringLiteral("vcodec")).toLower() == QStringLiteral("libx265");
}

RenderPresetModel::RenderPresetModel(QDomElement preset, const QString &presetFile, bool editable, const QString &groupName, const QString &renderer)
    : m_presetFile(presetFile)
    , m_editable(editable)
    , m_name(preset.attribute(QStringLiteral("name")))
    , m_note()
    , m_standard(preset.attribute(QStringLiteral("standard")))
    , m_params()
    , m_groupName(preset.attribute(QStringLiteral("category"), groupName))
    , m_renderer(renderer)
    , m_url(preset.attribute(QStringLiteral("url")))
    , m_speeds(preset.attribute(QStringLiteral("speeds")))
    , m_defaultSpeedIndex(preset.attribute(QStringLiteral("defaultspeedindex"), QStringLiteral("-1")).toInt())
    , m_topFieldFirst(preset.attribute(QStringLiteral("top_field_first")))
{
    if (m_groupName.isEmpty()) {
        m_groupName = i18nc("Category Name", "Custom");
    }

    m_extension = preset.attribute(QStringLiteral("extension"));
    m_manual = preset.attribute(QStringLiteral("manual")) == QLatin1String("1");

    QTextDocument docConvert;
    docConvert.setHtml(preset.attribute(QStringLiteral("args")));
    // setParams after we know the extension to make setting the format automatically work
    setParams(docConvert.toPlainText().simplified());

    if (m_defaultSpeedIndex < 0) {
        m_defaultSpeedIndex = (speeds().count() - 1) * 0.75;
    }

    m_vQualities = preset.attribute(QStringLiteral("qualities"));
    m_defaultVQuality = preset.attribute(QStringLiteral("defaultquality"));
    m_vBitrates = preset.attribute(QStringLiteral("bitrates"));
    m_defaultVBitrate = preset.attribute(QStringLiteral("defaultbitrate"));

    m_aQualities = preset.attribute(QStringLiteral("audioqualities"));
    m_defaultAQuality = preset.attribute(QStringLiteral("defaultaudioquality"));
    m_aBitrates = preset.attribute(QStringLiteral("audiobitrates"));
    m_defaultABitrate = preset.attribute(QStringLiteral("defaultaudiobitrate"));

    checkPreset();
}

RenderPresetModel::RenderPresetModel(const QString &groupName, const QString &path, QString presetName, const QString &params, bool codecInName)
    : m_editable(false)
    , m_manual(false)
    , m_params()
    , m_groupName(groupName)
    , m_renderer(QStringLiteral("avformat"))
    , m_defaultSpeedIndex(-1)
{
    KConfig config(path, KConfig::SimpleConfig);
    KConfigGroup group = config.group(QByteArray());
    QString vcodec = group.readEntry("vcodec");
    QString acodec = group.readEntry("acodec");
    m_extension = group.readEntry("meta.preset.extension");
    // setParams after we know the extension to make setting the format automatically work
    setParams(params);
    m_note = group.readEntry("meta.preset.note");

    if (codecInName && (!vcodec.isEmpty() || !acodec.isEmpty())) {
        presetName.append(" (");
        if (!vcodec.isEmpty()) {
            presetName.append(vcodec);
            if (!acodec.isEmpty()) {
                presetName.append("+" + acodec);
            }
        } else if (!acodec.isEmpty()) {
            presetName.append(acodec);
        }
        presetName.append(QLatin1Char(')'));
    }
    m_name = presetName;
    checkPreset();
}

RenderPresetModel::RenderPresetModel(const QString &name, const QString &groupName, const QString &params, const QString &extension,
                                     const QString &defaultVBitrate, const QString &defaultVQuality, const QString &defaultABitrate,
                                     const QString &defaultAQuality, const QString &speedsString, bool manualPreset)
    : m_presetFile()
    , m_editable()
    , m_name(name)
    , m_note()
    , m_standard()
    , m_extension(extension)
    , m_groupName(groupName)
    , m_renderer(QStringLiteral("avformat"))
    , m_url()
    , m_speeds(speedsString)
    , m_topFieldFirst()
    , m_vBitrates()
    , m_defaultVBitrate(defaultVBitrate)
    , m_vQualities()
    , m_defaultVQuality(defaultVQuality)
    , m_aBitrates()
    , m_defaultABitrate(defaultABitrate)
    , m_aQualities()
    , m_defaultAQuality(defaultAQuality)
{
    setParams(params);
    if (m_groupName.isEmpty()) {
        m_groupName = i18nc("Category Name", "Custom");
    }

    m_defaultSpeedIndex = (speeds().count() - 1) * 0.75;
    m_manual = manualPreset;

    checkPreset();
}

void RenderPresetModel::checkPreset()
{
    QStringList acodecs = RenderPresetRepository::acodecs();
    QStringList vcodecs = RenderPresetRepository::vcodecs();
    QStringList supportedFormats = RenderPresetRepository::supportedFormats();

    bool replaceVorbisCodec = acodecs.contains(QStringLiteral("libvorbis"));
    bool replaceLibfaacCodec = acodecs.contains(QStringLiteral("libfaac"));

    if (replaceVorbisCodec && (m_params.value(QStringLiteral("acodec")) == QStringLiteral("vorbis"))) {
        // replace vorbis with libvorbis
        m_params[QStringLiteral("acodec")] = QLatin1String("libvorbis");
    }
    if (replaceLibfaacCodec && (m_params.value(QStringLiteral("acodec")) == QStringLiteral("aac"))) {
        // replace aac with libfaac
        m_params[QStringLiteral("acodec")] = QLatin1String("libfaac");
    }

    // We borrow a reference to the profile's pointer to query it more easily
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    double project_framerate = double(profile->frame_rate_num()) / profile->frame_rate_den();

    if (m_standard.isEmpty() ||
        (m_standard.contains(QStringLiteral("PAL"), Qt::CaseInsensitive) && profile->frame_rate_num() == 25 && profile->frame_rate_den() == 1) ||
        (m_standard.contains(QStringLiteral("NTSC"), Qt::CaseInsensitive) && profile->frame_rate_num() == 30000 && profile->frame_rate_den() == 1001)) {
        // Standard OK
    } else {
        m_errors = i18n("Standard (%1) not compatible with project profile (%2)", m_standard, project_framerate);
        return;
    }

    if (hasParam(QStringLiteral("mlt_profile"))) {
        QString profile_str = getParam(QStringLiteral("mlt_profile"));
        std::unique_ptr<ProfileModel> &target_profile = ProfileRepository::get()->getProfile(profile_str);
        if (target_profile->frame_rate_den() > 0) {
            double profile_rate = double(target_profile->frame_rate_num()) / target_profile->frame_rate_den();
            if (int(1000.0 * profile_rate) != int(1000.0 * project_framerate)) {
                m_errors = i18n("Frame rate (%1) not compatible with project profile (%2)", profile_rate, project_framerate);
                return;
            }
        }
    }

    if (hasParam(QStringLiteral("properties"))) {
        // This is a native MLT render profile, check the file for codec
        QString presetName = getParam(QStringLiteral("properties"));
        presetName.prepend(QStringLiteral("consumer/avformat/"));
        std::unique_ptr<Mlt::Properties> presets(pCore->getMltRepository()->presets());
        std::unique_ptr<Mlt::Properties> preset(new Mlt::Properties(mlt_properties(presets->get_data(presetName.toUtf8().constData()))));
        const QString format = QString(preset->get("f")).toLower();
        if (!format.isEmpty() && !supportedFormats.contains(format)) {
            m_errors = i18n("Unsupported video format: %1", format);
            return;
        }
        const QString vcodec = QString(preset->get("vcodec")).toLower();
        if (!vcodec.isEmpty() && !vcodecs.contains(vcodec)) {
            m_errors = i18n("Unsupported video codec: %1", vcodec);
            return;
        }
        const QString acodec = QString(preset->get("acodec")).toLower();
        if (!acodec.isEmpty() && !acodecs.contains(acodec)) {
            m_errors = i18n("Unsupported audio codec: %1", acodec);
            return;
        }
    }

    // Make sure the selected preset uses an installed avformat codec / format
    QString format;
    format = getParam(QStringLiteral("f")).toLower();
    if (!format.isEmpty() && !supportedFormats.contains(format)) {
        m_errors = i18n("Unsupported video format: %1", format);
        return;
    }

    // check for missing audio codecs
    format = getParam(QStringLiteral("acodec")).toLower();
    if (!format.isEmpty() && !acodecs.contains(format)) {
        m_errors = i18n("Unsupported audio codec: %1", format);
        return;
    }
    // check for missing video codecs
    format = getParam(QStringLiteral("vcodec")).toLower();
    if (!format.isEmpty() && !vcodecs.contains(format)) {
        m_errors = i18n("Unsupported video codec: %1", format);
        return;
    }

    if (hasParam(QStringLiteral("profile"))) {
        m_warnings = i18n("This render preset uses a 'profile' parameter.<br />Unless you know what you are doing you will probably "
                          "have to change it to 'mlt_profile'.");
        return;
    }
}

QDomElement RenderPresetModel::toXml()
{
    QDomDocument doc;
    QDomElement profileElement = doc.createElement(QStringLiteral("profile"));
    doc.appendChild(profileElement);

    profileElement.setAttribute(QStringLiteral("name"), m_name);
    profileElement.setAttribute(QStringLiteral("category"), m_groupName);
    if (!m_extension.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("extension"), m_extension);
    }
    if (m_manual) {
        profileElement.setAttribute(QStringLiteral("manual"), QStringLiteral("1"));
    }
    profileElement.setAttribute(QStringLiteral("args"), m_params.toString());
    if (!m_defaultVBitrate.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultbitrate"), m_defaultVBitrate);
    }
    if (!m_vBitrates.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("bitrates"), m_vBitrates);
    }
    if (!m_defaultVQuality.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultquality"), m_defaultVQuality);
    }
    if (!m_vQualities.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("qualities"), m_vQualities);
    }
    if (!m_defaultABitrate.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultaudiobitrate"), m_defaultABitrate);
    }
    if (!m_aBitrates.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("audiobitrates"), m_aBitrates);
    }
    if (!m_defaultAQuality.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultaudioquality"), m_defaultAQuality);
    }
    if (!m_aQualities.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("audioqualities"), m_aQualities);
    }
    if (!m_speeds.isEmpty()) {
        // profile has a variable speed
        profileElement.setAttribute(QStringLiteral("speeds"), m_speeds);
        if (m_defaultSpeedIndex > 0) {
            profileElement.setAttribute(QStringLiteral("defaultspeedindex"), m_defaultSpeedIndex);
        }
    }
    return doc.documentElement();
}

void RenderPresetModel::setParams(const QString &params)
{
    m_params.clear();
    m_params.insertFromString(params, true);

    if (!hasParam(QStringLiteral("f")) && !m_extension.isEmpty() && RenderPresetRepository::supportedFormats().contains(m_extension)) {
        m_params.insert(QStringLiteral("f"), m_extension);
    }
}

RenderPresetParams RenderPresetModel::params(QStringList removeParams) const
{
    RenderPresetParams newParams = m_params;
    for (QString toRemove : removeParams) {
        newParams.remove(toRemove);
    }
    return newParams;
}

QStringList RenderPresetModel::defaultValues() const
{
    QString defaultSpeedParams;
    QStringList presetSpeeds = speeds();
    if (!presetSpeeds.isEmpty() && m_defaultSpeedIndex < presetSpeeds.count()) {
        defaultSpeedParams = presetSpeeds.at(m_defaultSpeedIndex);
    }
    return {defaultSpeedParams, m_defaultABitrate, m_defaultAQuality, m_defaultVBitrate, m_defaultVQuality};
}

QString RenderPresetModel::extension() const
{
    if (!m_extension.isEmpty()) {
        return m_extension;
    }
    return getParam(QStringLiteral("f"));
};

QStringList RenderPresetModel::audioBitrates() const
{
    return m_aBitrates.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QString RenderPresetModel::defaultABitrate() const
{
    return m_defaultABitrate;
}

QStringList RenderPresetModel::audioQualities() const
{
    if (!m_aQualities.isEmpty()) {
        return m_aQualities.split(QLatin1Char(','), Qt::SkipEmptyParts);
    } else {
        // ATTENTION: historically qualities are sorted from best to worse for some reason
        QString acodec = getParam(QStringLiteral("acodec")).toLower();
        if (acodec == "libmp3lame") {
            return {"0", "9"};
        } else if (acodec == "libvorbis" || acodec == "vorbis" || acodec == "libopus") {
            return {"10", "0"};
        } else {
            return {"500", "0"};
        }
    }
}

QString RenderPresetModel::defaultAQuality() const
{
    return m_defaultAQuality;
}

QStringList RenderPresetModel::videoBitrates() const
{
    return m_vBitrates.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QString RenderPresetModel::defaultVBitrate() const
{
    return m_defaultVBitrate;
}

QStringList RenderPresetModel::videoQualities() const
{
    if (!m_vQualities.isEmpty()) {
        return m_vQualities.split(QLatin1Char(','), Qt::SkipEmptyParts);
    } else {
        // ATTENTION: historically qualities are sorted from best to worse for some reason
        QString vcodec = getParam(QStringLiteral("vcodec")).toLower();
        if (vcodec == "libx265" || vcodec.contains("nvenc") || vcodec.endsWith("_amf") || vcodec.startsWith("libx264") || vcodec.endsWith("_vaapi") ||
            vcodec.endsWith("_qsv")) {
            return {"0", "51"};
        } else if (vcodec.startsWith("libvpx") || vcodec.startsWith("libaom-")) {
            return {"0", "63"};
        } else if (vcodec.startsWith("libwebp")) {
            return {"100", "0"};
        } else {
            return {"1", "31"};
        }
    }
}

QString RenderPresetModel::defaultVQuality() const
{
    return m_defaultVQuality;
}

bool RenderPresetModel::editable() const
{
    return m_editable;
}

QStringList RenderPresetModel::speeds() const
{
    return m_speeds.split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

QString RenderPresetModel::getParam(const QString &name) const
{
    return params().value(name);
}

bool RenderPresetModel::isManual() const
{
    return m_manual;
}

bool RenderPresetModel::hasParam(const QString &name) const
{
    // return params().contains(name);
    return !getParam(name).isEmpty();
}

RenderPresetParams::RateControl RenderPresetModel::audioRateControl() const
{
    QString value = getParam(QStringLiteral("vbr"));
    if (!value.isEmpty()) {
        // libopus rate mode
        if (value == QStringLiteral("off")) {
            return RenderPresetParams::Constant;
        }
        if (value == QStringLiteral("constrained")) {
            return RenderPresetParams::Average;
        }
        return RenderPresetParams::Quality;
    }
    if (hasParam(QStringLiteral("aq")) || hasParam(QStringLiteral("compression_level"))) {
        return RenderPresetParams::Quality;
    }
    if (hasParam(QStringLiteral("ab"))) {
        return RenderPresetParams::Constant;
    }
    return RenderPresetParams::Unknown;
}

RenderPresetModel::InstallType RenderPresetModel::installType() const
{
    if (m_editable) {
        if (m_presetFile.endsWith(QLatin1String("customprofiles.xml"))) {
            return InstallType::Custom;
        } else {
            return InstallType::Download;
        }
    }
    return InstallType::BuildIn;
}

bool RenderPresetModel::hasFixedSize() const
{
    // TODO
    return hasParam(QStringLiteral("s")) || m_params.contains(QLatin1String("%dv_standard"));
}

QString RenderPresetModel::error() const
{
    return m_errors;
}

QString RenderPresetModel::warning() const
{
    return m_warnings;
}

int RenderPresetModel::estimateFileSize(int length)
{
    Q_UNUSED(length)
    // TODO
    /*RateControl vrc = videoRateControl();
    if (vrc == Average || vrc == Constant) {

        return m_defaultVBitrate.toInt() * length;
    }*/
    return -1;
}
