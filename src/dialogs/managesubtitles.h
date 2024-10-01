/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_managesubtitles_ui.h"

class SubtitleModel;
class TimelineController;

class SideBarDropFilter : public QObject
{
    Q_OBJECT
public:
    explicit SideBarDropFilter(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
Q_SIGNALS:
    void drop(QObject *obj, QDropEvent *event);
};

/**
 * @class ManageSubtitles
 * @brief A dialog for managing project subtitles.
 * @author Jean-Baptiste Mardelle
 */
class ManageSubtitles : public QDialog, public Ui::ManageSubtitles_UI
{
    Q_OBJECT

public:
    explicit ManageSubtitles(std::shared_ptr<SubtitleModel> model, TimelineController *controller, int currentIx, QWidget *parent = nullptr);
    ~ManageSubtitles() override;

private:
    std::shared_ptr<SubtitleModel> m_model;
    TimelineController *m_controller;
    void parseFileList();
    void parseEventList();
    void parseStyleList(bool global);
    void parseInfoList();
    void parseSideBar();

    int m_activeSubFile{-1};

private Q_SLOTS:
    void updateSubtitle(QTreeWidgetItem *item, int column);
    void addSubtitleFile(const QString name = QString());
    void duplicateFile();
    void deleteFile();
    void addLayer();
    void deleteLayer();
    void duplicateLayer();
    void addStyle(bool global);
    void deleteStyle(bool global);
    void duplicateStyle(bool global);
    void editStyle(bool global);
    void importSubtitleFile();
};
