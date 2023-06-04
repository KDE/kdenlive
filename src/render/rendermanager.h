/*
 * SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include <QMap>
#include <QObject>

#include "bin/model/markerlistmodel.hpp"

class KdenliveDoc;
class QUrl;
class QString;
class QStringList;
class QDomDocument;

class RenderManager
{
public:
    struct RenderJob
    {
        QString playlistPath;
        QString outputPath;
        QString subtitlePath;
        bool embedSubtitle = false;
        bool delayedRendering = false;
    };

    RenderManager();

    // void prepareRendering();
    void prepareRendering(const QUrl &openUrl);
    void prepareRendering(KdenliveDoc *project, const QDomDocument &document);

    void generateRenderFiles(RenderJob job, QDomDocument doc, int in, int out);

    bool startRendering(RenderJob job);

    // QString generatePlaylistFile(bool delayedRendering);

    void prepareMultiAudioFiles(QMap<QString, QString> &renderFiles, const QDomDocument &doc, RenderJob job);

    QString createEmptyTempFile(const QString &extension);

    void disableSubtitles(QDomDocument &doc);
    void setAutoClosePlaylists(QDomDocument &doc);

    bool writeToFile(const QDomDocument &doc, const QString &filename);

    struct RenderSection
    {
        int in;
        int out;
        QString name;
    };

    QMap<QString, QString> m_renderFiles;

Q_SIGNALS:
    void showMessage(const QString &message);
    // void showMessage(QString);

private:
    bool m_useProxies;
    bool m_twoPassRendering;
    // bool m_embedSubtitle;
    bool m_multiAudioExport;
    bool m_guideMultiExport;
    QString m_overlayData;
    // QString m_outputFile;
    std::weak_ptr<MarkerListModel> m_guidesModel;
    QString m_renderer;
    int m_in;
    int m_out;
    std::vector<RenderSection> m_renderSections;

    void createAssetBundle(const RenderJob &job, KdenliveDoc *project);
    std::vector<RenderSection> getGuideSections(int guideCategory, int boundingIn, int boundingOut);
};
