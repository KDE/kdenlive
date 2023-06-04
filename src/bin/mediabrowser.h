/*
SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KDirOperator>
#include <QTimer>

class KUrlNavigator;
class KUrlComboBox;
class KFileFilterCombo;

/**
 * @class MediaBrowser
 * @brief The bin widget takes care of both item model and view upon project opening.
 */
class MediaBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit MediaBrowser(QWidget *parent = nullptr);
    ~MediaBrowser() override;
    const QUrl url() const;
    void setUrl(const QUrl url);
    void back();
    void forward();
    /** @brief create a map of all actions having shortcut conflicts between Kdenlive's actioncollection and the KDirOperator */

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    KDirOperator *m_op;
    KUrlNavigator *m_locationEdit;
    KFileFilterCombo *m_filterCombo;
    KUrlComboBox *m_filenameEdit;
    QTimer m_filterDelayTimer;
    QList<QAction *> m_browserActions;
    QList<QAction *> m_conflictingAppActions;
    void importSelection();
    void openExternalFile(const QUrl &url);
    void enableAppShortcuts();
    void disableAppShortcuts();
public Q_SLOTS:
    void detectShortcutConflicts();

private Q_SLOTS:
    void slotFilterChanged();
    void slotLocationChanged(const QString &text);
    void slotUrlEntered(const QUrl &url);
    void connectView();
    void slotViewDoubleClicked();
};
