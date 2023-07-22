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
    enum MissingStatus { Fixed, Reloaded, Missing, Placeholder, Remove };
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
     * @brief checks for problems with the clips in the project
     * Checks for missing proxies, wrong duration clips, missing fonts, missing images, missing source clips
     * Calls DocumentChecker::checkMissingImagesAndFonts () /n
     * Called by KdenliveDoc::checkDocumentClips ()        /n
     * @return
     */
    bool hasErrorInClips();
    static QString fixLumaPath(const QString &file);
    static QString searchLuma(const QDir &dir, const QString &file);

    static QString readableNameForClipType(ClipType::ProducerType type);
    static QString readableNameForMissingType(MissingType type);
    static QString readableNameForMissingStatus(MissingStatus type);

    static QString searchPathRecursively(const QDir &dir, const QString &fileName, ClipType::ProducerType type = ClipType::Unknown);
    static QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash, const QString &fileName);
    static QString searchDirRecursively(const QDir &dir, const QString &matchHash, const QString &fullName);

    void tempTest();

private Q_SLOTS:
    void acceptDialog();

    /** @brief Check if images and fonts in this clip exists, returns a list of images that do exist so we don't check twice. */
    void checkMissingImagesAndFonts(const QStringList &images, const QStringList &fonts, const QString &id);
    // void slotCheckButtons();

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

    bool m_abortSearch;
    bool m_checkRunning;

    QString ensureAbsoultePath(QString filepath);
    static QStringList getAssetsFiles(const QDomDocument &doc, const QString &tagName, const QMap<QString, QString> &searchPairs);
    static QStringList getAssetsServiceIds(const QDomDocument &doc, const QString &tagName);
    static void removeAssetsById(QDomDocument &doc, const QString &tagName, const QStringList &idsToDelete);

    static bool isMltBuildInLuma(const QString &lumaName);

    // void fixClipItem(QTreeWidgetItem *child, const QDomNodeList &producers, const QDomNodeList &chains, const QDomNodeList &trans, const QDomNodeList
    // &filters);
    // void fixSourceClipItem(QTreeWidgetItem *child, const QDomNodeList &producers, const QDomNodeList &chains);
    void fixProxyClip(const QString &id, const QString &oldUrl, const QString &newUrl);
    void doFixProxyClip(QDomElement &e, const QString &oldUrl, const QString &newUrl);
    /** @brief Returns list of transitions ids / tag containing luma files */
    const QMap<QString, QString> getLumaPairs() const;
    /** @brief Returns list of filters ids / tag containing asset files */
    const QMap<QString, QString> getAssetPairs() const;
    /** @brief Remove _missingsourcec flag in fixed clips */
    void fixMissingSource(const QString &id, const QDomNodeList &producers, const QDomNodeList &chains);
    /** @brief Check if the producer has an id. If not (should happen, but...) try to recover it
     *  @returns true if the producer has been changed (id recovered), false if it was either already okay or could not be recovered
     */
    bool ensureProducerHasId(QDomElement &producer, const QDomNodeList &entries);
    /** @brief Check if the producer represents an "invalid" placeholder (project saved with missing source). If such a placeholder is detected, it tries to
     * recover the original clip.
     *  @returns true if the producer has been changed (recovered), false if it was either already okay or could not be recovered
     */
    bool ensureProducerIsNotPlaceholder(QDomElement &producer);

    void setReloadProxy(QDomElement &producer, const QString &realPath);

    /** @brief Check for various missing elements */
    QString getMissingProducers(QDomElement &e, const QDomNodeList &entries, const QStringList &verifiedPaths, const QString &storageFolder);
    /** @brief If project path changed, try to relocate its resources */
    const QString relocateResource(QString sourceResource);

    static ClipType::ProducerType getClipType(const QString &service, const QString &resource);
    QString getProducerResource(const QDomElement &producer);
    static QString getKdenliveClipId(const QDomElement &producer);

    QStringList getInfoMessages();

    // TODO!!!
    bool itemsContain(const QString &path, MissingType type, MissingStatus status = MissingStatus::Missing);
    int itemIndexByClipId(const QString &clipId);
    bool itemsByTypeAndStatus(MissingType type, MissingStatus status = MissingStatus::Missing);

    static bool isSlideshow(const QString &resource);
    bool isSequenceWithSpeedEffect(const QDomElement &producer);
    bool isProfileHD(const QDomDocument &doc);

    void fixSourceClipItem(const DocumentChecker::DocumentResource &resource, const QDomNodeList &producers, const QDomNodeList &chains);
    void fixMissingItem(const DocumentChecker::DocumentResource &resource, const QDomNodeList &producers, const QDomNodeList &chains, const QDomNodeList &trans,
                        const QDomNodeList &filters);

    void fixTitleImage(QDomElement &e, const QString &oldPath, const QString &newPath);
    void fixTitleFont(const QDomNodeList &producers);
    void fixAsset(const QDomNodeList &assets, const QMap<QString, QString> &searchPairs, const QString &oldPath, const QString &newPath);
    void usePlaceholderForClip(const QDomNodeList &items, const QString &clipId);
    void removeClip(const QDomNodeList &producers, const QDomNodeList &playlists, const QString &clipId);
    void fixClip(const QDomNodeList &items, const QString &clipId, const QString &newPath);

    QStringList fixSequences(QDomElement &e, const QDomNodeList &producers, const QStringList &tractorIds);

    void applyChanges();

Q_SIGNALS:
    void showScanning(const QString);
};
