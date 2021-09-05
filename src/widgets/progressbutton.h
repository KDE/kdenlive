/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef PROGRESSBUTTON_H
#define PROGRESSBUTTON_H

#include <QElapsedTimer>
#include <QStyleOptionToolButton>
#include <QToolButton>

class QAction;

/** @class ProgressButton
    @brief A Toolbar button with a small progress bar.
 */
class ProgressButton : public QToolButton
{
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_OBJECT
public:
    explicit ProgressButton(const QString &text, double max = 100, QWidget *parent = nullptr);
    ~ProgressButton() override;
    int progress() const;
    void setProgress(int);
    void defineDefaultAction(QAction *action, QAction *actionInProgress);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QAction *m_defaultAction;
    int m_max;
    int m_progress;
    QElapsedTimer m_timer;
    QString m_remainingTime;
    int m_iconSize;
    QFont m_progressFont;
    QStyleOptionToolButton m_buttonStyle;
    /** @brief While rendering, replace real action by a fake on so that rendering is not triggered when clicking again. */
    QAction *m_dummyAction;

signals:
    void progressChanged();
};

#endif
