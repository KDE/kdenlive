/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "bin/model/markerlistmodel.hpp"
#include "renderpresets/renderpresetmodel.hpp"

class RenderRequest
{
public:
    RenderRequest();

    struct RenderJob
    {
        QString playlistPath;
        QString outputPath;
        QString subtitlePath;
    };

    /** @brief Set frame range that should be rendered
     *  @param in The in point in frames. -1 means project start.
     *  @param out The out point in frames. -1 means project end.
     */
    void setBounds(int in, int out);
    void setOutputFile(const QString &filename);
    void setPresetParams(const RenderPresetParams &params);
    void setDelayedRendering(bool enabled);
    void setProxyRendering(bool enabled);
    void setEmbedSubtitles(bool enabled);
    void setTwoPass(bool enabled);
    void setAudioFilePerTrack(bool enabled);
    void setGuideParams(std::weak_ptr<MarkerListModel> model, bool enableMultiExport, int filterCategory);
    void setOverlayData(const QString &data);

    std::vector<RenderJob> process();

    QStringList errorMessages();

private:
    struct RenderSection
    {
        int in;
        int out;
        QString name;
    };

    QString m_overlayData;
    bool m_proxyRendering = false;
    RenderPresetParams m_presetParams;
    bool m_audioFilePerTrack = false;
    bool m_delayedRendering = false;
    QString m_outputFile;
    bool m_embedSubtitles = false;
    int m_boundingIn;
    int m_boundingOut;
    std::weak_ptr<MarkerListModel> m_guidesModel;
    bool m_guideMultiExport = false;
    int m_guideCategory = -1; /// category used as filter if @variable guideMultiExport is @value true
    bool m_twoPass = false;

    QStringList m_errors;

    void setDocGeneralParams(QDomDocument doc, int in, int out);
    void setDocTwoPassParams(int pass, QDomDocument &doc, const QString &outputFile);
    std::vector<RenderSection> getGuideSections();
    static void prepareMultiAudioFiles(std::vector<RenderJob> &jobs, const QDomDocument &doc, const QString &playlistFile, const QString &targetFile,
                                       const QUuid &uuid);

    static QString createEmptyTempFile(const QString &extension);

    /** @brief Create a new empty playlist (*.mlt) file and @returns the filename of the created file.
     *  This is different to @fn createEmptyTempFile since it also takes delayed rendering in to account
     *  and hence might create a persistent file instead of a temp file.
     */
    QString generatePlaylistFile();

    /** @brief Create Render jobs for a render section.
     *  There might be multiple jobs for one section for each pass in case of 2pass or each audio track in case of multi audio track export
     * @param jobs the vector to which the jobs will be added
     */
    void createRenderJobs(std::vector<RenderJob> &jobs, const QDomDocument &doc, const QString &playlistPath, QString outputPath, const QString &subtitlePath,
                          const QUuid &uuid);

    void addErrorMessage(const QString &error);
};
