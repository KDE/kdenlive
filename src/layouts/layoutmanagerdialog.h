/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "layouts/layoutcollection.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

class LayoutManagerDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief LayoutManagerDialog always works on a copy of LayoutCollection for preview. Changes are only applied if accepted.
     * @param layouts The collection to preview and edit.
     * @param currentLayoutId The currently selected layout.
     */
    LayoutManagerDialog(QWidget *parent, LayoutCollection &layouts, const QString &currentLayoutId);
    void updateOrder();
    QString getSelectedLayoutId() const;

private:
    void setupUi(const QString &currentLayoutId);
    void addLayoutItem(const LayoutInfo &layout);
    void updateButtonStates();

    void deleteCurrentItem();
    void moveItemUp();
    void moveItemDown();
    void resetToDefaults();
    void importLayout();
    void exportLayout();

    QListWidget *m_listWidget;
    QDialogButtonBox *m_buttonBox;
    QToolButton *m_deleteButton;
    QToolButton *m_upButton;
    QToolButton *m_downButton;
    QToolButton *m_resetButton;
    QToolButton *m_importButton;
    QToolButton *m_exportButton;
    LayoutCollection m_previewCollection;
    QIcon m_horizontalProfile;
    QIcon m_horizontalAndVerticalProfile;
    QIcon m_verticalProfile;
};
