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
     * @brief Set the directory where transition preview GIFs are stored
     * @param path The path to the directory
     */
    void setPreviewDirectory(const QString &path);

    /**
     * @brief Get a movie for a transition
     * @param transitionId The ID of the transition
     * @return A pointer to the QMovie, or nullptr if not found
     */
    QMovie *getMovie(QString transitionId) const;

private:
    QString m_previewDirectory;
    mutable std::unique_ptr<QMovie> m_movie;
    mutable QString m_currentTransitionId;
    QPixmap m_defaultPixmap;
    QSize m_iconSize;
};
