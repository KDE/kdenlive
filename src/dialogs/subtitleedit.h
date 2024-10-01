/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_editsub_ui.h"

class SubtitleModel;
class TimecodeDisplay;

class SimpleEditorEventFilter : public QObject
{
    Q_OBJECT
public:
    explicit SimpleEditorEventFilter(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
Q_SIGNALS:
    void singleKeyPress(QKeyEvent *event);
    void inputMethod(QInputMethodEvent *event);
    void shortCut(QKeyEvent *event);
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

public Q_SLOTS:
    void setActiveSubtitle(int id);

private Q_SLOTS:
    void updateSubtitle();
    void goToPrevious();
    void goToNext();
    void slotZoomIn();
    void slotZoomOut();
    /** @brief Sync simpleSubText with subText */
    void syncSimpleText();
    void slotToggleBold();
    void slotToggleItalic();
    void slotToggleUnderline();
    void slotToggleStrikeOut();
    void slotSelectFont();
    void slotResetStyle();
    void slotSetPosition();

private:
    std::shared_ptr<SubtitleModel> m_model;
    int m_activeSub{-1};
    int m_layer;
    bool m_isSimpleEdit{false};
    /** @brief A list as the pos, original length and simplified length of each
     *  override block and escape sequences. This is used to sync the cursor position
     */
    std::vector<std::pair<int, std::pair<int, int>>> m_offsets;
    GenTime m_startPos;
    GenTime m_endPos;

    void updateCharInfo();
    void applyFontSize();
    void updateEffects();
    void updateOffset();

Q_SIGNALS:
    void addSubtitle(const QString &);
    void cutSubtitle(int id, int cursorPos);
    void showSubtitleManager(int page);
};
