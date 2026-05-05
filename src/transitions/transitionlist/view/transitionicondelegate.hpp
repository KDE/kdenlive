/*
    SPDX-FileCopyrightText: 2024 KDE Community
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QHash>
#include <QMovie>
#include <QPixmap>
#include <QStyledItemDelegate>

/**
 * @class TransitionIconDelegate
 * @brief A delegate for rendering animated transition previews in a QListView
 */
class TransitionIconDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TransitionIconDelegate(const QSize &iconSize, QObject *parent = nullptr);
    ~TransitionIconDelegate() override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief Get a movie for a transition
     * @param transitionId The ID of the transition
     * @return A pointer to the QMovie, or nullptr if not found
     */
    QMovie *getMovie(QString transitionId, bool animate = false) const;

private:
    mutable std::unique_ptr<QMovie> m_movie;
    mutable std::unique_ptr<QMovie> m_animatedMovie;
    mutable QString m_currentTransitionId;
    QPixmap m_defaultPixmap;
    QSize m_iconSize;
};
