/*
    SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "uiutils.h"
#include "kdenlivesettings.h"

#include <KConfig>
#include <KConfigGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QMap>
#include <QPixmap>
#include <QStandardPaths>

const QStringList UiUtils::getProxySafeParams()
{
    static QStringList allowedParams = {QStringLiteral("f"),        QStringLiteral("vf"),
                                        QStringLiteral("af"),       QStringLiteral("c:v"),
                                        QStringLiteral("c:a"),      QStringLiteral("codec:v"),
                                        QStringLiteral("codec:a"),  QStringLiteral("g"),
                                        QStringLiteral("qscale"),   QStringLiteral("filter_hw_device"),
                                        QStringLiteral("i"),        QStringLiteral("vsync"),
                                        QStringLiteral("fps_mode"), QStringLiteral("ab"),
                                        QStringLiteral("qp"),       QStringLiteral("bf"),
                                        QStringLiteral("crf"),      QStringLiteral("preset"),
                                        QStringLiteral("vendor"),   QStringLiteral("vb"),
                                        QStringLiteral("vprofile"), QStringLiteral("init_hw_device"),
                                        QStringLiteral("hwaccel"),  QStringLiteral("hwaccel_output_format"),
                                        QStringLiteral("resize"),   QStringLiteral("rc"),
                                        QStringLiteral("pix_fmt"),  QStringLiteral("vcodec"),
                                        QStringLiteral("acodec")};
    QStringList customList = KdenliveSettings::safeFFmpegParams();
    customList << allowedParams;
    customList.removeDuplicates();
    return customList;
}

const QStringList UiUtils::getProxyForbiddenParams()
{
    static QStringList forbiddenParams = {QStringLiteral("attach"), QStringLiteral("signature"), QStringLiteral("metadata"),
                                          QStringLiteral("null"),   QStringLiteral("dump"),      QStringLiteral("concat"),
                                          QStringLiteral("safe"),   QStringLiteral("ladspa"),    QStringLiteral("protocol_whitelist"),
                                          QStringLiteral("-ante "), QStringLiteral("-post ")};
    return forbiddenParams;
}

const QStringList UiUtils::getRenderForbiddenParams()
{
    static QStringList forbiddenParams = {QStringLiteral(" ante="), QStringLiteral(" post=")};
    return forbiddenParams;
}

UiUtils *UiUtils::instance()
{
    static UiUtils *instance;
    if (!instance) instance = new UiUtils;

    return instance;
}

UiUtils *UiUtils::create(QQmlEngine *, QJSEngine *)
{
    return instance();
}

QFont UiUtils::getFixedFont()
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

QFont UiUtils::getSmallestReadableFont()
{
    return QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
}

qreal UiUtils::getBaseSizeMedium()
{
    QFontInfo fi(getFixedFont());
    return fi.pixelSize();
}

bool UiUtils::isTrustedProxyProfile(QString proxyData, const QString &extension)
{
    // FFmpeg option values can select file-writing filters and protocols, so
    // option-name validation alone is insufficient for project-supplied data.
    proxyData = proxyData.simplified().remove(QLatin1Char('\\'));
    KConfig config(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    const KConfigGroup group(&config, QStringLiteral("proxy"));
    const QMap<QString, QString> profiles = group.entryMap();
    for (auto profile = profiles.cbegin(); profile != profiles.cend(); ++profile) {
        QString trustedParams = profile.value().section(QLatin1Char(';'), 0, 0).simplified().remove(QLatin1Char('\\'));
        const QString trustedExtension = profile.value().section(QLatin1Char(';'), 1, 1);
        if (proxyData == trustedParams && extension == trustedExtension) {
            return true;
        }
    }
    return false;
}

QStringList UiUtils::checkUnknownProxyParams(QString proxyData)
{
    // Next check if all our params are in the safe list
    if (!proxyData.startsWith(QLatin1Char(' '))) {
        proxyData.prepend(QLatin1Char(' '));
    }
    const QStringList params = proxyData.split(QLatin1String(" -"), Qt::SkipEmptyParts);
    QStringList detectedParams;
    for (auto &p : params) {
        QString paramName = p.section(QLatin1Char(' '), 0, 0);
        if (paramName.contains(QLatin1Char('='))) {
            paramName = p.section(QLatin1Char('='), 0, 0);
        }
        detectedParams << paramName;
    }
    detectedParams.removeDuplicates();
    QStringList unknownParams;
    const QStringList safeList = getProxySafeParams();
    for (auto &d : detectedParams) {
        if (!safeList.contains(d)) {
            unknownParams << d;
        }
    }
    return unknownParams;
}

void UiUtils::addSafeParameters(QStringList unknownParams)
{
    unknownParams << KdenliveSettings::safeFFmpegParams();
    unknownParams.removeDuplicates();
    KdenliveSettings::setSafeFFmpegParams(unknownParams);
}

QIcon UiUtils::rotatedIcon(const QString &iconName, const QSize iconSize, qreal rotation)
{
    QIcon icon = QIcon::fromTheme(iconName);
    QPixmap pix = icon.pixmap(iconSize);
    QTransform trans;
    trans.rotate(rotation);
    pix = pix.transformed(trans);
    return QIcon(pix);
}

QString UiUtils::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, const QString &extension)
{
    QString selectedFile = QFileDialog::getSaveFileName(parent, caption, dir, filter);
    if (!selectedFile.isEmpty() && QFileInfo(selectedFile).suffix().isEmpty()) {
        selectedFile.append(extension);
    }
    return selectedFile;
}
