/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_managesubtitles_ui.h"

class SubtitleModel;
class TimelineController;

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
    void parseList(int ix = -1);

private Q_SLOTS:
    void updateSubtitle(QTreeWidgetItem *item, int column);
    void addSubtitle(const QString name = QString());
    void duplicateSubtitle();
    void deleteSubtitle();
    void importSubtitle();
};
