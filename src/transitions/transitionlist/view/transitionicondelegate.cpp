/*
    SPDX-FileCopyrightText: 2025 KDE Community
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transitionicondelegate.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "transitions/transitionsrepository.hpp"

#include <KLocalizedString>
#include <QAbstractItemView>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QListView>
#include <QMovie>
#include <QPainter>
#include <QStandardPaths>

TransitionIconDelegate::TransitionIconDelegate(const QSize &iconSize, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_iconSize(iconSize)
{
    // Create default pixmap
    m_defaultPixmap = QPixmap(m_iconSize);
    m_defaultPixmap.fill(Qt::lightGray);
    QPainter p(&m_defaultPixmap);
    p.setPen(Qt::darkGray);
    p.drawText(m_defaultPixmap.rect(), Qt::AlignCenter, i18n("No Preview"));

    // Ensure the preview directory exists
    QDir previewDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/transitions/previews"));
    previewDir.mkpath(".");
    qDebug() << "TransitionIconDelegate initialized with preview directory:" << previewDir.absolutePath();
}

TransitionIconDelegate::~TransitionIconDelegate() {}

void TransitionIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return;
    }

    // Get transition ID
    QString transitionId = index.data(AssetTreeModel::IdRole).toString();
    if (transitionId.isEmpty() || transitionId == QLatin1String("root")) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    // Luma transition id is the full path to luma, adjust
    auto type = index.data(AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
    if (type == AssetListType::AssetType::LumaTransition) {
        transitionId = QFileInfo(transitionId).baseName();
    }

    // Prepare to draw
    painter->save();

    // Draw selection background if selected
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, option.palette.alternateBase());
    }

    // Calculate icon rect (centered in the top part of the item)
    QRect iconRect = option.rect;
    iconRect.setHeight(m_iconSize.height() * iconRect.width() / m_iconSize.width());
    iconRect.setWidth(option.rect.width());

    // Calculate text rect (below the icon)
    QRect textRect = option.rect;
    textRect.setTop(iconRect.bottom() + 5);

    // Get movie
    QMovie *movie = getMovie(transitionId);
    if (movie) {
        // Draw the current frame of the movie
        int max = movie->frameCount() / 2;
        if (max == 0) {
            max = 20;
        }
        if (movie->state() == QMovie::NotRunning) {
            movie->start();
            movie->setPaused(true);
            int i = 0;
            while (i < max) {
                movie->jumpToNextFrame();
                i++;
            }
        }
        QPixmap currentFrame = movie->currentPixmap().scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Center the frame in the icon rect
        QRect frameRect = QRect(iconRect.left() + (iconRect.width() - currentFrame.width()) / 2,
                                iconRect.top() + (iconRect.height() - currentFrame.height()) / 2, currentFrame.width(), currentFrame.height());

        painter->drawPixmap(frameRect, currentFrame);
    } else {
        // Draw default pixmap
        QPixmap preview = m_defaultPixmap;
        QRect previewRect = QRect(iconRect.left() + (iconRect.width() - preview.width()) / 2, iconRect.top() + (iconRect.height() - preview.height()) / 2,
                                  preview.width(), preview.height());
        painter->drawPixmap(previewRect, preview);
    }

    // Draw transition name
    const QString name = index.data(Qt::DisplayRole).toString();
    QFont font = option.font;

    // Make favorite items bold
    bool isFavorite = index.data(AssetTreeModel::FavoriteRole).toBool();
    if (isFavorite) {
        font.setBold(true);
    }

    painter->setFont(font);
    painter->setPen(option.state & QStyle::State_Selected ? option.palette.highlightedText().color() : option.palette.text().color());

    // Draw text centered
    const QString elidedName = option.fontMetrics.elidedText(name, Qt::ElideMiddle, textRect.width());
    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedName);

    painter->restore();
}

QSize TransitionIconDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QSize(m_iconSize.width(), m_iconSize.height() + option.fontMetrics.lineSpacing() + 4);
}

QMovie *TransitionIconDelegate::getMovie(QString transitionId, bool animate) const
{
    // Check if we already have this movie
    if (m_currentTransitionId == transitionId && m_animatedMovie) {
        return m_animatedMovie.get();
    }

    // Try to load the movie
    QString filePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("transitions/previews/%1.webp").arg(transitionId));

    if (filePath.isEmpty()) {
        // Custom user generated transition preview are gif since webp not widely available in our binaries
        filePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("transitions/previews/%1.gif").arg(transitionId));
        if (filePath.isEmpty()) {
            qDebug() << "No preview found for transition:" << transitionId;
            return nullptr;
        }
    }
    if (animate) {
        m_currentTransitionId = transitionId;
        m_animatedMovie.reset(new QMovie(filePath));
        return m_animatedMovie.get();
    }
    m_movie.reset(new QMovie(filePath));
    return m_movie.get();
}
