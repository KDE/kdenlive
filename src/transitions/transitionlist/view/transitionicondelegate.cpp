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
#include <QListView>
#include <QMovie>
#include <QPainter>
#include <QStandardPaths>

TransitionIconDelegate::TransitionIconDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_previewDirectory(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/transitions/previews"))
    , m_iconSize(320, 180)
{
    // Create default pixmap
    m_defaultPixmap = QPixmap(m_iconSize);
    m_defaultPixmap.fill(Qt::lightGray);
    QPainter p(&m_defaultPixmap);
    p.setPen(Qt::darkGray);
    p.drawText(m_defaultPixmap.rect(), Qt::AlignCenter, i18n("No Preview"));

    // Ensure the preview directory exists
    QDir().mkpath(m_previewDirectory);
    qDebug() << "TransitionIconDelegate initialized with preview directory:" << m_previewDirectory;
}

TransitionIconDelegate::~TransitionIconDelegate()
{
    // Clean up movies
    qDeleteAll(m_movies);
}

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

    // Get movie or static preview
    QMovie *movie = getMovie(transitionId);
    if (movie) {
        // Draw the current frame of the movie
        QPixmap currentFrame = movie->currentPixmap().scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Center the frame in the icon rect
        QRect frameRect = QRect(iconRect.left() + (iconRect.width() - currentFrame.width()) / 2,
                                iconRect.top() + (iconRect.height() - currentFrame.height()) / 2, currentFrame.width(), currentFrame.height());

        painter->drawPixmap(frameRect, currentFrame);
    } else {
        // Draw static preview or default pixmap
        QPixmap preview = getStaticPreview(transitionId);
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
    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, name);

    painter->restore();
}

QSize TransitionIconDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);

    // Calculate item width based on font metrics
    int width = qMax(m_iconSize.width() / 2, option.rect.width());

    // Height includes icon height + text height + padding
    int textHeight = option.fontMetrics.height() * 2;                                // Allow for 2 lines of text
    int height = m_iconSize.height() * width / m_iconSize.width() + textHeight + 10; // 10px padding

    return QSize(width, height);
}

void TransitionIconDelegate::setPreviewDirectory(const QString &path)
{
    if (m_previewDirectory != path) {
        // Clean up existing movies
        qDeleteAll(m_movies);
        m_movies.clear();
        m_staticPreviews.clear();

        m_previewDirectory = path;
    }
}

QMovie *TransitionIconDelegate::getMovie(const QString &transitionId) const
{
    // Check if we already have this movie
    if (m_movies.contains(transitionId)) {
        return m_movies[transitionId];
    }

    // Try to load the movie
    QString filePath = QDir(m_previewDirectory).filePath(transitionId + QStringLiteral(".gif"));
    qDebug() << "Looking for transition preview at:" << filePath;

    if (!QFileInfo::exists(filePath)) {
        qDebug() << "No preview found for transition:" << transitionId;
        return nullptr;
    }

    qDebug() << "Found preview for transition:" << transitionId;
    QMovie *movie = new QMovie(filePath);
    movie->setCacheMode(QMovie::CacheAll);
    movie->start();

    // Connect to frameChanged to trigger repaint
    connect(movie, &QMovie::frameChanged, [this, movie, transitionId]() {
        // Find all list views using this delegate
        for (QWidget *widget : QApplication::allWidgets()) {
            QListView *listView = qobject_cast<QListView *>(widget);
            if (listView && listView->itemDelegate() == this) {
                // Force update of the view
                QAbstractItemModel *model = listView->model();
                if (model) {
                    for (int row = 0; row < model->rowCount(); row++) {
                        QModelIndex idx = model->index(row, 0);
                        QString id = idx.data(AssetTreeModel::IdRole).toString();
                        if (id == transitionId) {
                            listView->update(idx);
                            break;
                        }
                    }
                }
            }
        }
    });

    m_movies[transitionId] = movie;
    return movie;
}

QPixmap TransitionIconDelegate::getStaticPreview(const QString &transitionId) const
{
    // Check if we already have this preview
    if (m_staticPreviews.contains(transitionId)) {
        return m_staticPreviews[transitionId];
    }

    // Try to load a static preview
    QString filePath = QDir(m_previewDirectory).filePath(transitionId + QStringLiteral(".png"));
    if (QFileInfo::exists(filePath)) {
        QPixmap pixmap(filePath);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(m_iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_staticPreviews[transitionId] = pixmap;
            return pixmap;
        }
    }

    // Use default pixmap
    m_staticPreviews[transitionId] = m_defaultPixmap;
    return m_defaultPixmap;
}