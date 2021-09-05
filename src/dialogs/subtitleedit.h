/*
 *   SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef SUBTITLEEDIT_H
#define SUBTITLEEDIT_H

#include "ui_editsub_ui.h"

#include "definitions.h"


class SubtitleModel;
class TimecodeDisplay;

class ShiftEnterFilter : public QObject
{
    Q_OBJECT
public:
    ShiftEnterFilter(QObject *parent = nullptr);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
signals:
    void triggerUpdate();
};  


/**
 * @class SubtitleEdit: Subtitle edit widget
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */
class SubtitleEdit : public QWidget, public Ui::SubEdit_UI
{
    Q_OBJECT

public:
    explicit SubtitleEdit(QWidget *parent = nullptr);
    void setModel(std::shared_ptr<SubtitleModel> model);

public slots:
    void setActiveSubtitle(int id);

private slots:
    void updateSubtitle();
    void goToPrevious();
    void goToNext();

private:
    std::shared_ptr<SubtitleModel> m_model;
    int m_activeSub{-1};
    TimecodeDisplay *m_position;
    TimecodeDisplay *m_endPosition;
    TimecodeDisplay *m_duration;
    GenTime m_startPos;
    GenTime m_endPos;

signals:
    void addSubtitle(const QString &);
    void cutSubtitle(int id, int cursorPos);
};

#endif
