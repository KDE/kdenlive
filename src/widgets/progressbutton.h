/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PROGRESSBUTTON_H
#define PROGRESSBUTTON_H

#include <QElapsedTimer>
#include <QStyleOptionToolButton>
#include <QToolButton>

class QAction;

/**
 * @class ProgressButton
 * @brief A Toolbar button with a small progress bar.
 *
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
