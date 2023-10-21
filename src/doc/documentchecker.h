/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_missingclips_ui.h"

#include <QDir>
#include <QDomElement>
#include <QUrl>

class DocumentChecker : public QObject
{
    Q_OBJECT

public:
    enum MissingStatus { Fixed, Reload, Missing, MissingButProxy, Placeholder, Remove };
    enum MissingType { Clip, Proxy, Luma, AssetFile, TitleImage, TitleFont, Effect, Transition };
    struct DocumentResource
    {
        MissingStatus status = MissingStatus::Missing;
        MissingType type;
        QString originalFilePath;
        QString newFilePath;
        QString clipId;
        QString hash;
        QString fileSize;
        ClipType::ProducerType clipType;
    };

    explicit DocumentChecker(QUrl url, const QDomDocument &doc);
    ~DocumentChecker() override;
    /**
     * @brief checks for problems with the project
     * Checks for missing proxies, wrong duration clips, missing fonts, missing images, missing source clips, missing effects, missing transitions
     * @returns whether error have been found
     */
    bool hasErrorInProject();
    static QString fixLutFile(const QString &file);
    static QString fixLumaPath(const QString &file);
    static QString searchLuma(const QDir &dir, const QString &file);

    static QString readableNameForClipType(ClipType::ProducerType type);
    static QString readableNameForMissingType(MissingType type);
    static QString readableNameForMissingStatus(MissingStatus type);

    static QString searchPathRecursively(const QDir &dir, const QString &fileName, ClipType::ProducerType type = ClipType::Unknown);
    static QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash, const QString &fileName);
    static QString searchDirRecursively(const QDir &dir, const QString &matchHash, const QString &fullName);

    bool resolveProblemsWithGUI();
    /* @brief Get a count of missing items in each category */
    QMap<DocumentChecker::MissingType, int> getCheckResults();

    std::vector<DocumentResource> resourceItems() { return m_items; }

private:
    QUrl m_url;
    QDomDocument m_doc;
    QString m_documentid;
    QString m_root;
    QPair<QString, QString> m_rootReplacement;

    QDomNodeList m_binEntries;
    std::vector<DocumentResource> m_items;

    QStringList m_safeImages;
    QStringList m_safeFonts;

    QStringList m_binIds;
    QStringList m_warnings;

    QString ensureAbsolutePath(QString filepath);

    /** @brief Returns list of transitions ids / tag containing luma files */
    const QMap<QString, QString> getLumaPairs() const;
    /** @brief Returns list of filters ids / tag containing asset files */
    const QMap<QString, QString> getAssetPairs() const;

    /** @brief Check if the producer has an id. If not (should not happen, but...) try to recover it
     *  @returns true if the producer has been changed (id recovered), false if it was either already okay or could not be recovered
     */
    bool ensureProducerHasId(QDomElement &producer, const QDomNodeList &entries);
    /** @brief Check if the producer represents an "invalid" placeholder (project saved with missing source). If such a placeholder is detected, it tries to
     * recover the original clip.
     *  @returns true if the producer has been changed (recovered), false if it was either already okay or could not be recovered
     */
    bool ensureProducerIsNotPlaceholder(QDomElement &producer);

    /** @brief Check for various missing elements */
    QString getMissingProducers(QDomElement &e, const QDomNodeList &entries, const QString &storageFolder);
    /** @brief Check if images and fonts in this clip exists, returns a list of images that do exist so we don't check twice. */
    void checkMissingImagesAndFonts(const QStringList &images, const QStringList &fonts, const QString &id);
    /** @brief If project path changed, try to relocate its resources */
    const QString relocateResource(QString sourceResource);

    static ClipType::ProducerType getClipType(const QString &service, const QString &resource);
    QString getProducerResource(const QDomElement &producer);
    static QString getKdenliveClipId(const QDomElement &producer);
    static QStringList getAssetsFiles(const QDomDocument &doc, const QString &tagName, const QMap<QString, QString> &searchPairs);
    static QStringList getAssetsServiceIds(const QDomDocument &doc, const QString &tagName);

    QStringList getInfoMessages();

    static bool isMltBuildInLuma(const QString &lumaName);
    static bool isSlideshow(const QString &resource);
    bool isSequenceWithSpeedEffect(const QDomElement &producer);
    static bool isProfileHD(const QDomDocument &doc);

    void fixMissingItem(const DocumentChecker::DocumentResource &resource, const QDomNodeList &producers, const QDomNodeList &chains, const QDomNodeList &trans,
                        const QDomNodeList &filters);

    bool itemsContain(MissingType type, const QString &path = QString(), MissingStatus status = MissingStatus::Missing);
    int itemIndexByClipId(const QString &clipId);

    void fixTitleImage(QDomElement &e, const QString &oldPath, const QString &newPath);
    void fixTitleFont(const QDomNodeList &producers, const QString &oldFont, const QString &newFont);
    void fixAssetResource(const QDomNodeList &assets, const QMap<QString, QString> &searchPairs, const QString &oldPath, const QString &newPath);
    static void removeAssetsById(QDomDocument &doc, const QString &tagName, const QStringList &idsToDelete);
    /** @brief Update a filter's tag and id with a new name */
    static void fixAssetsById(QDomDocument &doc, const QString &tagName, const QString &oldId, const QString &newId);
    void usePlaceholderForClip(const QDomNodeList &items, const QString &clipId);
    void removeClip(const QDomNodeList &producers, const QDomNodeList &chains, const QDomNodeList &playlists, const QString &clipId);
    void fixClip(const QDomNodeList &items, const QString &clipId, const QString &newPath);

    void fixProxyClip(const QDomNodeList &items, const QString &id, const QString &oldUrl, const QString &newUrl);
    void removeProxy(const QDomNodeList &items, const QString &clipId, bool recreate);
    /** @brief Remove _missingsourcec flag in fixed clips */
    void fixMissingSource(const QString &id, const QDomNodeList &producers, const QDomNodeList &chains);

    QStringList fixSequences(QDomElement &e, const QDomNodeList &producers, const QStringList &tractorIds);
};

QDebug operator<<(QDebug qd, const DocumentChecker::DocumentResource &item);
