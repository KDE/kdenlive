/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "bin.h"
#include "bincommands.h"
#include "clipcreator.hpp"
#include "core.h"
#include "dialogs/clipcreationdialog.h"
#include "dialogs/clipjobmanager.h"
#include "dialogs/kdenlivesettingsdialog.h"
#include "dialogs/textbasededit.h"
#include "dialogs/timeremap.h"
#include "doc/documentchecker.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "glaxnimatelauncher.h"
#include "jobs/abstracttask.h"
#include "jobs/audiolevelstask.h"
#include "jobs/cliploadtask.h"
#include "jobs/taskmanager.h"
#include "jobs/transcodetask.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"
#include "mediabrowser.h"
#include "mlt++/Mlt.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/clippropertiescontroller.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "profiles/profilemodel.hpp"
#include "project/dialogs/guideslist.h"
#include "project/dialogs/slideshowclip.h"
#include "project/invaliddialog.h"
#include "project/projectmanager.h"
#include "project/transcodeseek.h"
#include "projectclip.h"
#include "projectfolder.h"
#include "projectitemmodel.h"
#include "projectsortproxymodel.h"
#include "projectsubclip.h"
#include "tagwidget.hpp"
#include "titler/titlewidget.h"
#include "ui_newtimeline_ui.h"
#include "ui_qtextclip_ui.h"
#include "undohelper.hpp"
#include "utils/thumbnailcache.hpp"
#include "xml/xml.hpp"

#include "utils/KMessageBox_KdenliveCompat.h"
#include <KActionMenu>
#include <KColorScheme>
#include <KIO/ApplicationLauncherJob>
#include <KIO/FileCopyJob>
#include <KIO/OpenFileManagerWindowJob>
#include <KIconEffect>
#include <KIconTheme>
#include <KMessageBox>
#include <KOpenWithDialog>
#include <KRatingPainter>
#include <KService>
#include <KUrlRequesterDialog>
#include <KXMLGUIFactory>
#include <kwidgetsaddons_version.h>

#include <QCryptographicHash>
#include <QDrag>
#include <QFile>
#include <QMenu>
#include <QMimeData>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QTimeLine>
#include <QToolBar>
#include <QUndoCommand>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>
#include <utility>

static QImage m_videoIcon;
static QImage m_audioIcon;
static QImage m_audioUsedIcon;
static QImage m_videoUsedIcon;
static QSize m_iconSize;
static QIcon m_folderIcon;
static QIcon m_sequenceFolderIcon;

/**
 * @class BinItemDelegate
 * @brief This class is responsible for drawing items in the QTreeView.
 */
class BinItemDelegate : public QStyledItemDelegate
{
public:
    explicit BinItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)

    {
        connect(this, &QStyledItemDelegate::closeEditor, [&]() { m_editorOpen = false; });
    }
    void setEditorData(QWidget *w, const QModelIndex &i) const override
    {
        if (!m_editorOpen) {
            QStyledItemDelegate::setEditorData(w, i);
            m_editorOpen = true;
        }
    }
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (index.column() == 0) {
                QPoint pos = me->pos();
                if (m_audioDragRect.contains(pos)) {
                    dragType = PlaylistState::AudioOnly;
                } else if (m_videoDragRect.contains(pos)) {
                    dragType = PlaylistState::VideoOnly;
                } else {
                    dragType = PlaylistState::Disabled;
                }
            } else {
                dragType = PlaylistState::Disabled;
                if (index.column() == 7) {
                    // Rating
                    QRect rect = option.rect;
                    rect.adjust(option.rect.width() / 12, 0, 0, 0);
                    int rate = 0;
                    if (me->pos().x() > rect.x()) {
                        rate = KRatingPainter::getRatingFromPosition(rect, Qt::AlignLeft | Qt::AlignVCenter, qApp->layoutDirection(), me->pos());
                    }
                    if (rate > -1) {
                        // Full star rating only
                        if (rate % 2 == 1) {
                            rate++;
                        }
                        Q_EMIT static_cast<ProjectSortProxyModel *>(model)->updateRating(index, uint(rate));
                    }
                }
            }
        }
        event->ignore();
        return false;
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() != 0) {
            QStyledItemDelegate::updateEditorGeometry(editor, option, index);
            return;
        }
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QRect r1 = option.rect;
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        int decoWidth = 0;
        if (opt.decorationSize.height() > 0) {
            decoWidth += int(r1.height() * pCore->getCurrentDar());
        }
        int mid = 0;
        if (type == AbstractProjectItem::ClipItem || type == AbstractProjectItem::SubClipItem) {
            mid = int((r1.height() / 2));
        }
        r1.adjust(decoWidth, 0, 0, -mid);
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QRect r2 = fm.boundingRect(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString()).toRect();
        editor->setGeometry(r2);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        QString text = index.data(AbstractProjectItem::DataName).toString();
        QRectF r = option.rect;
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        int width = int(fm.boundingRect(r, Qt::AlignLeft | Qt::AlignTop, text).width() + option.decorationSize.width()) + 2 * textMargin;
        hint.setWidth(width);
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        if (type == AbstractProjectItem::FolderItem) {
            return QSize(hint.width(), qMin(option.fontMetrics.lineSpacing() + 4, hint.height()));
        }
        if (type == AbstractProjectItem::ClipItem) {
            return QSize(hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, qMax(hint.height(), option.decorationSize.height())));
        }
        if (type == AbstractProjectItem::SubClipItem) {
            return QSize(hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, qMin(hint.height(), int(option.decorationSize.height() / 1.5))));
        }
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QString line1 = index.data(Qt::DisplayRole).toString();
        QString line2 = index.data(Qt::UserRole).toString();

        int textW = qMax(option.fontMetrics.horizontalAdvance(line1), option.fontMetrics.horizontalAdvance(line2));
        QSize iconSize = icon.actualSize(option.decorationSize);
        return {qMax(textW, iconSize.width()) + 4, option.fontMetrics.lineSpacing() * 2 + 4};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() == 0 && !index.data().isNull()) {
            QRect r1 = option.rect;
            painter->save();
            painter->setClipRect(r1);
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            // QRect r = QStyle::alignedRect(opt.direction, Qt::AlignVCenter | Qt::AlignLeft, opt.decorationSize, r1);
            // Draw alternate background
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            if ((option.state & static_cast<int>(QStyle::State_Selected)) != 0) {
                painter->setPen(option.palette.highlightedText().color());
            } else {
                painter->setPen(option.palette.text().color());
            }
            QRect r = r1;
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            if (type == AbstractProjectItem::ClipItem || type == AbstractProjectItem::SubClipItem) {
                int decoWidth = 0;
                FileStatus::ClipStatus clipStatus = FileStatus::ClipStatus(index.data(AbstractProjectItem::ClipStatus).toInt());
                int cType = index.data(AbstractProjectItem::ClipType).toInt();
                if (opt.decorationSize.height() > 0) {
                    r.setWidth(int(r.height() * pCore->getCurrentDar()));
                    QPixmap pix = opt.icon.pixmap(opt.icon.actualSize(r.size()));
                    if (!pix.isNull()) {
                        // Draw icon
                        decoWidth += r.width() + textMargin;
                        r.setWidth(r.height() * pix.width() / pix.height());
                        painter->drawPixmap(r, pix, QRect(0, 0, pix.width(), pix.height()));
                    }
                    m_thumbRect = r;

                    // Draw frame in case of missing source
                    if (clipStatus == FileStatus::StatusMissing || clipStatus == FileStatus::StatusProxyOnly) {
                        painter->save();
                        painter->setPen(QPen(clipStatus == FileStatus::StatusProxyOnly ? Qt::yellow : Qt::red, 3));
                        painter->drawRect(m_thumbRect.adjusted(0, 0, -1, -1));
                        painter->restore();
                    } else if (cType == ClipType::Image || cType == ClipType::SlideShow) {
                        // Draw 'photo' frame to identify image clips
                        painter->save();
                        int penWidth = m_thumbRect.height() / 14;
                        penWidth += penWidth % 2;
                        painter->setPen(QPen(QColor(255, 255, 255, 160), penWidth));
                        penWidth /= 2;
                        painter->drawRoundedRect(m_thumbRect.adjusted(penWidth, penWidth, -penWidth - 1, -penWidth - 1), 4, 4);
                        painter->setPen(QPen(Qt::black, 1));
                        painter->drawRoundedRect(m_thumbRect.adjusted(0, 0, -1, -1), 4, 4);
                        painter->restore();
                    }
                }
                int mid = int((r1.height() / 2));
                r1.adjust(decoWidth, 0, 0, -mid);
                QRect r2 = option.rect;
                r2.adjust(decoWidth, mid, 0, 0);
                QRectF bounding;
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString(), &bounding);
                font.setBold(false);
                painter->setFont(font);
                QString subText = index.data(AbstractProjectItem::DataDuration).toString();
                QString tags = index.data(AbstractProjectItem::DataTag).toString();
                if (!tags.isEmpty()) {
                    QStringList t = tags.split(QLatin1Char(';'));
                    QRectF tagRect = m_thumbRect.adjusted(2, 2, 0, 2);
                    tagRect.setWidth(r1.height() / 3.5);
                    tagRect.setHeight(tagRect.width());
                    for (const QString &color : qAsConst(t)) {
                        painter->setBrush(QColor(color));
                        painter->drawRoundedRect(tagRect, tagRect.height() / 2, tagRect.height() / 2);
                        tagRect.moveTop(tagRect.bottom() + tagRect.height() / 4);
                    }
                    painter->setBrush(Qt::NoBrush);
                }
                if (!subText.isEmpty()) {
                    r2.adjust(0, int(bounding.bottom() - r2.top()), 0, 0);
                    QColor subTextColor = painter->pen().color();
                    bool selected = opt.state & QStyle::State_Selected;
                    if (!selected) {
                        subTextColor.setAlphaF(.7);
                    }
                    painter->setPen(subTextColor);
                    // Draw usage counter
                    const QString usage = index.data(AbstractProjectItem::UsageCount).toString();
                    if (!usage.isEmpty()) {
                        subText.append(QString(" [%1]").arg(usage));
                    }
                    painter->drawText(r2, Qt::AlignLeft | Qt::AlignTop, subText, &bounding);
                    // Add audio/video icons for selective drag
                    bool hasAudioAndVideo = index.data(AbstractProjectItem::ClipHasAudioAndVideo).toBool();
                    if (hasAudioAndVideo && (cType == ClipType::AV || cType == ClipType::Playlist || cType == ClipType::Timeline)) {
                        QRect audioRect(0, 0, m_audioIcon.width(), m_audioIcon.height());
                        audioRect.moveTop(bounding.top() + 1);
                        QRect videoIconRect = audioRect;
                        videoIconRect.moveRight(
                            qMax(int(bounding.right() + (2 * textMargin) + 2 * (1 + m_audioIcon.width())), option.rect.right() - (2 * textMargin)));
                        audioRect.moveRight(videoIconRect.left() - (2 * textMargin) - 1);
                        if (opt.state & QStyle::State_MouseOver) {
                            m_audioDragRect = audioRect.adjusted(-1, -1, 1, 1);
                            m_videoDragRect = videoIconRect.adjusted(-1, -1, 1, 1);
                            painter->drawImage(audioRect.topLeft(), m_audioIcon);
                            painter->drawImage(videoIconRect.topLeft(), m_videoIcon);
                            painter->setPen(opt.palette.highlight().color());
                            painter->drawRect(m_audioDragRect);
                            painter->drawRect(m_videoDragRect);
                        } else if (!usage.isEmpty()) {
                            if (index.data(AbstractProjectItem::AudioUsed).toBool()) {
                                painter->drawImage(audioRect.topLeft(), selected ? m_audioIcon : m_audioUsedIcon);
                            }
                            if (index.data(AbstractProjectItem::VideoUsed).toBool()) {
                                painter->drawImage(videoIconRect.topLeft(), selected ? m_videoIcon : m_videoUsedIcon);
                            }
                        }
                    } else if (!usage.isEmpty()) {
                        QRect iconRect(0, 0, m_audioIcon.width(), m_audioIcon.height());
                        iconRect.moveTop(bounding.top() + 1);
                        int minPos = bounding.right() + (2 * textMargin) + m_audioIcon.width();
                        iconRect.moveRight(qMax(minPos, option.rect.right() - (2 * textMargin)));
                        if (index.data(AbstractProjectItem::AudioUsed).toBool()) {
                            painter->drawImage(iconRect.topLeft(), selected ? m_audioIcon : m_audioUsedIcon);
                        } else {
                            painter->drawImage(iconRect.topLeft(), selected ? m_videoIcon : m_videoUsedIcon);
                        }
                    }
                }
                if (type == AbstractProjectItem::ClipItem) {
                    // Overlay icon if necessary
                    QVariant v = index.data(AbstractProjectItem::IconOverlay);
                    if (!v.isNull()) {
                        QIcon reload = QIcon::fromTheme(v.toString());
                        int size = style->pixelMetric(QStyle::PM_SmallIconSize);
                        reload.paint(painter, QRect(r.left() + 2, r.bottom() - size - 2, size, size));
                    }
                    int jobProgress = index.data(AbstractProjectItem::JobProgress).toInt();
                    auto status = index.data(AbstractProjectItem::JobStatus).value<TaskManagerStatus>();
                    if (jobProgress < 100 && (status == TaskManagerStatus::Pending || status == TaskManagerStatus::Running)) {
                        // Draw job progress bar
                        int progressWidth = option.fontMetrics.averageCharWidth() * 8;
                        int progressHeight = option.fontMetrics.ascent() / 4;
                        QRect progress(r1.x() + 1, opt.rect.bottom() - progressHeight - 2, progressWidth, progressHeight);
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(Qt::darkGray);
                        if (status == TaskManagerStatus::Running) {
                            painter->drawRoundedRect(progress, 2, 2);
                            painter->setBrush((option.state & static_cast<int>((QStyle::State_Selected) != 0)) != 0 ? option.palette.text()
                                                                                                                    : option.palette.highlight());
                            progress.setWidth((progressWidth - 2) * jobProgress / 100);
                            painter->drawRoundedRect(progress, 2, 2);
                        } else {
                            // Draw kind of a pause icon
                            progress.setWidth(3);
                            painter->drawRect(progress);
                            progress.moveLeft(progress.right() + 3);
                            painter->drawRect(progress);
                        }
                    }
                    bool jobsucceeded = index.data(AbstractProjectItem::JobSuccess).toBool();
                    if (!jobsucceeded) {
                        QIcon warning = QIcon::fromTheme(QStringLiteral("process-stop"));
                        warning.paint(painter, r2);
                    }
                }
            } else {
                // Folder
                int decoWidth = 0;
                if (opt.decorationSize.height() > 0) {
                    QIcon icon = index.data(AbstractProjectItem::SequenceFolder).toBool() ? m_sequenceFolderIcon : m_folderIcon;
                    r.setWidth(int(r.height() * pCore->getCurrentDar()));
                    QPixmap pix = icon.pixmap(icon.actualSize(r.size()));
                    // Draw icon
                    decoWidth += r.width() + textMargin;
                    r.setWidth(r.height() * pix.width() / pix.height());
                    painter->drawPixmap(r, pix, QRect(0, 0, pix.width(), pix.height()));
                }
                r1.adjust(decoWidth, 0, 0, 0);
                QRectF bounding;
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString(), &bounding);
            }
            painter->restore();
        } else if (index.column() == 7) {
            // Rating
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            QRect r1 = opt.rect;
            // Tweak bg opacity since breeze dark star has same color as highlighted background
            painter->setOpacity(0.5);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            painter->setOpacity(1);
            if (index.data(AbstractProjectItem::ItemTypeRole).toInt() != AbstractProjectItem::FolderItem) {
                r1.adjust(r1.width() / 12, 0, 0, 0);
                KRatingPainter::paintRating(painter, r1, Qt::AlignLeft | Qt::AlignVCenter, index.data().toInt());
            }
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }

    int getFrame(const QModelIndex &index, int mouseX)
    {
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        if ((type != AbstractProjectItem::ClipItem && type != AbstractProjectItem::SubClipItem)) {
            return 0;
        }
        if (mouseX < m_thumbRect.x() || mouseX > m_thumbRect.right()) {
            return -1;
        }
        return 100 * (mouseX - m_thumbRect.x()) / m_thumbRect.width();
    }

private:
    mutable bool m_editorOpen{false};
    mutable QRect m_audioDragRect;
    mutable QRect m_videoDragRect;
    mutable QRect m_thumbRect;

public:
    PlaylistState::ClipState dragType{PlaylistState::Disabled};
};

/**
 * @class BinListItemDelegate
 * @brief This class is responsible for drawing items in the QListView (Icon view).
 */

class BinListItemDelegate : public QStyledItemDelegate
{
public:
    explicit BinListItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)

    {
        connect(this, &QStyledItemDelegate::closeEditor, [&]() { m_editorOpen = false; });
    }
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        Q_UNUSED(model);
        Q_UNUSED(option);
        Q_UNUSED(index);
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_audioDragRect.contains(me->pos())) {
                dragType = PlaylistState::AudioOnly;
            } else if (m_videoDragRect.contains(me->pos())) {
                dragType = PlaylistState::VideoOnly;
            } else {
                dragType = PlaylistState::Disabled;
            }
        }
        event->ignore();
        return false;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (!index.data().isNull()) {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            // QStyledItemDelegate::paint(painter, opt, index);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            // style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
            QRect r = opt.rect;
            r.setHeight(r.width() / pCore->getCurrentDar());
            int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
            bool isFolder = type == AbstractProjectItem::FolderItem;
            bool isSequenceFolder = index.data(AbstractProjectItem::SequenceFolder).toBool();
            QPixmap pix = isFolder ? (isSequenceFolder ? m_sequenceFolderIcon.pixmap(m_sequenceFolderIcon.actualSize(r.size()))
                                                       : m_folderIcon.pixmap(m_folderIcon.actualSize(r.size())))
                                   : opt.icon.pixmap(opt.icon.actualSize(r.size()));
            if (!pix.isNull()) {
                // Draw icon
                painter->drawPixmap(r, pix, QRect(0, 0, pix.width(), pix.height()));
            }
            m_thumbRect = r;
            QRect textRect = opt.rect;
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            textRect.adjust(textMargin, 0, -textMargin, -textMargin);
            QRectF bounding;
            QString itemText = index.data(AbstractProjectItem::DataName).toString();
            // Draw usage counter
            const QString usage = isFolder ? QString() : index.data(AbstractProjectItem::UsageCount).toString();
            if (!usage.isEmpty()) {
                int usageWidth = option.fontMetrics.horizontalAdvance(QString(" [%1]").arg(usage));
                int availableWidth = textRect.width() - usageWidth;
                if (option.fontMetrics.horizontalAdvance(itemText) > availableWidth) {
                    itemText = option.fontMetrics.elidedText(itemText, Qt::ElideRight, availableWidth);
                }
                itemText.append(QString(" [%1]").arg(usage));
            } else {
                if (option.fontMetrics.horizontalAdvance(itemText) > textRect.width()) {
                    itemText = option.fontMetrics.elidedText(itemText, Qt::ElideRight, textRect.width());
                }
            }
            painter->drawText(textRect, Qt::AlignCenter | Qt::AlignBottom, itemText, &bounding);

            if (isFolder) {
                return;
            }

            // Tags
            QString tags = index.data(AbstractProjectItem::DataTag).toString();
            if (!tags.isEmpty()) {
                QStringList t = tags.split(QLatin1Char(';'));
                QRectF tagRect = m_thumbRect.adjusted(2, 2, 0, 2);
                tagRect.setWidth(m_thumbRect.height() / 5);
                tagRect.setHeight(tagRect.width());
                painter->save();
                for (const QString &color : qAsConst(t)) {
                    painter->setBrush(QColor(color));
                    painter->drawRoundedRect(tagRect, tagRect.height() / 2, tagRect.height() / 2);
                    tagRect.moveTop(tagRect.bottom() + tagRect.height() / 4);
                }
                painter->restore();
            }

            // Add audio/video icons for selective drag
            int cType = index.data(AbstractProjectItem::ClipType).toInt();
            bool hasAudioAndVideo = index.data(AbstractProjectItem::ClipHasAudioAndVideo).toBool();
            if (hasAudioAndVideo && (cType == ClipType::AV || cType == ClipType::Playlist || cType == ClipType::Timeline) &&
                m_thumbRect.height() > 2.5 * m_audioIcon.height()) {
                QRect thumbRect = m_thumbRect;
                thumbRect.setLeft(opt.rect.right() - m_audioIcon.width() - 6);
                if (opt.state & QStyle::State_MouseOver || !usage.isEmpty()) {
                    QColor bgColor = option.palette.window().color();
                    bgColor.setAlphaF(.7);
                    painter->fillRect(thumbRect, bgColor);
                }
                thumbRect.setSize(m_audioIcon.size());
                thumbRect.translate(3, 2);
                QRect videoThumbRect = thumbRect;
                videoThumbRect.moveTop(thumbRect.bottom() + 2);
                if (opt.state & QStyle::State_MouseOver) {
                    m_audioDragRect = thumbRect;
                    m_videoDragRect = videoThumbRect;
                    painter->drawImage(m_audioDragRect.topLeft(), m_audioIcon);
                    painter->drawImage(m_videoDragRect.topLeft(), m_videoIcon);
                } else if (!usage.isEmpty()) {
                    if (index.data(AbstractProjectItem::AudioUsed).toBool()) {
                        painter->drawImage(thumbRect.topLeft(), m_audioUsedIcon);
                    }
                    if (index.data(AbstractProjectItem::VideoUsed).toBool()) {
                        painter->drawImage(videoThumbRect.topLeft(), m_videoUsedIcon);
                    }
                }
            }
            // Draw frame in case of missing source
            FileStatus::ClipStatus clipStatus = FileStatus::ClipStatus(index.data(AbstractProjectItem::ClipStatus).toInt());
            if (clipStatus == FileStatus::StatusMissing || clipStatus == FileStatus::StatusProxyOnly) {
                painter->save();
                painter->setPen(QPen(clipStatus == FileStatus::StatusProxyOnly ? Qt::yellow : Qt::red, 3));
                painter->drawRect(m_thumbRect);
                painter->restore();
            } else if (cType == ClipType::Image || cType == ClipType::SlideShow) {
                // Draw 'photo' frame to identify image clips
                painter->save();
                int penWidth = m_thumbRect.height() / 14;
                penWidth += penWidth % 2;
                painter->setPen(QPen(QColor(255, 255, 255, 160), penWidth));
                penWidth /= 2;
                painter->drawRoundedRect(m_thumbRect.adjusted(penWidth, penWidth, -penWidth - 1, -penWidth + 1), 4, 4);
                painter->setPen(QPen(Qt::black, 1));
                painter->drawRoundedRect(m_thumbRect.adjusted(0, 0, -1, 1), 4, 4);
                painter->restore();
            }
            // Overlay icon if necessary
            QVariant v = index.data(AbstractProjectItem::IconOverlay);
            if (!v.isNull()) {
                QRect r = m_thumbRect;
                QIcon reload = QIcon::fromTheme(v.toString());
                r.setTop(r.bottom() - (opt.rect.height() - r.height()));
                r.setWidth(r.height());
                reload.paint(painter, r);
            }
            int jobProgress = index.data(AbstractProjectItem::JobProgress).toInt();
            auto status = index.data(AbstractProjectItem::JobStatus).value<TaskManagerStatus>();
            if (jobProgress < 100 && (status == TaskManagerStatus::Pending || status == TaskManagerStatus::Running)) {
                // Draw job progress bar
                int progressHeight = option.fontMetrics.ascent() / 4;
                QRect thumbRect = m_thumbRect.adjusted(2, 2, -2, -2);
                QRect progress(thumbRect.x(), thumbRect.bottom() - progressHeight - 2, thumbRect.width(), progressHeight);
                painter->setPen(Qt::NoPen);
                painter->setBrush(Qt::darkGray);
                if (status == TaskManagerStatus::Running) {
                    painter->drawRoundedRect(progress, 2, 2);
                    painter->setBrush((option.state & static_cast<int>((QStyle::State_Selected) != 0)) != 0 ? option.palette.text()
                                                                                                            : option.palette.highlight());
                    progress.setWidth((thumbRect.width() - 2) * jobProgress / 100);
                    painter->drawRoundedRect(progress, 2, 2);
                } else {
                    // Draw kind of a pause icon
                    progress.setWidth(3);
                    painter->drawRect(progress);
                    progress.moveLeft(progress.right() + 3);
                    painter->drawRect(progress);
                }
            }
            bool jobsucceeded = index.data(AbstractProjectItem::JobSuccess).toBool();
            if (!jobsucceeded) {
                QIcon warning = QIcon::fromTheme(QStringLiteral("process-stop"));
                QRect thumbRect = m_thumbRect.adjusted(2, 2, 2, 2);
                warning.paint(painter, thumbRect);
            }
        }
    }

    int getFrame(const QModelIndex &index, QPoint pos)
    {
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        if ((type != AbstractProjectItem::ClipItem && type != AbstractProjectItem::SubClipItem) || !m_thumbRect.contains(pos)) {
            return 0;
        }
        return 100 * (pos.x() - m_thumbRect.x()) / m_thumbRect.width();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        int textHeight = int(option.fontMetrics.height() * 1.5);
        return (QSize(m_iconSize.width(), m_iconSize.height() + textHeight));
    }

private:
    mutable bool m_editorOpen{false};
    mutable QRect m_audioDragRect;
    mutable QRect m_videoDragRect;
    mutable QRect m_thumbRect;

public:
    PlaylistState::ClipState dragType{PlaylistState::Disabled};
};

MyListView::MyListView(QWidget *parent)
    : QListView(parent)
{
    setViewMode(QListView::IconMode);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setWordWrap(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setUniformItemSizes(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    // setDropIndicatorShown(true);
    viewport()->setAcceptDrops(true);
}

void MyListView::focusInEvent(QFocusEvent *event)
{
    QListView::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason) {
        Q_EMIT focusView();
    }
}

void MyListView::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("text/producerslist"))) {
        // Internal drag/drop, ensure it is not a zone drop
        if (!QString(event->mimeData()->data(QStringLiteral("text/producerslist"))).contains(QLatin1Char('/'))) {
            bool isSameRoot = false;
            QString rootId = QString(event->mimeData()->data(QStringLiteral("text/rootId")));
            if (rootIndex().data(AbstractProjectItem::DataId).toString() == rootId) {
                isSameRoot = true;
            }
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
            if (isSameRoot && !indexAt(event->position().toPoint()).isValid()) {
#else
            if (isSameRoot && !indexAt(event->pos()).isValid()) {
#endif
                event->ignore();
                return;
            }
        }
    }
    QListView::dropEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void MyListView::enterEvent(QEnterEvent *event)
#else
void MyListView::enterEvent(QEvent *event)
#endif
{
    QListView::enterEvent(event);
    pCore->setWidgetKeyBinding(i18n("<b>Double click</b> to add a file to the project"));
}

void MyListView::leaveEvent(QEvent *event)
{
    QListView::leaveEvent(event);
    pCore->setWidgetKeyBinding();
}

void MyListView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QModelIndex ix = indexAt(event->pos());
        if (ix.isValid()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            QAbstractItemDelegate *del = itemDelegate(ix);
#else
            QAbstractItemDelegate *del = itemDelegateForIndex(ix);
#endif
            m_dragType = static_cast<BinListItemDelegate *>(del)->dragType;
            m_startPos = event->pos();
        } else {
            m_dragType = PlaylistState::Disabled;
            m_startPos = QPoint();
        }
        Q_EMIT updateDragMode(m_dragType);
    }
    QListView::mousePressEvent(event);
}

void MyListView::mouseReleaseEvent(QMouseEvent *event)
{
    m_startPos = QPoint();
    QListView::mouseReleaseEvent(event);
}

void MyListView::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) != 0u) {
        if (!m_startPos.isNull() && (event->pos() - m_startPos).manhattanLength() > QApplication::startDragDistance()) {
            QModelIndexList indexes = selectedIndexes();
            if (indexes.isEmpty()) {
                // Dragging from empty zone, abort
                QListView::mouseMoveEvent(event);
                return;
            }
            auto *drag = new QDrag(this);
            drag->setMimeData(model()->mimeData(indexes));
            QModelIndex ix = indexes.constFirst();
            if (ix.isValid()) {
                QIcon icon = ix.data(AbstractProjectItem::DataThumbnail).value<QIcon>();
                QPixmap pix = icon.pixmap(iconSize());
                QSize size = pix.size() / 2;
                QImage image(size, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter p(&image);
                p.setOpacity(0.7);
                p.drawPixmap(0, 0, image.width(), image.height(), pix);
                p.setOpacity(1);
                if (indexes.count() > 1) {
                    QPalette palette;
                    int radius = size.height() / 3;
                    p.setBrush(palette.highlight());
                    p.setPen(palette.highlightedText().color());
                    p.drawEllipse(QPoint(size.width() / 2, size.height() / 2), radius, radius);
                    p.drawText(size.width() / 2 - radius, size.height() / 2 - radius, 2 * radius, 2 * radius, Qt::AlignCenter,
                               QString::number(indexes.count()));
                }
                p.end();
                drag->setPixmap(QPixmap::fromImage(image));
            }
            drag->exec();
            Q_EMIT processDragEnd();
            return;
        }
        QListView::mouseMoveEvent(event);
        return;
    }
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        if (KdenliveSettings::hoverPreview()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            QAbstractItemDelegate *del = itemDelegate(index);
#else
            QAbstractItemDelegate *del = itemDelegateForIndex(index);
#endif
            if (del) {
                auto delegate = static_cast<BinListItemDelegate *>(del);
                QRect vRect = visualRect(index);
                if (vRect.contains(event->pos())) {
                    if (m_lastHoveredItem != index) {
                        if (m_lastHoveredItem.isValid()) {
                            Q_EMIT displayBinFrame(m_lastHoveredItem, -1);
                        }
                        m_lastHoveredItem = index;
                    }
                    int frame = delegate->getFrame(index, event->pos());
                    Q_EMIT displayBinFrame(index, frame, event->modifiers() & Qt::ShiftModifier);
                } else if (m_lastHoveredItem == index) {
                    Q_EMIT displayBinFrame(m_lastHoveredItem, -1);
                    m_lastHoveredItem = QModelIndex();
                }
            } else {
                if (m_lastHoveredItem.isValid()) {
                    Q_EMIT displayBinFrame(m_lastHoveredItem, -1);
                    m_lastHoveredItem = QModelIndex();
                }
            }
            pCore->bin()->updateKeyBinding(i18n("<b>Shift+seek</b> over thumbnail to set default thumbnail, <b>F2</b> to rename selected item"));
        } else {
            pCore->bin()->updateKeyBinding(i18n("<b>F2</b> to rename selected item"));
        }
    } else {
        pCore->bin()->updateKeyBinding();
        if (m_lastHoveredItem.isValid()) {
            Q_EMIT displayBinFrame(m_lastHoveredItem, -1);
            m_lastHoveredItem = QModelIndex();
        }
    }
    if (m_startPos.isNull() && event->buttons() == Qt::NoButton) {
        QListView::mouseMoveEvent(event);
    }
}

MyTreeView::MyTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setEditing(false);
    setAcceptDrops(true);
}

void MyTreeView::mousePressEvent(QMouseEvent *event)
{
    QTreeView::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        QModelIndex ix = indexAt(event->pos());
        if (ix.isValid()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            QAbstractItemDelegate *del = itemDelegate(ix);
#else
            QAbstractItemDelegate *del = itemDelegateForIndex(ix);
#endif
            m_dragType = static_cast<BinItemDelegate *>(del)->dragType;
            m_startPos = event->pos();

        } else {
            m_dragType = PlaylistState::Disabled;
            m_startPos = QPoint();
        }
    }
    event->accept();
}

void MyTreeView::mouseReleaseEvent(QMouseEvent *event)
{
    m_startPos = QPoint();
    QTreeView::mouseReleaseEvent(event);
}

void MyTreeView::focusInEvent(QFocusEvent *event)
{
    QTreeView::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason) {
        Q_EMIT focusView();
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void MyTreeView::enterEvent(QEnterEvent *event)
#else
void MyTreeView::enterEvent(QEvent *event)
#endif
{
    QTreeView::enterEvent(event);
    pCore->setWidgetKeyBinding(i18n("<b>Double click</b> to add a file to the project"));
}

void MyTreeView::leaveEvent(QEvent *event)
{
    QTreeView::leaveEvent(event);
    pCore->setWidgetKeyBinding();
}

void MyTreeView::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) != 0u) {
        if (!m_startPos.isNull()) {
            int distance = (event->pos() - m_startPos).manhattanLength();
            if (distance >= QApplication::startDragDistance()) {
                performDrag();
                return;
            }
        }
        QTreeView::mouseMoveEvent(event);
        return;
    } else {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            if (KdenliveSettings::hoverPreview()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                QAbstractItemDelegate *del = itemDelegate(index);
#else
                QAbstractItemDelegate *del = itemDelegateForIndex(index);
#endif
                int frame = static_cast<BinItemDelegate *>(del)->getFrame(index, event->pos().x());
                if (frame >= 0) {
                    Q_EMIT displayBinFrame(index, frame, event->modifiers() & Qt::ShiftModifier);
                    if (m_lastHoveredItem != index) {
                        if (m_lastHoveredItem.isValid()) {
                            Q_EMIT displayBinFrame(m_lastHoveredItem, -1);
                        }
                        m_lastHoveredItem = index;
                    }
                } else if (m_lastHoveredItem.isValid()) {
                    Q_EMIT displayBinFrame(m_lastHoveredItem, -1);
                    m_lastHoveredItem = QModelIndex();
                }
                pCore->bin()->updateKeyBinding(i18n("<b>Shift+seek</b> over thumbnail to set default thumbnail, <b>F2</b> to rename selected item"));
            } else {
                pCore->bin()->updateKeyBinding(i18n("<b>F2</b> to rename selected item"));
            }
        } else {
            if (m_lastHoveredItem.isValid()) {
                Q_EMIT displayBinFrame(m_lastHoveredItem, -1);
                m_lastHoveredItem = QModelIndex();
            }
            pCore->bin()->updateKeyBinding();
        }
    }
    if (m_startPos.isNull() && event->buttons() == Qt::NoButton) {
        QTreeView::mouseMoveEvent(event);
    }
}

void MyTreeView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    QAbstractItemView::closeEditor(editor, hint);
    setEditing(false);
}

void MyTreeView::editorDestroyed(QObject *editor)
{
    QAbstractItemView::editorDestroyed(editor);
    setEditing(false);
}

bool MyTreeView::isEditing() const
{
    return state() == QAbstractItemView::EditingState;
}

void MyTreeView::setEditing(bool edit)
{
    setState(edit ? QAbstractItemView::EditingState : QAbstractItemView::NoState);
    if (!edit) {
        // Ensure edited item is selected
        Q_EMIT selectCurrent();
    }
}

bool MyTreeView::performDrag()
{
    QModelIndexList bases = selectedIndexes();
    QModelIndexList indexes;
    for (int i = 0; i < bases.count(); i++) {
        if (bases.at(i).column() == 0) {
            indexes << bases.at(i);
        }
    }
    if (indexes.isEmpty()) {
        return false;
    }
    // Check if we want audio or video only
    Q_EMIT updateDragMode(m_dragType);
    auto *drag = new QDrag(this);
    drag->setMimeData(model()->mimeData(indexes));
    QModelIndex ix = indexes.constFirst();
    if (ix.isValid()) {
        QIcon icon = ix.data(AbstractProjectItem::DataThumbnail).value<QIcon>();
        QPixmap pix = icon.pixmap(iconSize());
        QSize size = pix.size() / 2;
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setOpacity(0.7);
        p.drawPixmap(0, 0, image.width(), image.height(), pix);
        p.setOpacity(1);
        if (indexes.count() > 1) {
            QPalette palette;
            int radius = size.height() / 3;
            p.setBrush(palette.highlight());
            p.setPen(palette.highlightedText().color());
            p.drawEllipse(QPoint(size.width() / 2, size.height() / 2), radius, radius);
            p.drawText(size.width() / 2 - radius, size.height() / 2 - radius, 2 * radius, 2 * radius, Qt::AlignCenter, QString::number(indexes.count()));
        }
        p.end();
        drag->setPixmap(QPixmap::fromImage(image));
    }
    drag->exec();
    drag->deleteLater();
    Q_EMIT processDragEnd();
    return true;
}

SmallJobLabel::SmallJobLabel(QWidget *parent)
    : QPushButton(parent)

{
    setFixedWidth(0);
    setFlat(true);
    m_timeLine = new QTimeLine(500, this);
    QObject::connect(m_timeLine, &QTimeLine::valueChanged, this, &SmallJobLabel::slotTimeLineChanged);
    QObject::connect(m_timeLine, &QTimeLine::finished, this, &SmallJobLabel::slotTimeLineFinished);
    hide();
}

const QString SmallJobLabel::getStyleSheet(const QPalette &p)
{
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window);
    QColor bg = scheme.background(KColorScheme::LinkBackground).color();
    QColor fg = scheme.foreground(KColorScheme::LinkText).color();
    QString style =
        QStringLiteral("QPushButton {margin:3px;padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}")
            .arg(bg.red())
            .arg(bg.green())
            .arg(bg.blue())
            .arg(fg.red())
            .arg(fg.green())
            .arg(fg.blue());

    bg = scheme.background(KColorScheme::ActiveBackground).color();
    fg = scheme.foreground(KColorScheme::ActiveText).color();
    style.append(
        QStringLiteral("\nQPushButton:hover {margin:3px;padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}")
            .arg(bg.red())
            .arg(bg.green())
            .arg(bg.blue())
            .arg(fg.red())
            .arg(fg.green())
            .arg(fg.blue()));

    return style;
}

void SmallJobLabel::setAction(QAction *action)
{
    m_action = action;
}

void SmallJobLabel::slotTimeLineChanged(qreal value)
{
    setFixedWidth(int(qMin(value * 2, qreal(1.0)) * sizeHint().width()));
    update();
}

void SmallJobLabel::slotTimeLineFinished()
{
    if (m_timeLine->direction() == QTimeLine::Forward) {
        // Show
        m_action->setVisible(true);
    } else {
        // Hide
        m_action->setVisible(false);
        setText(QString());
    }
}

void SmallJobLabel::slotSetJobCount(int jobCount)
{
    QMutexLocker lk(&m_locker);
    if (jobCount > 0) {
        // prepare animation
        setText(i18np("%1 job", "%1 jobs", jobCount));
        setToolTip(i18np("%1 pending job", "%1 pending jobs", jobCount));

        if (style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this) != 0) {
            setFixedWidth(sizeHint().width());
            m_action->setVisible(true);
            return;
        }

        if (m_action->isVisible()) {
            setFixedWidth(sizeHint().width());
            update();
            return;
        }

        setFixedWidth(0);
        m_action->setVisible(true);
        int wantedWidth = sizeHint().width();
        setGeometry(-wantedWidth, 0, wantedWidth, height());
        m_timeLine->setDirection(QTimeLine::Forward);
        if (m_timeLine->state() == QTimeLine::NotRunning) {
            m_timeLine->start();
        }
    } else {
        if (style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this) != 0) {
            setFixedWidth(0);
            m_action->setVisible(false);
            return;
        }
        // hide
        m_timeLine->setDirection(QTimeLine::Backward);
        if (m_timeLine->state() == QTimeLine::NotRunning) {
            m_timeLine->start();
        }
    }
}

LineEventEater::LineEventEater(QObject *parent)
    : QObject(parent)
{
}

bool LineEventEater::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            Q_EMIT clearSearchLine();
        }
        break;
    case QEvent::Resize:
        // Workaround Qt BUG 54676
        Q_EMIT showClearButton(static_cast<QResizeEvent *>(event)->size().width() > QFontMetrics(QApplication::font()).averageCharWidth() * 8);
        break;
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

Bin::Bin(std::shared_ptr<ProjectItemModel> model, QWidget *parent, bool isMainBin)
    : QWidget(parent)
    , isLoading(false)
    , shouldCheckProfile(false)
    , m_isMainBin(isMainBin)
    , m_itemModel(std::move(model))
    , m_itemView(nullptr)
    , m_binTreeViewDelegate(nullptr)
    , m_binListViewDelegate(nullptr)
    , m_doc(nullptr)
    , m_extractAudioAction(nullptr)
    , m_transcodeAction(nullptr)
    , m_clipsActionsMenu(nullptr)
    , m_inTimelineAction(nullptr)
    , m_listType(BinViewType(KdenliveSettings::binMode()))
    , m_baseIconSize(160, 90)
    , m_propertiesDock(nullptr)
    , m_propertiesPanel(nullptr)
    , m_monitor(nullptr)
    , m_blankThumb()
    , m_filterTagGroup(this)
    , m_filterRateGroup(this)
    , m_filterUsageGroup(this)
    , m_filterTypeGroup(this)
    , m_invalidClipDialog(nullptr)
    , m_transcodingDialog(nullptr)
    , m_gainedFocus(false)
    , m_audioDuration(0)
    , m_processedAudio(0)
{
    m_layout = new QVBoxLayout(this);

    // Create toolbar for buttons
    m_toolbar = new QToolBar(this);
    int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_toolbar->setIconSize(QSize(iconSize, iconSize));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_layout->addWidget(m_toolbar);

    if (m_isMainBin) {
        // Init icons
        m_audioIcon = QImage(iconSize, iconSize, QImage::Format_ARGB32_Premultiplied);
        m_videoIcon = QImage(iconSize, iconSize, QImage::Format_ARGB32_Premultiplied);
        m_folderIcon = QIcon::fromTheme(QStringLiteral("folder"));
        m_sequenceFolderIcon = QIcon::fromTheme(QStringLiteral("folder-yellow"));
    }

    // Tags panel
    m_tagsWidget = new TagWidget(this);
    connect(m_tagsWidget, &TagWidget::switchTag, this, &Bin::switchTag);
    connect(m_tagsWidget, &TagWidget::updateProjectTags, this, &Bin::updateTags);
    m_tagsWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_layout->addWidget(m_tagsWidget);
    m_tagsWidget->setVisible(false);

    m_layout->setSpacing(0);
    m_layout->setContentsMargins(0, 0, 0, 0);
    // Search line
    m_searchLine = new QLineEdit(this);
    m_searchLine->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_searchLine->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    // m_searchLine->setClearButtonEnabled(true);
    m_searchLine->setPlaceholderText(i18n("Search…"));
    m_searchLine->setFocusPolicy(Qt::ClickFocus);
    m_searchLine->setAccessibleName(i18n("Bin Search"));
    connect(m_searchLine, &QLineEdit::textChanged, this, [this](const QString &str) {
        m_proxyModel->slotSetSearchString(str);
        if (str.isEmpty()) {
            // focus last selected item when clearing search line
            QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
            if (current.isValid()) {
                m_itemView->scrollTo(current, QAbstractItemView::EnsureVisible);
            }
        }
    });

    auto *leventEater = new LineEventEater(this);
    m_searchLine->installEventFilter(leventEater);
    connect(leventEater, &LineEventEater::clearSearchLine, m_searchLine, &QLineEdit::clear);
    connect(leventEater, &LineEventEater::showClearButton, this, &Bin::showClearButton);

    setFocusPolicy(Qt::ClickFocus);
    connect(this, &Bin::refreshPanel, this, &Bin::doRefreshPanel);

    // Zoom slider
    QWidget *container = new QWidget(this);
    auto *lay = new QHBoxLayout;
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMinimumWidth(40);
    m_slider->setRange(0, 10);
    m_slider->setValue(KdenliveSettings::bin_zoom());
    connect(m_slider, &QAbstractSlider::valueChanged, this, &Bin::slotSetIconSize);
    auto *tb1 = new QToolButton(this);
    tb1->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    connect(tb1, &QToolButton::clicked, this, [&]() { m_slider->setValue(qMin(m_slider->value() + 1, m_slider->maximum())); });
    auto *tb2 = new QToolButton(this);
    tb2->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    connect(tb2, &QToolButton::clicked, this, [&]() { m_slider->setValue(qMax(m_slider->value() - 1, m_slider->minimum())); });
    lay->addWidget(tb2);
    lay->addWidget(m_slider);
    lay->addWidget(tb1);
    container->setLayout(lay);
    auto *zoomWidget = new QWidgetAction(this);
    zoomWidget->setDefaultWidget(container);

    // View type
    KSelectAction *listType = new KSelectAction(QIcon::fromTheme(QStringLiteral("view-list-tree")), i18n("View Mode"), this);
    pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode"), listType);
    pCore->window()->actionCollection()->setShortcutsConfigurable(listType, false);
    QAction *treeViewAction = listType->addAction(QIcon::fromTheme(QStringLiteral("view-list-tree")), i18n("Tree View"));
    listType->addAction(treeViewAction);
    treeViewAction->setData(BinTreeView);
    if (m_listType == treeViewAction->data().toInt()) {
        listType->setCurrentAction(treeViewAction);
    }
    pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode_tree"), treeViewAction);
    QAction *profileAction = new QAction(i18n("Adjust Profile to Current Clip"), this);
    connect(profileAction, &QAction::triggered, this, &Bin::adjustProjectProfileToItem);
    pCore->window()->actionCollection()->addAction(QStringLiteral("project_adjust_profile"), profileAction);

    QAction *iconViewAction = listType->addAction(QIcon::fromTheme(QStringLiteral("view-list-icons")), i18n("Icon View"));
    iconViewAction->setData(BinIconView);
    if (m_listType == iconViewAction->data().toInt()) {
        listType->setCurrentAction(iconViewAction);
    }
    pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode_icon"), iconViewAction);

    // Sort menu
    m_sortDescend = new QAction(i18n("Descending"), this);
    m_sortDescend->setCheckable(true);
    m_sortDescend->setChecked(KdenliveSettings::binSorting() > 99);
    connect(m_sortDescend, &QAction::triggered, this, [&]() {
        if (m_sortGroup->checkedAction()) {
            int actionData = m_sortGroup->checkedAction()->data().toInt();
            if ((m_itemView != nullptr) && m_listType == BinTreeView) {
                auto *view = static_cast<QTreeView *>(m_itemView);
                view->header()->setSortIndicator(actionData, m_sortDescend->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder);
            } else {
                m_proxyModel->sort(actionData, m_sortDescend->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder);
            }
            KdenliveSettings::setBinSorting(actionData + (m_sortDescend->isChecked() ? 100 : 0));
        }
    });

    auto *sortAction = new KActionMenu(i18n("Sort By"), this);
    int binSort = KdenliveSettings::binSorting() % 100;
    m_sortGroup = new QActionGroup(sortAction);
    QAction *sortByName = new QAction(i18n("Name"), m_sortGroup);
    sortByName->setCheckable(true);
    sortByName->setData(0);
    sortByName->setChecked(binSort == 0);
    QAction *sortByDate = new QAction(i18n("Date"), m_sortGroup);
    sortByDate->setCheckable(true);
    sortByDate->setData(1);
    sortByDate->setChecked(binSort == 1);
    QAction *sortByDesc = new QAction(i18n("Description"), m_sortGroup);
    sortByDesc->setCheckable(true);
    sortByDesc->setData(2);
    sortByDesc->setChecked(binSort == 2);
    QAction *sortByType = new QAction(i18n("Type"), m_sortGroup);
    sortByType->setCheckable(true);
    sortByType->setData(3);
    sortByType->setChecked(binSort == 3);
    QAction *sortByDuration = new QAction(i18n("Duration"), m_sortGroup);
    sortByDuration->setCheckable(true);
    sortByDuration->setData(5);
    sortByDuration->setChecked(binSort == 5);
    QAction *sortByInsert = new QAction(i18n("Insert Order"), m_sortGroup);
    sortByInsert->setCheckable(true);
    sortByInsert->setData(6);
    sortByInsert->setChecked(binSort == 6);
    QAction *sortByRating = new QAction(i18n("Rating"), m_sortGroup);
    sortByRating->setCheckable(true);
    sortByRating->setData(7);
    sortByRating->setChecked(binSort == 7);
    sortAction->addAction(sortByName);
    sortAction->addAction(sortByDate);
    sortAction->addAction(sortByDuration);
    sortAction->addAction(sortByRating);
    sortAction->addAction(sortByType);
    sortAction->addAction(sortByInsert);
    sortAction->addAction(sortByDesc);
    sortAction->addSeparator();
    sortAction->addAction(m_sortDescend);
    connect(m_sortGroup, &QActionGroup::triggered, this, [&](QAction *ac) {
        int actionData = ac->data().toInt();
        if ((m_itemView != nullptr) && m_listType == BinTreeView) {
            auto *view = static_cast<QTreeView *>(m_itemView);
            view->header()->setSortIndicator(actionData, m_sortDescend->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder);
        } else {
            m_proxyModel->sort(actionData, m_sortDescend->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder);
        }
        KdenliveSettings::setBinSorting(actionData + (m_sortDescend->isChecked() ? 100 : 0));
    });

    QAction *disableEffects = new QAction(i18n("Disable Bin Effects"), this);
    disableEffects->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
    disableEffects->setData("disable_bin_effects");
    disableEffects->setCheckable(true);
    disableEffects->setChecked(false);
    connect(disableEffects, &QAction::triggered, this, [this](bool disable) { this->setBinEffectsEnabled(!disable); });
    pCore->window()->actionCollection()->addAction(QStringLiteral("disable_bin_effects"), disableEffects);

    QAction *hoverPreview = new QAction(i18n("Show Video Preview in Thumbnails"), this);
    hoverPreview->setCheckable(true);
    hoverPreview->setChecked(KdenliveSettings::hoverPreview());
    connect(hoverPreview, &QAction::triggered, [](bool checked) { KdenliveSettings::setHoverPreview(checked); });

    listType->setToolBarMode(KSelectAction::MenuMode);
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 240, 0)
    connect(listType, &KSelectAction::actionTriggered, this, &Bin::slotInitView);
#else
    connect(listType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &Bin::slotInitView);
#endif

    // Settings menu
    auto *settingsAction = new KActionMenu(QIcon::fromTheme(QStringLiteral("application-menu")), i18n("Options"), this);
    settingsAction->setWhatsThis(xi18nc("@info:whatsthis", "Opens a window to configure the project bin (e.g. view mode, sort, show rating)."));
    settingsAction->setPopupMode(QToolButton::InstantPopup);
    settingsAction->addAction(zoomWidget);
    settingsAction->addAction(listType);
    settingsAction->addAction(sortAction);

    // Column show / hide actions
    m_showDate = new QAction(i18n("Show Date"), this);
    m_showDate->setCheckable(true);
    m_showDate->setData(1);
    connect(m_showDate, &QAction::triggered, this, &Bin::slotShowColumn);
    m_showDesc = new QAction(i18n("Show Description"), this);
    m_showDesc->setCheckable(true);
    m_showDesc->setData(2);
    connect(m_showDesc, &QAction::triggered, this, &Bin::slotShowColumn);
    m_showRating = new QAction(i18n("Show Rating"), this);
    m_showRating->setCheckable(true);
    m_showRating->setData(7);
    connect(m_showRating, &QAction::triggered, this, &Bin::slotShowColumn);

    settingsAction->addAction(m_showDate);
    settingsAction->addAction(m_showDesc);
    settingsAction->addAction(m_showRating);
    settingsAction->addAction(disableEffects);
    settingsAction->addAction(hoverPreview);

    if (!m_isMainBin) {
        // Add close action
        QAction *close = KStandardAction::close(this, SIGNAL(requestBinClose()), this);
        settingsAction->addAction(close);
    }

    // Show tags panel
    m_tagAction = new QAction(QIcon::fromTheme(QStringLiteral("tag")), i18n("Tags Panel"), this);
    m_tagAction->setCheckable(true);
    m_toolbar->addAction(m_tagAction);
    connect(m_tagAction, &QAction::triggered, this, [&](bool triggered) {
        if (triggered) {
            m_tagsWidget->setVisible(true);
        } else {
            m_tagsWidget->setVisible(false);
        }
    });

    // Filter menu
    m_filterTagGroup.setExclusive(false);
    m_filterRateGroup.setExclusive(false);
    m_filterUsageGroup.setExclusive(true);
    m_filterTypeGroup.setExclusive(false);
    m_filterMenu = new QMenu(i18n("Filter"), this);
    m_filterButton = new QToolButton;
    m_filterButton->setCheckable(true);
    m_filterButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_filterButton->setIcon(QIcon::fromTheme(QStringLiteral("view-filter")));
    m_filterButton->setToolTip(i18n("Filter"));
    m_filterButton->setWhatsThis(xi18nc("@info:whatsthis", "Filter the project bin contents. Click on the filter icon to toggle the filter display. Click on "
                                                           "the arrow icon to open a list of possible filter settings."));
    m_filterButton->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_filterButton->setMenu(m_filterMenu);

    connect(m_filterButton, &QToolButton::toggled, this, [this](bool toggle) {
        if (!toggle) {
            m_proxyModel->slotClearSearchFilters();
            return;
        }
        slotApplyFilters();
    });

    connect(m_filterMenu, &QMenu::triggered, this, [this](QAction *action) {
        if (action->data().toString().isEmpty()) {
            // Clear filters action
            QSignalBlocker bk(m_filterMenu);
            QList<QAction *> list = m_filterMenu->actions();
            list << m_filterTypeGroup.actions();
            for (QAction *ac : qAsConst(list)) {
                ac->setChecked(false);
            }
            m_proxyModel->slotClearSearchFilters();
            m_filterButton->setChecked(false);
            return;
        }
        slotApplyFilters();
    });

    m_tagAction->setCheckable(true);
    m_toolbar->addAction(m_tagAction);
    m_toolbar->addAction(settingsAction);

    if (m_isMainBin) {
        // small info button for pending jobs
        m_infoLabel = new SmallJobLabel(this);
        m_infoLabel->setStyleSheet(SmallJobLabel::getStyleSheet(palette()));
        connect(&pCore->taskManager, &TaskManager::jobCount, m_infoLabel, &SmallJobLabel::slotSetJobCount);
        QAction *infoAction = m_toolbar->addWidget(m_infoLabel);
        m_jobsMenu = new QMenu(this);
        // connect(m_jobsMenu, &QMenu::aboutToShow, this, &Bin::slotPrepareJobsMenu);
        m_cancelJobs = new QAction(i18n("Cancel All Jobs"), this);
        m_cancelJobs->setCheckable(false);
        m_discardCurrentClipJobs = new QAction(i18n("Cancel Current Clip Jobs"), this);
        m_discardCurrentClipJobs->setCheckable(false);
        m_discardPendingJobs = new QAction(i18n("Cancel Pending Jobs"), this);
        m_discardPendingJobs->setCheckable(false);
        m_jobsMenu->addAction(m_cancelJobs);
        m_jobsMenu->addAction(m_discardCurrentClipJobs);
        m_jobsMenu->addAction(m_discardPendingJobs);
        m_infoLabel->setMenu(m_jobsMenu);
        m_infoLabel->setAction(infoAction);

        connect(m_discardCurrentClipJobs, &QAction::triggered, this, [&]() {
            const QString currentId = m_monitor->activeClipId();
            if (!currentId.isEmpty()) {
                pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, currentId.toInt(), QUuid()), AbstractTask::NOJOBTYPE, true);
            }
        });
        connect(m_cancelJobs, &QAction::triggered, [&]() { pCore->taskManager.slotCancelJobs(); });
        connect(m_discardPendingJobs, &QAction::triggered, [&]() {
            // TODO: implement pending only deletion
            pCore->taskManager.slotCancelJobs();
        });
        connect(this, &Bin::requestUpdateSequences, [this](QMap<QUuid, int> sequences) {
            QMapIterator<QUuid, int> s(sequences);
            while (s.hasNext()) {
                s.next();
                updateSequenceClip(s.key(), s.value(), -1, true);
            }
        });
    }
    // Hack, create toolbar spacer
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_toolbar->addWidget(spacer);

    // Add filter and search line
    m_toolbar->addWidget(m_filterButton);
    m_toolbar->addWidget(m_searchLine);

    // connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
    m_headerInfo = QByteArray::fromBase64(KdenliveSettings::treeviewheaders().toLatin1());
    if (m_isMainBin) {
        m_propertiesPanel = new QScrollArea(this);
        m_propertiesPanel->setFrameShape(QFrame::NoFrame);
        m_propertiesPanel->setAccessibleName(i18n("Bin Clip Properties"));
    }
    // Insert listview
    m_itemView = new MyTreeView(this);
    m_layout->addWidget(m_itemView);
    // Info widget for failed jobs, other errors
    m_infoMessage = new KMessageWidget(this);
    m_layout->addWidget(m_infoMessage);
    m_infoMessage->setCloseButtonVisible(false);
    connect(m_infoMessage, &KMessageWidget::hideAnimationFinished, this, &Bin::slotResetInfoMessage);
    // m_infoMessage->setWordWrap(true);
    m_infoMessage->hide();
    connect(this, &Bin::requesteInvalidRemoval, this, &Bin::slotQueryRemoval);
    connect(pCore.get(), &Core::updatePalette, this, &Bin::slotUpdatePalette);
    connect(m_itemModel.get(), &QAbstractItemModel::rowsInserted, this, &Bin::updateClipsCount);
    connect(m_itemModel.get(), &QAbstractItemModel::rowsRemoved, this, &Bin::updateClipsCount);
    connect(this, SIGNAL(displayBinMessage(QString, KMessageWidget::MessageType)), this, SLOT(doDisplayMessage(QString, KMessageWidget::MessageType)));
    wheelAccumulatedDelta = 0;
}

Bin::~Bin()
{
    if (m_isMainBin) {
        disconnect(&pCore->taskManager, &TaskManager::jobCount, m_infoLabel, &SmallJobLabel::slotSetJobCount);
        pCore->taskManager.slotCancelJobs();
        blockSignals(true);
        m_proxyModel->selectionModel()->blockSignals(true);
        setEnabled(false);
        m_propertiesPanel = nullptr;
        abortOperations();
        m_itemModel->clean();
    } else {
        blockSignals(true);
        setEnabled(false);
        m_proxyModel->selectionModel()->blockSignals(true);
    }
}

void Bin::slotUpdatePalette()
{
    if (m_isMainBin) {
        // Refresh icons
        QIcon audioIcon = QIcon::fromTheme(QStringLiteral("audio-volume-medium"));
        QIcon videoIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-video"));
        m_audioIcon.fill(Qt::transparent);
        m_videoIcon.fill(Qt::transparent);
        QPainter p(&m_audioIcon);
        audioIcon.paint(&p, 0, 0, m_audioIcon.width(), m_audioIcon.height());
        p.end();
        QPainter p2(&m_videoIcon);
        videoIcon.paint(&p2, 0, 0, m_videoIcon.width(), m_videoIcon.height());
        p2.end();
        m_audioUsedIcon = m_audioIcon;
        KIconEffect::toMonochrome(m_audioUsedIcon, palette().link().color(), palette().link().color(), 1);
        m_videoUsedIcon = m_videoIcon;
        KIconEffect::toMonochrome(m_videoUsedIcon, palette().link().color(), palette().link().color(), 1);
    }
}

QDockWidget *Bin::clipPropertiesDock()
{
    return m_propertiesDock;
}

void Bin::abortOperations()
{
    m_infoMessage->hide();
    blockSignals(true);
    if (m_propertiesPanel) {
        for (QWidget *w : m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
            delete w;
        }
    }
    delete m_itemView;
    m_itemView = nullptr;
    blockSignals(false);
}

bool Bin::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (m_itemView && m_listType == BinTreeView) {
            // Folder state is only valid in tree view mode
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && mouseEvent->modifiers() & Qt::ShiftModifier) {
                QModelIndex idx = m_itemView->indexAt(mouseEvent->pos());
                if (idx.isValid() && idx.column() == 0 && m_proxyModel) {
                    std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(idx));
                    if (item->itemType() == AbstractProjectItem::FolderItem) {
                        auto *tView = static_cast<QTreeView *>(m_itemView);
                        QRect r = tView->visualRect(idx);
                        if (mouseEvent->pos().x() < r.x()) {
                            if (!tView->isExpanded(idx)) {
                                tView->expandAll();
                            } else {
                                tView->collapseAll();
                            }
                            return true;
                        }
                    }
                }
            }
        }
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        if (!m_monitor->isActive()) {
            m_monitor->slotActivateMonitor();
        } else {
            // Force raise
            m_monitor->parentWidget()->raise();
        }
        bool success = QWidget::eventFilter(obj, event);
        if (m_gainedFocus) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (m_itemView) {
                QModelIndex idx = m_itemView->indexAt(mouseEvent->pos());
                m_gainedFocus = false;
                if (idx.isValid() && m_proxyModel) {
                    std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(idx));
                    if (item->itemType() == AbstractProjectItem::ClipItem) {
                        auto clip = std::static_pointer_cast<ProjectClip>(item);
                        if (clip && clip->statusReady()) {
                            editMasterEffect(item);
                        }
                    } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
                        auto clip = std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip();
                        if (clip && clip->statusReady()) {
                            editMasterEffect(item);
                        }
                    }
                } else {
                    editMasterEffect(nullptr);
                }
            }
            // make sure we discard the focus indicator
            m_gainedFocus = false;
        }
        return success;
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (m_itemView) {
            QModelIndex idx = m_itemView->indexAt(mouseEvent->pos());
            if (!idx.isValid()) {
                // User double clicked on empty area
                slotAddClip();
            } else {
                slotItemDoubleClicked(idx, mouseEvent->pos(), mouseEvent->modifiers());
            }
        } else {
            qCDebug(KDENLIVE_LOG) << " +++++++ NO VIEW-------!!";
        }
        return true;
    }
    if (event->type() == QEvent::Wheel) {
        auto *e = static_cast<QWheelEvent *>(event);
        if ((e != nullptr) && e->modifiers() == Qt::ControlModifier) {
            wheelAccumulatedDelta += e->angleDelta().y();
            if (abs(wheelAccumulatedDelta) >= QWheelEvent::DefaultDeltasPerStep) {
                slotZoomView(wheelAccumulatedDelta > 0);
            }
            // Q_EMIT zoomView(e->delta() > 0);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void Bin::refreshIcons()
{
    QList<QMenu *> allMenus = this->findChildren<QMenu *>();
    for (int i = 0; i < allMenus.count(); i++) {
        QMenu *m = allMenus.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setIcon(newIcon);
    }
    QList<QToolButton *> allButtons = this->findChildren<QToolButton *>();
    for (int i = 0; i < allButtons.count(); i++) {
        QToolButton *m = allButtons.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setIcon(newIcon);
    }
}

void Bin::slotSaveHeaders()
{
    if ((m_itemView != nullptr) && m_listType == BinTreeView) {
        // save current treeview state (column width)
        auto *view = static_cast<QTreeView *>(m_itemView);
        m_headerInfo = view->header()->saveState();
        KdenliveSettings::setTreeviewheaders(m_headerInfo.toBase64());
    }
}

void Bin::updateSortingAction(int ix)
{
    if (ix == KdenliveSettings::binSorting()) {
        return;
    }
    int index = ix % 100;
    for (QAction *ac : m_sortGroup->actions()) {
        if (ac->data().toInt() == index) {
            ac->setChecked(true);
            ac->trigger();
        }
    }
    if ((ix > 99) != m_sortDescend->isChecked()) {
        m_sortDescend->trigger();
    }
}

void Bin::slotZoomView(bool zoomIn)
{
    wheelAccumulatedDelta = 0;
    if (m_itemModel->rowCount() == 0) {
        // Don't zoom on empty bin
        return;
    }
    int progress = (zoomIn) ? 1 : -1;
    m_slider->setValue(m_slider->value() + progress);
}

Monitor *Bin::monitor()
{
    return m_monitor;
}

void Bin::slotAddClip()
{
    // Check if we are in a folder
    QString parentFolder = getCurrentFolder();
    ClipCreationDialog::createClipsCommand(m_doc, parentFolder, m_itemModel);
    pCore->window()->raiseBin();
}

std::shared_ptr<ProjectClip> Bin::getFirstSelectedClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return std::shared_ptr<ProjectClip>();
    }
    for (const QModelIndex &ix : indexes) {
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            auto clip = std::static_pointer_cast<ProjectClip>(item);
            if (clip) {
                return clip;
            }
        }
    }
    return nullptr;
}

void Bin::slotDeleteClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    std::vector<std::shared_ptr<AbstractProjectItem>> items;
    bool included = false;
    bool usedFolder = false;
    QList<QUuid> sequences;
    auto checkInclusion = [](bool accum, std::shared_ptr<TreeItem> item) {
        return accum || std::static_pointer_cast<AbstractProjectItem>(item)->isIncludedInTimeline();
    };
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        if (!item) {
            qDebug() << "Suspicious: item not found when trying to delete";
            continue;
        }
        included = included || item->accumulate(false, checkInclusion);
        // Check if we are deleting non-empty folders:
        usedFolder = usedFolder || item->childCount() > 0;
        if (item->itemType() == AbstractProjectItem::FolderItem) {
            QList<std::shared_ptr<ProjectClip>> children = std::static_pointer_cast<ProjectFolder>(item)->childClips();
            for (auto &c : children) {
                if (c->clipType() == ClipType::Timeline) {
                    const QUuid uuid = c->getSequenceUuid();
                    sequences << uuid;
                }
            }

        } else if (item->itemType() == AbstractProjectItem::ClipItem) {
            auto c = std::static_pointer_cast<ProjectClip>(item);
            if (c->clipType() == ClipType::Timeline) {
                const QUuid uuid = c->getSequenceUuid();
                sequences << uuid;
            }
        }
        items.push_back(item);
    }
    if (!sequences.isEmpty()) {
        if (sequences.count() == m_itemModel->sequenceCount()) {
            KMessageBox::error(this, i18n("You cannot delete all sequences of a project"));
            return;
        }
        if (KMessageBox::warningContinueCancel(this, i18n("Deleting sequences cannot be undone")) != KMessageBox::Continue) {
            return;
        }
        for (auto seq : sequences) {
            pCore->projectManager()->closeTimeline(seq, true);
        }
    }
    if (included && (KMessageBox::warningContinueCancel(this, i18n("This will delete all selected clips from the timeline")) != KMessageBox::Continue)) {
        return;
    }
    if (usedFolder && (KMessageBox::warningContinueCancel(this, i18n("This will delete all folder content")) != KMessageBox::Continue)) {
        return;
    }
    // Clear undostack as it contains references to deleted timeline models
    pCore->undoStack()->clear();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Ensure we don't delete a parent before a child
    // std::sort(items.begin(), items.end(), [](std::shared_ptr<AbstractProjectItem> a, std::shared_ptr<AbstractProjectItem>b) { return a->depth() > b->depth();
    // });
    QStringList notDeleted;
    for (const auto &item : items) {
        if (!m_itemModel->requestBinClipDeletion(item, undo, redo)) {
            notDeleted << item->name();
        }
    }
    if (!notDeleted.isEmpty()) {
        KMessageBox::errorList(this, i18n("Some items could not be deleted. Maybe there are instances on locked tracks?"), notDeleted);
    }
    pCore->pushUndo(undo, redo, i18n("Delete bin Clips"));
}

void Bin::slotReloadClip()
{
    qDebug() << "---------\nRELOADING CLIP\n----------------";
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        std::shared_ptr<ProjectClip> currentItem = nullptr;
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            currentItem = std::static_pointer_cast<ProjectClip>(item);
        } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
            currentItem = std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip();
        }
        if (currentItem) {
            Q_EMIT openClip(std::shared_ptr<ProjectClip>());
            if (currentItem->clipStatus() == FileStatus::StatusMissing || currentItem->clipStatus() == FileStatus::StatusProxyOnly) {
                // Don't attempt to reload missing clip
                // Check if source file is available
                const QString sourceUrl = currentItem->url();
                if (!QFile::exists(sourceUrl)) {
                    Q_EMIT displayBinMessage(i18n("Missing source clip"), KMessageWidget::Warning);
                    return;
                }
            }
            if (currentItem->clipType() == ClipType::Playlist) {
                // Check if a clip inside playlist is missing
                QString path = currentItem->url();
                QFile f(path);
                QDomDocument doc;
                if (!Xml::docContentFromFile(doc, path, false)) {
                    DocumentChecker d(QUrl::fromLocalFile(path), doc);
                    if (!d.hasErrorInProject() && doc.documentElement().hasAttribute(QStringLiteral("modified"))) {
                        QString backupFile = path + QStringLiteral(".backup");
                        KIO::FileCopyJob *copyjob = KIO::file_copy(QUrl::fromLocalFile(path), QUrl::fromLocalFile(backupFile));
                        if (copyjob->exec()) {
                            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                KMessageBox::error(this, i18n("Unable to write to file %1", path));
                            } else {
                                QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                                out.setCodec("UTF-8");
#endif
                                out << doc.toString();
                                f.close();
                                KMessageBox::information(
                                    this,
                                    i18n("Your project file was modified by Kdenlive.\nTo make sure you do not lose data, a backup copy called %1 was created.",
                                         backupFile));
                            }
                        }
                    }
                } else {
                    KMessageBox::error(this, i18n("Unable to parse file %1", path));
                }
            }
            currentItem->reloadProducer(false, false, true);
        }
    }
}

void Bin::replaceSingleClip(const QString clipId, const QString &newUrl)
{
    if (newUrl.isEmpty() || !QFile::exists(newUrl)) {
        Q_EMIT displayBinMessage(i18n("Cannot replace clip with invalid file %1", QFileInfo(newUrl).fileName()), KMessageWidget::Information);
        return;
    }
    std::shared_ptr<ProjectClip> currentItem = getBinClip(clipId);
    if (currentItem) {
        QMap<QString, QString> sourceProps;
        QMap<QString, QString> newProps;
        sourceProps.insert(QStringLiteral("resource"), currentItem->url());
        sourceProps.insert(QStringLiteral("kdenlive:originalurl"), currentItem->url());
        sourceProps.insert(QStringLiteral("kdenlive:clipname"), currentItem->clipName());
        sourceProps.insert(QStringLiteral("kdenlive:proxy"), currentItem->getProducerProperty(QStringLiteral("kdenlive:proxy")));
        sourceProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
        newProps.insert(QStringLiteral("resource"), newUrl);
        newProps.insert(QStringLiteral("kdenlive:originalurl"), newUrl);
        newProps.insert(QStringLiteral("kdenlive:clipname"), QFileInfo(newUrl).fileName());
        newProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
        newProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
        // Check if replacement clip is long enough
        if (currentItem->hasLimitedDuration() && currentItem->isIncludedInTimeline()) {
            // Clip is used in timeline, make sure length is similar
            std::unique_ptr<Mlt::Producer> replacementProd(new Mlt::Producer(pCore->getProjectProfile(), newUrl.toUtf8().constData()));
            int currentDuration = int(currentItem->frameDuration());
            if (replacementProd->is_valid()) {
                int replacementDuration = replacementProd->get_length();
                if (replacementDuration < currentDuration) {
                    if (KMessageBox::warningContinueCancel(
                            this, i18n("You are replacing a clip with a shorter one, this might cause issues in timeline.\nReplacement is %1 frames shorter.",
                                       (currentDuration - replacementDuration))) != KMessageBox::Continue) {
                        return;
                        ;
                    }
                }
            } else {
                KMessageBox::error(this, i18n("The selected file %1 is invalid.", newUrl));
                return;
            }
        }
        slotEditClipCommand(currentItem->clipId(), sourceProps, newProps);
    } else {
        Q_EMIT displayBinMessage(i18n("Cannot find original clip to be replaced"), KMessageWidget::Information);
    }
}

void Bin::slotReplaceClipInTimeline()
{
    // Check if we have 2 selected clips
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QModelIndex activeIndex = m_proxyModel->selectionModel()->currentIndex();
    std::shared_ptr<AbstractProjectItem> selectedItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(activeIndex));
    std::shared_ptr<AbstractProjectItem> secondaryItem = nullptr;
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0 || ix == activeIndex) {
            continue;
        }
        if (secondaryItem != nullptr) {
            // More than 2 clips selected, abort
            secondaryItem = nullptr;
            break;
        }
        secondaryItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
    }
    if (selectedItem == nullptr || secondaryItem == nullptr || selectedItem->itemType() != AbstractProjectItem::ClipItem ||
        secondaryItem->itemType() != AbstractProjectItem::ClipItem) {
        KMessageBox::information(this, i18n("You need to select 2 clips (the replacement clip and the original clip) for timeline replacement."));
        return;
    }
    // Check that we don't try to embed a sequence onto itself
    auto selected = std::static_pointer_cast<ProjectClip>(selectedItem);
    if (selected->timelineInstances().isEmpty()) {
        KMessageBox::information(this, i18n("The current clip <b>%1</b> is not inserted in the active timeline.", selected->clipName()));
        return;
    }
    auto secondary = std::static_pointer_cast<ProjectClip>(secondaryItem);
    if (secondary->clipType() == ClipType::Timeline && secondary->getSequenceUuid() == pCore->currentTimelineId()) {
        KMessageBox::information(this, i18n("You cannot insert the sequence <b>%1</b> into itself.", secondary->clipName()));
        return;
    }
    std::pair<bool, bool> hasAV = {selected->hasAudio(), selected->hasVideo()};
    std::pair<bool, bool> secondaryHasAV = {secondary->hasAudio(), secondary->hasVideo()};
    std::pair<bool, bool> replacementAV = {false, false};
    if (secondaryHasAV.first && hasAV.first) {
        // Both clips have audio
        replacementAV.first = true;
        if (secondaryHasAV.second && hasAV.second) {
            // Both clips have video, ask what user wants
            replacementAV.second = true;
        }
    } else if (secondaryHasAV.second && hasAV.second) {
        replacementAV.second = true;
    }
    qDebug() << "ANALYSIS RESULT:\nMASTER: " << hasAV << "\nSECONDARY: " << secondaryHasAV << "\nRESULT: " << replacementAV;
    if (replacementAV.first == false && replacementAV.second == false) {
        KMessageBox::information(this,
                                 i18n("The selected clips %1 and %2 don't match, cannot replace audio nor video", selected->clipName(), secondary->clipName()));
        return;
    }
    int sourceDuration = selected->frameDuration();
    int replaceDuration = secondary->frameDuration();
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    if (replaceDuration != sourceDuration) {
        // Display respective clip duration in case of mismatch
        l->addWidget(new QLabel(i18n("Replacing instances of clip <b>%1</b> (%2)<br/>in active timeline\nwith clip <b>%3</b> (%4)", selected->clipName(),
                                     selected->getStringDuration(), secondary->clipName(), secondary->getStringDuration()),
                                &d));
    } else {
        l->addWidget(new QLabel(
            i18n("Replacing instances of clip <b>%1</b><br/>in active timeline\nwith clip <b>%2</b>", selected->clipName(), secondary->clipName()), &d));
    }
    QCheckBox *cbAudio = new QCheckBox(i18n("Replace audio"), this);
    cbAudio->setEnabled(replacementAV.first);
    cbAudio->setChecked(replacementAV.first);
    QCheckBox *cbVideo = new QCheckBox(i18n("Replace video"), this);
    cbVideo->setEnabled(replacementAV.second);
    cbVideo->setChecked(replacementAV.second && !replacementAV.first);
    l->addWidget(cbAudio);
    l->addWidget(cbVideo);
    if (sourceDuration != replaceDuration) {
        KMessageWidget *km = new KMessageWidget(this);
        km->setCloseButtonVisible(false);
        if (sourceDuration > replaceDuration) {
            km->setMessageType(KMessageWidget::Warning);
            km->setText(i18n("Replacement clip is shorter than source clip.<br><b>Some clips in timeline might not be replaced</b>"));
        } else {
            km->setMessageType(KMessageWidget::Information);
            km->setText(i18n("Replacement clip duration is longer that your source clip"));
        }
        l->addWidget(km);
    }
    l->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted || (!cbVideo->isCheckable() && !cbAudio->isChecked())) {
        return;
    }
    pCore->projectManager()->replaceTimelineInstances(selected->clipId(), secondary->clipId(), cbAudio->isChecked(), cbVideo->isChecked());
}

void Bin::slotReplaceClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QModelIndex activeSelection = m_proxyModel->selectionModel()->currentIndex();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        std::shared_ptr<ProjectClip> currentItem = nullptr;
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            currentItem = std::static_pointer_cast<ProjectClip>(item);
            if (ix == activeSelection) {
                qDebug() << "==== FOUND ACTIVE CLIP: " << currentItem->clipUrl();
            } else {
                qDebug() << "==== FOUND SELECED CLIP: " << currentItem->clipUrl();
            }
        } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
            currentItem = std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip();
        }
        if (currentItem) {
            Q_EMIT openClip(std::shared_ptr<ProjectClip>());
            QString fileName = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Open Replacement for %1", currentItem->clipName()),
                                                            QFileInfo(currentItem->url()).absolutePath(), ClipCreationDialog::getExtensionsFilter());
            if (!fileName.isEmpty()) {
                QMap<QString, QString> sourceProps;
                QMap<QString, QString> newProps;
                std::pair<bool, bool> hasAV = {currentItem->hasAudio(), currentItem->hasVideo()};
                sourceProps.insert(QStringLiteral("resource"), currentItem->url());
                sourceProps.insert(QStringLiteral("kdenlive:originalurl"), currentItem->url());
                sourceProps.insert(QStringLiteral("kdenlive:clipname"), currentItem->clipName());
                sourceProps.insert(QStringLiteral("kdenlive:proxy"), currentItem->getProducerProperty(QStringLiteral("kdenlive:proxy")));
                sourceProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
                newProps.insert(QStringLiteral("resource"), fileName);
                newProps.insert(QStringLiteral("kdenlive:originalurl"), fileName);
                newProps.insert(QStringLiteral("kdenlive:clipname"), QFileInfo(fileName).fileName());
                newProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
                newProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
                // Check if replacement clip is long enough
                if (currentItem->hasLimitedDuration() && currentItem->isIncludedInTimeline()) {
                    // Clip is used in timeline, make sure length is similar
                    std::unique_ptr<Mlt::Producer> replacementProd(new Mlt::Producer(pCore->getProjectProfile(), fileName.toUtf8().constData()));
                    int currentDuration = int(currentItem->frameDuration());
                    if (replacementProd->is_valid()) {
                        replacementProd->probe();
                        std::pair<bool, bool> replacementHasAV = {
                            (replacementProd->property_exists("audio_index") && replacementProd->get_int("audio_index") > -1),
                            (replacementProd->property_exists("video_index") && replacementProd->get_int("video_index") > -1)};
                        if (hasAV != replacementHasAV) {
                            replacementProd.reset();
                            QString message;
                            std::pair<bool, bool> replaceAV = {false, false};
                            if (hasAV.first && hasAV.second) {
                                if (!replacementHasAV.second) {
                                    // Original clip has audio and video, replacement has only audio
                                    replaceAV.first = true;
                                    message = i18n("Replacement clip does not contain a video stream. Do you want to replace only the audio component of this "
                                                   "clip in the active timeline ?");
                                } else if (!replacementHasAV.first) {
                                    // Original clip has audio and video, replacement has only video
                                    replaceAV.second = true;
                                    message = i18n("Replacement clip does not contain an audio stream. Do you want to replace only the video component of this "
                                                   "clip in the active timeline ?");
                                }
                                if (replaceAV.first == false && replaceAV.second == false) {
                                    KMessageBox::information(this, i18n("You cannot replace a clip with a different type of clip (audio/video not matching)."));
                                    return;
                                }
                                if (KMessageBox::questionTwoActions(QApplication::activeWindow(), message, {}, KGuiItem(i18n("Replace in timeline")),
                                                                    KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction) {
                                    return;
                                }
                                // Replace only one component.
                                QString folder = QStringLiteral("-1");
                                auto containingFolder = std::static_pointer_cast<ProjectFolder>(currentItem->parent());
                                if (containingFolder) {
                                    folder = containingFolder->clipId();
                                }
                                Fun undo = []() { return true; };
                                Fun redo = []() { return true; };
                                std::function<void(const QString &)> callBack = [replaceAudio = replaceAV.first,
                                                                                 sourceId = currentItem->clipId()](const QString &binId) {
                                    qDebug() << "::: READY TO REPLACE: " << sourceId << ", WITH: " << binId;
                                    if (!binId.isEmpty()) {
                                        pCore->projectManager()->replaceTimelineInstances(sourceId, binId, replaceAudio, !replaceAudio);
                                    }
                                    return true;
                                };
                                const QString clipId = ClipCreator::createClipFromFile(fileName, folder, m_itemModel, undo, redo, callBack);
                                pCore->pushUndo(undo, redo, i18nc("@action", "Add clip"));
                            } else if ((hasAV.first && !replacementHasAV.first) || (hasAV.second && !replacementHasAV.second)) {
                                KMessageBox::information(this, i18n("You cannot replace a clip with a different type of clip (audio/video not matching)."));
                            }
                            return;
                        }
                        int replacementDuration = replacementProd->get_length();
                        if (replacementDuration < currentDuration) {
                            if (KMessageBox::warningContinueCancel(
                                    this,
                                    i18n("You are replacing a clip with a shorter one, this might cause issues in timeline.\nReplacement is %1 frames shorter.",
                                         (currentDuration - replacementDuration))) != KMessageBox::Continue) {
                                continue;
                            }
                        }
                    } else {
                        KMessageBox::error(this, i18n("The selected file %1 is invalid.", fileName));
                        continue;
                    }
                }
                slotEditClipCommand(currentItem->clipId(), sourceProps, newProps);
            }
        }
    }
}

void Bin::slotLocateClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        std::shared_ptr<ProjectClip> currentItem = nullptr;
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            currentItem = std::static_pointer_cast<ProjectClip>(item);
        } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
            currentItem = std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip();
        }
        if (currentItem) {
            QUrl url = QUrl::fromLocalFile(currentItem->url()); //.adjusted(QUrl::RemoveFilename);
            bool exists = QFile(url.toLocalFile()).exists();
            if (currentItem->hasUrl() && exists) {
                KIO::highlightInFileManager({url});
                qCDebug(KDENLIVE_LOG) << "  / / " + url.toString();
            } else {
                if (!exists) {
                    pCore->displayMessage(i18n("Could not locate %1", url.toString()), MessageType::ErrorMessage, 300);
                }
                return;
            }
        }
    }
}

void Bin::slotDuplicateClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QList<std::shared_ptr<AbstractProjectItem>> items;
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        items << m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
    }
    std::function<void(const QString &)> callBack = [this](const QString &binId) {
        if (!binId.isEmpty()) {
            selectClipById(binId);
            std::shared_ptr<ProjectClip> clip = getBinClip(binId);
            if (clip && clip->clipType() == ClipType::Timeline) {
                // For duplicated timeline clips, we need to build the timelinemodel otherwise the producer is not correctly saved
                const QUuid uuid = clip->getSequenceUuid();
                return pCore->projectManager()->openTimeline(binId, uuid, -1, true);
            }
        }
        return true;
    };
    int ix = 0;
    QString lastId;
    for (const auto &item : qAsConst(items)) {
        ix++;
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            auto currentItem = std::static_pointer_cast<ProjectClip>(item);
            if (currentItem) {
                if (currentItem->clipType() == ClipType::Timeline) {
                    const QUuid uuid = currentItem->getSequenceUuid();
                    if (m_doc->getTimelinesUuids().contains(uuid)) {
                        // Sync last changes for this timeline if it is opened
                        m_doc->storeGroups(uuid);
                        pCore->projectManager()->syncTimeline(uuid, true);
                    }
                    QTemporaryFile src(QDir::temp().absoluteFilePath(QString("XXXXXX.mlt")));
                    src.setAutoRemove(false);
                    if (!src.open()) {
                        pCore->displayMessage(i18n("Could not create temporary file in %1", QDir::temp().absolutePath()), MessageType::ErrorMessage, 500);
                        return;
                    }
                    // Save playlist to disk
                    currentItem->cloneProducerToFile(src.fileName());
                    // extract xml
                    QDomDocument xml = ClipCreator::getXmlFromUrl(src.fileName());
                    if (xml.isNull()) {
                        pCore->displayMessage(i18n("Duplicating sequence failed"), MessageType::ErrorMessage, 500);
                        return;
                    }
                    QDomDocument doc;
                    if (!Xml::docContentFromFile(doc, src.fileName(), false)) {
                        return;
                    }
                    QMutexLocker lock(&pCore->xmlMutex);
                    const QByteArray result = doc.toString().toUtf8();
                    std::shared_ptr<Mlt::Producer> xmlProd(new Mlt::Producer(pCore->getProjectProfile(), "xml-string", result.constData()));
                    lock.unlock();
                    QString id;
                    Fun undo = []() { return true; };
                    Fun redo = []() { return true; };
                    xmlProd->set("kdenlive:clipname", i18n("%1 (copy)", currentItem->clipName()).toUtf8().constData());
                    xmlProd->set("kdenlive:sequenceproperties.documentuuid", m_doc->uuid().toString().toUtf8().constData());
                    m_itemModel->requestAddBinClip(id, xmlProd, item->parent()->clipId(), undo, redo, callBack);
                    pCore->pushUndo(undo, redo, i18n("Duplicate clip"));
                } else {
                    QDomDocument doc;
                    QDomElement xml = currentItem->toXml(doc);
                    if (!xml.isNull()) {
                        QString currentName = Xml::getXmlProperty(xml, QStringLiteral("kdenlive:clipname"));
                        if (currentName.isEmpty()) {
                            QUrl url = QUrl::fromLocalFile(Xml::getXmlProperty(xml, QStringLiteral("resource")));
                            if (url.isValid()) {
                                currentName = url.fileName();
                            }
                        }
                        if (!currentName.isEmpty()) {
                            currentName.append(i18nc("append to clip name to indicate a copied idem", " (copy)"));
                            Xml::setXmlProperty(xml, QStringLiteral("kdenlive:clipname"), currentName);
                        }
                        if (currentItem->clipType() == ClipType::Text) {
                            // Remove unique id
                            Xml::removeXmlProperty(xml, QStringLiteral("kdenlive:uniqueId"));
                        }
                        QString id;
                        if (ix == items.count()) {
                            m_itemModel->requestAddBinClip(id, xml, item->parent()->clipId(), i18n("Duplicate clip"), callBack);
                        } else {
                            m_itemModel->requestAddBinClip(id, xml, item->parent()->clipId(), i18n("Duplicate clip"));
                        }
                    }
                }
            }
        } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
            auto currentItem = std::static_pointer_cast<ProjectSubClip>(item);
            QString id;
            QPoint clipZone = currentItem->zone();
            m_itemModel->requestAddBinSubClip(id, clipZone.x(), clipZone.y(), {}, currentItem->getMasterClip()->clipId());
            lastId = id;
        }
    }
    if (!lastId.isEmpty()) {
        selectClipById(lastId);
    }
}

void Bin::setMonitor(Monitor *monitor)
{
    m_monitor = monitor;
    connect(m_monitor, &Monitor::addClipToProject, this, &Bin::slotAddClipToProject);
    connect(m_monitor, &Monitor::refreshCurrentClip, this, &Bin::slotOpenCurrent);
    connect(m_itemModel.get(), &ProjectItemModel::resetPlayOrLoopZone, m_monitor, &Monitor::resetPlayOrLoopZone, Qt::DirectConnection);
    connect(this, &Bin::openClip, [&](std::shared_ptr<ProjectClip> clip, int in, int out) {
        m_monitor->slotOpenClip(clip, in, out);
        if (clip && clip->hasLimitedDuration()) {
            clip->refreshBounds();
        }
        pCore->textEditWidget()->openClip(clip);
    });
}

void Bin::cleanDocument()
{
    blockSignals(true);
    if (m_proxyModel) {
        m_proxyModel->selectionModel()->blockSignals(true);
    }
    setEnabled(false);
    // Cleanup references in the clip properties dialog
    Q_EMIT requestShowClipProperties(nullptr);

    // Cleanup previous project
    m_itemModel->clean();
    if (m_propertiesPanel) {
        m_propertiesPanel->setProperty("clipId", QString());
        for (QWidget *w : m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
            delete w;
        }
    }
    delete m_itemView;
    m_itemView = nullptr;
    isLoading = false;
    shouldCheckProfile = false;
    pCore->textEditWidget()->openClip(nullptr);
}

const QString Bin::setDocument(KdenliveDoc *project, const QString &id)
{
    m_doc = project;
    QString folderName;
    if (m_isMainBin) {
        m_infoLabel->slotSetJobCount(0);
    }
    int iconHeight = int(QFontInfo(font()).pixelSize() * 3.5);
    m_baseIconSize = QSize(int(iconHeight * pCore->getCurrentDar()), iconHeight);
    setEnabled(true);
    blockSignals(false);
    if (m_proxyModel) {
        m_proxyModel->selectionModel()->blockSignals(false);
    }
    // reset filtering
    QSignalBlocker bk(m_filterButton);
    m_filterButton->setChecked(false);
    m_filterButton->setToolTip(i18n("Filter"));
    connect(m_proxyAction, &QAction::toggled, m_doc, [&](bool doProxy) { m_doc->slotProxyCurrentItem(doProxy); });

    // connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), m_itemView
    // connect(m_itemModel, SIGNAL(updateCurrentItem()), this, SLOT(autoSelect()));
    slotInitView(nullptr);
    bool binEffectsDisabled = getDocumentProperty(QStringLiteral("disablebineffects")).toInt() == 1;
    // Set media browser url
    QString url = getDocumentProperty(QStringLiteral("browserurl"));
    const QString sorting = getDocumentProperty(QStringLiteral("binsort"));
    if (!sorting.isEmpty()) {
        int binSorting = sorting.toInt();
        updateSortingAction(binSorting);
    }
    if (!url.isEmpty()) {
        if (QFileInfo(url).isRelative()) {
            url.prepend(m_doc->documentRoot());
        }
        pCore->mediaBrowser()->setUrl(QUrl::fromLocalFile(url));
    }
    QAction *disableEffects = pCore->window()->actionCollection()->action(QStringLiteral("disable_bin_effects"));
    if (disableEffects) {
        if (binEffectsDisabled != disableEffects->isChecked()) {
            QSignalBlocker effectBk(disableEffects);
            disableEffects->setChecked(binEffectsDisabled);
        }
    }
    m_itemModel->setBinEffectsEnabled(!binEffectsDisabled);
    if (!id.isEmpty()) {
        // Open view in a specific folder
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getItemByBinId(id);
        auto parentIx = m_itemModel->getIndexFromItem(item);
        m_itemView->setRootIndex(m_proxyModel->mapFromSource(parentIx));
        folderName = item->name();
        m_upAction->setEnabled(true);
        m_upAction->setVisible(true);
    }

    // setBinEffectsEnabled(!binEffectsDisabled, false);
    QMap<int, QStringList> projectTags = m_doc->getProjectTags();
    m_tagsWidget->rebuildTags(projectTags);
    rebuildFilters(projectTags.size());
    return folderName;
}

void Bin::rebuildFilters(int tagsCount)
{
    m_filterMenu->clear();

    // Add clear entry
    QAction *clearFilter = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear")), i18n("Clear Filters"), this);
    m_filterMenu->addAction(clearFilter);

    // Add tag filters
    m_filterMenu->addSeparator();

    QAction *tagFilter = new QAction(i18n("No Tags"), &m_filterTagGroup);
    tagFilter->setData(QStringLiteral("#"));
    tagFilter->setCheckable(true);
    m_filterMenu->addAction(tagFilter);

    for (int i = 1; i <= tagsCount; i++) {
        QAction *tag = pCore->window()->actionCollection()->action(QString("tag_%1").arg(i));
        if (tag) {
            QAction *tagFilter = new QAction(tag->icon(), tag->text(), &m_filterTagGroup);
            tagFilter->setData(tag->data());
            tagFilter->setCheckable(true);
            m_filterMenu->addAction(tagFilter);
        }
    }

    // Add rating filters
    m_filterMenu->addSeparator();

    for (int i = 0; i < 6; ++i) {
        auto *rateFilter = new QAction(QIcon::fromTheme(QStringLiteral("favorite")), i18np("%1 Star", "%1 Stars", i), &m_filterRateGroup);
        rateFilter->setData(QString(".%1").arg(2 * i));
        rateFilter->setCheckable(true);
        m_filterMenu->addAction(rateFilter);
    }
    // Add usage filter
    m_filterMenu->addSeparator();
    auto *usageMenu = new QMenu(i18n("Filter by Usage"), m_filterMenu);
    m_filterMenu->addMenu(usageMenu);

    auto *usageFilter = new QAction(i18n("All Clips"), &m_filterUsageGroup);
    usageFilter->setData(ProjectSortProxyModel::All);
    usageFilter->setCheckable(true);
    usageFilter->setChecked(true);
    usageMenu->addAction(usageFilter);
    usageFilter = new QAction(i18n("Unused Clips"), &m_filterUsageGroup);
    usageFilter->setData(ProjectSortProxyModel::Unused);
    usageFilter->setCheckable(true);
    usageMenu->addAction(usageFilter);
    usageFilter = new QAction(i18n("Used Clips"), &m_filterUsageGroup);
    usageFilter->setData(ProjectSortProxyModel::Used);
    usageFilter->setCheckable(true);
    usageMenu->addAction(usageFilter);

    // Add type filters
    m_filterMenu->addSeparator();
    auto *typeMenu = new QMenu(i18n("Filter by Type"), m_filterMenu);
    m_filterMenu->addMenu(typeMenu);
    m_filterMenu->addSeparator();
    auto *typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("video-x-generic")), i18n("AV Clip"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::AV);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("video-x-matroska")), i18n("Mute Video"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::Video);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("audio-x-generic")), i18n("Audio"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::Audio);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("image-jpeg")), i18n("Image"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::Image);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-add-slide-clip")), i18n("Slideshow"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::SlideShow);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("video-mlt-playlist")), i18n("Playlist"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::Playlist);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("video-mlt-playlist")), i18n("Sequences"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::Timeline);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("draw-text")), i18n("Title"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::Text);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("draw-text")), i18n("Title Template"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::TextTemplate);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
    typeFilter = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-add-color-clip")), i18n("Color"), &m_filterTypeGroup);
    typeFilter->setData(ClipType::Color);
    typeFilter->setCheckable(true);
    typeMenu->addAction(typeFilter);
}

void Bin::slotApplyFilters()
{
    QList<QAction *> list = m_filterMenu->actions();
    QList<int> rateFilters;
    QList<int> typeFilters;
    ProjectSortProxyModel::UsageFilter usageFilter = ProjectSortProxyModel::UsageFilter(m_filterUsageGroup.checkedAction()->data().toInt());
    QStringList tagFilters;
    for (QAction *ac : qAsConst(list)) {
        if (ac->isChecked()) {
            QString actionData = ac->data().toString();
            if (actionData.startsWith(QLatin1Char('#'))) {
                // Filter by tag
                tagFilters << actionData;
            } else if (actionData.startsWith(QLatin1Char('.'))) {
                // Filter by rating
                rateFilters << actionData.remove(0, 1).toInt();
            }
        }
    }
    // Type actions
    list = m_filterTypeGroup.actions();
    for (QAction *ac : qAsConst(list)) {
        if (ac->isChecked()) {
            typeFilters << ac->data().toInt();
        }
    }
    QSignalBlocker bkt(m_filterButton);
    if (!rateFilters.isEmpty() || !tagFilters.isEmpty() || !typeFilters.isEmpty() || usageFilter != ProjectSortProxyModel::All) {
        m_filterButton->setChecked(true);
    } else {
        m_filterButton->setChecked(false);
    }
    m_proxyModel->slotSetFilters(tagFilters, rateFilters, typeFilters, usageFilter);
}

void Bin::createClip(const QDomElement &xml)
{
    // Check if clip should be in a folder
    QString groupId = ProjectClip::getXmlProperty(xml, QStringLiteral("kdenlive:folderid"));
    std::shared_ptr<ProjectFolder> parentFolder = m_itemModel->getFolderByBinId(groupId);
    if (!parentFolder) {
        parentFolder = m_itemModel->getRootFolder();
    }
    QString path = Xml::getXmlProperty(xml, QStringLiteral("resource"));
    if (path.endsWith(QStringLiteral(".mlt")) || path.endsWith(QStringLiteral(".kdenlive"))) {
        QDomDocument doc;
        if (!Xml::docContentFromFile(doc, path, false)) {
            return;
        }
        DocumentChecker d(QUrl::fromLocalFile(path), doc);
        if (!d.hasErrorInProject() && doc.documentElement().hasAttribute(QStringLiteral("modified"))) {
            QString backupFile = path + QStringLiteral(".backup");
            KIO::FileCopyJob *copyjob = KIO::file_copy(QUrl::fromLocalFile(path), QUrl::fromLocalFile(backupFile));
            if (copyjob->exec()) {
                QFile f(path);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    KMessageBox::error(this, i18n("Unable to write to file %1", path));
                } else {
                    QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    out.setCodec("UTF-8");
#endif
                    out << doc.toString();
                    f.close();
                    KMessageBox::information(
                        this, i18n("Your project file was modified by Kdenlive.\nTo make sure you do not lose data, a backup copy called %1 was created.",
                                   backupFile));
                }
            }
        }
    }
    QString id = Xml::getTagContentByAttribute(xml, QStringLiteral("property"), QStringLiteral("name"), QStringLiteral("kdenlive:id"));
    if (id.isEmpty()) {
        id = QString::number(m_itemModel->getFreeClipId());
    }
    auto newClip = ProjectClip::construct(id, xml, m_blankThumb, m_itemModel);
    parentFolder->appendChild(newClip);
}

void Bin::slotAddFolder()
{
    auto parentFolder = m_itemModel->getFolderByBinId(getCurrentFolder());
    qDebug() << "parent folder id" << parentFolder->clipId();
    QString newId;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    m_itemModel->requestAddFolder(newId, i18n("Folder"), parentFolder->clipId(), undo, redo);
    pCore->pushUndo(undo, redo, i18n("Create bin folder"));
    if (m_listType == BinTreeView) {
        // Make sure parent folder is expanded
        if (parentFolder->clipId().toInt() > -1) {
            auto parentIx = m_itemModel->getIndexFromItem(parentFolder);
            auto *view = static_cast<QTreeView *>(m_itemView);
            view->expand(m_proxyModel->mapFromSource(parentIx));
        }
    }

    // Edit folder name
    auto folder = m_itemModel->getFolderByBinId(newId);
    auto ix = m_itemModel->getIndexFromItem(folder);

    // Scroll to ensure folder is visible
    m_itemView->scrollTo(m_proxyModel->mapFromSource(ix), QAbstractItemView::EnsureVisible);
    qDebug() << "selecting" << ix;
    if (ix.isValid()) {
        qDebug() << "ix valid";
        m_proxyModel->selectionModel()->clearSelection();
        int row = ix.row();
        const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
        const QModelIndex id2 = m_itemModel->index(row, m_itemModel->columnCount() - 1, ix.parent());
        if (id.isValid() && id2.isValid()) {
            m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)),
                                                   QItemSelectionModel::Select);
        }
        m_itemView->edit(m_proxyModel->mapFromSource(ix));
    }
}

void Bin::ensureCurrent()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selection().indexes();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        m_itemView->setCurrentIndex(ix);
    }
}

QModelIndex Bin::getIndexForId(const QString &id, bool folderWanted) const
{
    QModelIndexList items = m_itemModel->match(m_itemModel->index(0, 0), AbstractProjectItem::DataId, QVariant::fromValue(id), 1, Qt::MatchRecursive);
    for (int i = 0; i < items.count(); i++) {
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(items.at(i));
        AbstractProjectItem::PROJECTITEMTYPE type = currentItem->itemType();
        if (folderWanted && type == AbstractProjectItem::FolderItem) {
            // We found our folder
            return items.at(i);
        }
        if (!folderWanted && type == AbstractProjectItem::ClipItem) {
            // We found our clip
            return items.at(i);
        }
    }
    return {};
}

void Bin::selectAll()
{
    QModelIndex rootIndex = m_itemView->rootIndex();
    QModelIndex currentSelection = m_proxyModel->selectionModel()->currentIndex();
    if (currentSelection.isValid()) {
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(currentSelection));
        if (currentItem) {
            std::shared_ptr<ProjectFolder> parentFolder = std::static_pointer_cast<ProjectFolder>(currentItem->getEnclosingFolder());
            if (parentFolder) {
                rootIndex = m_proxyModel->mapFromSource(getIndexForId(parentFolder->clipId(), true));
            }
        }
    }
    m_proxyModel->selectAll(rootIndex);
}

void Bin::selectClipById(const QString &clipId, int frame, const QPoint &zone, bool activateMonitor)
{
    if (m_monitor->activeClipId() == clipId) {
        std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(clipId);
        if (clip) {
            QModelIndex ix = m_itemModel->getIndexFromItem(clip);
            int row = ix.row();
            const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
            const QModelIndex id2 = m_itemModel->index(row, m_itemModel->columnCount() - 1, ix.parent());
            if (id.isValid() && id2.isValid()) {
                m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)),
                                                       QItemSelectionModel::SelectCurrent);
            }
            m_itemView->scrollTo(m_proxyModel->mapFromSource(ix), QAbstractItemView::EnsureVisible);
        }
    } else {
        std::shared_ptr<ProjectClip> clip = getBinClip(clipId);
        if (clip == nullptr) {
            return;
        }
        selectClip(clip);
    }
    if (!zone.isNull()) {
        m_monitor->slotLoadClipZone(zone);
    }
    if (activateMonitor) {
        if (frame > -1) {
            m_monitor->slotSeek(frame);
            m_monitor->refreshMonitor();
        } else {
            m_monitor->slotActivateMonitor();
        }
    }
}

void Bin::selectProxyModel(const QModelIndex &id)
{
    if (isLoading) {
        // return;
    }
    if (id.isValid()) {
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(id));
        if (currentItem) {
            if (m_monitor->activeClipId() == currentItem->clipId()) {
                qDebug() << "//// COMPARING BIN CLIP ID - - - - - ALREADY OPENED";
                return;
            }
            QString clipService;
            // Set item as current so that it displays its content in clip monitor
            setCurrent(currentItem);
            AbstractProjectItem::PROJECTITEMTYPE itemType = currentItem->itemType();
            bool isClip = itemType == AbstractProjectItem::ClipItem;
            bool isFolder = itemType == AbstractProjectItem::FolderItem;
            std::shared_ptr<ProjectClip> clip = nullptr;
            if (isClip) {
                clip = std::static_pointer_cast<ProjectClip>(currentItem);
                m_tagsWidget->setTagData(clip->tags());
                m_deleteAction->setText(i18n("Delete Clip"));
                m_deleteAction->setWhatsThis(
                    xi18nc("@info:whatsthis", "Deletes the currently selected clips from the project bin and also from the timeline."));
                m_proxyAction->setText(i18n("Proxy Clip"));
            } else if (itemType == AbstractProjectItem::FolderItem) {
                // A folder was selected, disable editing clip
                m_tagsWidget->setTagData();
                m_deleteAction->setText(i18n("Delete Folder"));
                m_proxyAction->setText(i18n("Proxy Folder"));
            } else if (itemType == AbstractProjectItem::SubClipItem) {
                m_tagsWidget->setTagData(currentItem->tags());
                auto subClip = std::static_pointer_cast<ProjectSubClip>(currentItem);
                clip = subClip->getMasterClip();
                m_deleteAction->setText(i18n("Delete Clip"));
                m_deleteAction->setWhatsThis(
                    xi18nc("@info:whatsthis", "Deletes the currently selected clips from the project bin and also from the timeline."));
                m_proxyAction->setText(i18n("Proxy Clip"));
            }
            bool isImported = false;
            bool hasAudio = false;
            ClipType::ProducerType type = ClipType::Unknown;
            // don't need to wait for the clip to be ready to get its type
            if (clip) {
                type = clip->clipType();
            }
            if (clip && clip->statusReady()) {
                Q_EMIT requestShowClipProperties(clip, false);
                m_proxyAction->blockSignals(true);
                clipService = clip->getProducerProperty(QStringLiteral("mlt_service"));
                hasAudio = clip->hasAudio();
                m_proxyAction->setChecked(clip->hasProxy());
                m_proxyAction->blockSignals(false);
                if (clip->hasUrl()) {
                    isImported = true;
                }
            } else if (clip && clip->clipStatus() == FileStatus::StatusMissing) {
                m_openAction->setEnabled(false);
            } else {
                // Disable find in timeline option
                m_openAction->setEnabled(false);
            }
            if (m_clipsActionsMenu) {
                m_clipsActionsMenu->setEnabled(!isFolder);
            }
            m_editAction->setVisible(!isFolder);
            m_editAction->setEnabled(true);
            m_extractAudioAction->menuAction()->setVisible(hasAudio);
            m_extractAudioAction->setEnabled(hasAudio);
            m_openAction->setEnabled(type == ClipType::Image || type == ClipType::Audio || type == ClipType::TextTemplate || type == ClipType::Text ||
                                     type == ClipType::Animation);
            m_openAction->setVisible(!isFolder);
            m_duplicateAction->setEnabled(isClip);
            m_duplicateAction->setVisible(!isFolder);
            if (m_inTimelineAction) {
                m_inTimelineAction->setEnabled(isClip);
                m_inTimelineAction->setVisible(isClip);
            }
            m_locateAction->setEnabled(!isFolder && isImported);
            m_locateAction->setVisible(!isFolder && isImported);
            m_proxyAction->setEnabled(m_doc->useProxy() && !isFolder);
            m_reloadAction->setEnabled(isClip && type != ClipType::Timeline);
            m_reloadAction->setVisible(!isFolder);
            m_replaceAction->setEnabled(isClip);
            m_replaceAction->setVisible(!isFolder);
            m_replaceInTimelineAction->setEnabled(isClip);
            m_replaceInTimelineAction->setVisible(!isFolder);
            // Enable actions depending on clip type
            for (auto &a : m_clipsActionsMenu->actions()) {
                qDebug() << "ACTION: " << a->text() << " = " << a->data().toString();
                QString actionType = a->data().toString().section(QLatin1Char(';'), 1);
                qDebug() << ":::: COMPARING ACTIONTYPE: " << actionType << " = " << type;
                if (actionType.isEmpty()) {
                    a->setEnabled(true);
                } else if (actionType.contains(QLatin1Char('v')) && (type == ClipType::AV || type == ClipType::Video)) {
                    a->setEnabled(true);
                } else if (actionType.contains(QLatin1Char('a')) && (type == ClipType::AV || type == ClipType::Audio)) {
                    a->setEnabled(true);
                } else if (actionType.contains(QLatin1Char('i')) && type == ClipType::Image) {
                    a->setEnabled(true);
                } else {
                    a->setEnabled(false);
                }
            }
            m_clipsActionsMenu->menuAction()->setVisible(!isFolder && (type == ClipType::AV || type == ClipType::Timeline || type == ClipType::Playlist ||
                                                                       type == ClipType::Image || type == ClipType::Video || type == ClipType::Audio));

            m_transcodeAction->setEnabled(!isFolder);
            m_transcodeAction->setVisible(!isFolder && (type == ClipType::Playlist || type == ClipType::Timeline || type == ClipType::Text ||
                                                        clipService.contains(QStringLiteral("avformat"))));

            m_deleteAction->setEnabled(true);
            m_renameAction->setEnabled(true);
            updateClipsCount();
            return;
        }
    } else {
        // No item selected in bin
        Q_EMIT requestShowClipProperties(nullptr);
        Q_EMIT requestClipShow(nullptr);
        // clear effect stack
        Q_EMIT requestShowEffectStack(QString(), nullptr, QSize(), false);
        // Display black bg in clip monitor
        Q_EMIT openClip(std::shared_ptr<ProjectClip>());
    }
    m_editAction->setEnabled(false);
    m_clipsActionsMenu->setEnabled(false);
    m_extractAudioAction->setEnabled(false);
    m_transcodeAction->setEnabled(false);
    m_proxyAction->setEnabled(false);
    m_reloadAction->setEnabled(false);
    m_replaceAction->setEnabled(false);
    m_replaceInTimelineAction->setEnabled(false);
    m_locateAction->setEnabled(false);
    m_duplicateAction->setEnabled(false);
    m_openAction->setEnabled(false);
    m_deleteAction->setEnabled(false);
    m_renameAction->setEnabled(false);
    updateClipsCount();
}

std::vector<QString> Bin::selectedClipsIds(bool allowSubClips)
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    std::vector<QString> ids;
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        if (item->itemType() == AbstractProjectItem::SubClipItem) {
            if (allowSubClips) {
                auto subClipItem = std::static_pointer_cast<ProjectSubClip>(item);
                ids.push_back(subClipItem->cutClipId());
            } else {
                ids.push_back(item->clipId());
            }
        } else if (item->itemType() == AbstractProjectItem::ClipItem) {
            ids.push_back(item->clipId());
        }
    }
    return ids;
}

QList<std::shared_ptr<ProjectClip>> Bin::selectedClips()
{
    auto ids = selectedClipsIds();
    QList<std::shared_ptr<ProjectClip>> ret;
    for (const auto &id : ids) {
        ret.push_back(m_itemModel->getClipByBinID(id));
    }
    return ret;
}

void Bin::slotInitView(QAction *action)
{
    QString rootFolder;
    QString selectedItem;
    QStringList selectedItems;
    if (action) {
        QModelIndex currentSelection;
        if (m_itemView && m_proxyModel) {
            // Check currently selected item
            QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
            for (auto &ix : indexes) {
                if (!ix.isValid()) {
                    continue;
                }
                std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
                if (currentItem) {
                    selectedItems << currentItem->clipId();
                }
            }
            currentSelection = m_proxyModel->selectionModel()->currentIndex();
            if (currentSelection.isValid()) {
                std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(currentSelection));
                if (currentItem) {
                    selectedItem = currentItem->clipId();
                    std::shared_ptr<ProjectFolder> parentFolder = std::static_pointer_cast<ProjectFolder>(currentItem->getEnclosingFolder());
                    if (parentFolder) {
                        rootFolder = parentFolder->clipId();
                    }
                }
            }
        }
        if (m_proxyModel) {
            m_proxyModel->selectionModel()->clearSelection();
        }
        int viewType = action->data().toInt();
        if (m_isMainBin) {
            KdenliveSettings::setBinMode(viewType);
        }
        if (viewType == m_listType) {
            return;
        }
        if (m_listType == BinTreeView) {
            // save current treeview state (column width)
            auto *view = static_cast<QTreeView *>(m_itemView);
            m_headerInfo = view->header()->saveState();
            m_showDate->setEnabled(true);
            m_showDesc->setEnabled(true);
            m_showRating->setEnabled(true);
            m_upAction->setEnabled(false);
        }
        m_listType = static_cast<BinViewType>(viewType);
    }
    if (m_itemView) {
        delete m_itemView;
    }
    delete m_binTreeViewDelegate;
    delete m_binListViewDelegate;
    m_binTreeViewDelegate = nullptr;
    m_binListViewDelegate = nullptr;

    switch (m_listType) {
    case BinIconView:
        m_itemView = new MyListView(this);
        m_binListViewDelegate = new BinListItemDelegate(this);
        m_showDate->setEnabled(false);
        m_showDesc->setEnabled(false);
        m_showRating->setEnabled(false);
        m_upAction->setVisible(true);
        break;
    default:
        m_itemView = new MyTreeView(this);
        m_binTreeViewDelegate = new BinItemDelegate(this);
        m_showDate->setEnabled(true);
        m_showDesc->setEnabled(true);
        m_showRating->setEnabled(true);
        m_upAction->setVisible(false);
        break;
    }
    m_itemView->setMouseTracking(true);
    m_itemView->viewport()->installEventFilter(this);
    m_iconSize = m_baseIconSize * ((m_listType == BinIconView ? qMax(1, m_slider->value()) : m_slider->value()) / 4.0);
    m_itemView->setIconSize(m_iconSize);
    QPixmap pix(m_iconSize);
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
    m_itemView->addAction(m_renameAction);
    m_renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_proxyModel = std::make_unique<ProjectSortProxyModel>(this);
    // Connect models
    m_proxyModel->setSourceModel(m_itemModel.get());
    connect(m_itemModel.get(), &QAbstractItemModel::dataChanged, m_proxyModel.get(), &ProjectSortProxyModel::slotDataChanged);
    connect(m_proxyModel.get(), &ProjectSortProxyModel::updateRating, this, [&](const QModelIndex &ix, uint rating) {
        const QModelIndex index = m_proxyModel->mapToSource(ix);
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(index);
        if (item) {
            uint previousRating = item->rating();
            Fun undo = [this, item, index, previousRating]() {
                item->setRating(previousRating);
                Q_EMIT m_itemModel->dataChanged(index, index, {AbstractProjectItem::DataRating});
                return true;
            };
            Fun redo = [this, item, index, rating]() {
                item->setRating(rating);
                Q_EMIT m_itemModel->dataChanged(index, index, {AbstractProjectItem::DataRating});
                return true;
            };
            redo();
            pCore->pushUndo(undo, redo, i18n("Edit rating"));
        } else {
            Q_EMIT displayBinMessage(i18n("Cannot set rating on this item"), KMessageWidget::Information);
        }
    });
    connect(m_proxyModel.get(), &ProjectSortProxyModel::selectModel, this, &Bin::selectProxyModel);
    connect(m_proxyModel.get(), &QAbstractItemModel::layoutAboutToBeChanged, this, &Bin::slotSetSorting);
    m_itemView->setModel(m_proxyModel.get());
    m_itemView->setSelectionModel(m_proxyModel->selectionModel());
    m_proxyModel->setDynamicSortFilter(true);
    m_layout->insertWidget(2, m_itemView);
    // Reset drag type to normal
    m_itemModel->setDragType(PlaylistState::Disabled);

    // setup some default view specific parameters
    if (m_listType == BinTreeView) {
        m_itemView->setItemDelegate(m_binTreeViewDelegate);
        auto *view = static_cast<MyTreeView *>(m_itemView);
        view->setSortingEnabled(true);
        view->setWordWrap(true);
        connect(view, &MyTreeView::updateDragMode, m_itemModel.get(), &ProjectItemModel::setDragType, Qt::DirectConnection);
        connect(view, &MyTreeView::processDragEnd, this, &Bin::processDragEnd);
        connect(view, &MyTreeView::selectCurrent, this, &Bin::ensureCurrent);
        connect(view, &MyTreeView::displayBinFrame, this, &Bin::showBinFrame);
        if (!m_headerInfo.isEmpty()) {
            view->header()->restoreState(m_headerInfo);
        } else {
            view->header()->resizeSections(QHeaderView::ResizeToContents);
            view->resizeColumnToContents(0);
            // Date Column
            view->setColumnHidden(1, true);
            // Description Column
            view->setColumnHidden(2, true);
            // Rating column
            view->setColumnHidden(7, true);
        }
        // Type column
        view->setColumnHidden(3, true);
        // Tags column
        view->setColumnHidden(4, true);
        // Duration column
        view->setColumnHidden(5, true);
        // ID column
        view->setColumnHidden(6, true);
        // Rating column
        view->header()->resizeSection(7, QFontInfo(font()).pixelSize() * 4);
        // Usage column
        view->setColumnHidden(8, true);
        m_showDate->setChecked(!view->isColumnHidden(1));
        m_showDesc->setChecked(!view->isColumnHidden(2));
        m_showRating->setChecked(!view->isColumnHidden(7));
        if (m_sortGroup->checkedAction()) {
            view->header()->setSortIndicator(m_sortGroup->checkedAction()->data().toInt(),
                                             m_sortDescend->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder);
        }
        connect(view->header(), &QHeaderView::sectionResized, this, &Bin::slotSaveHeaders);
        connect(view->header(), &QHeaderView::sectionClicked, this, &Bin::slotSaveHeaders);
        connect(view->header(), &QHeaderView::sortIndicatorChanged, this, [this](int ix, Qt::SortOrder order) {
            QSignalBlocker bk(m_sortDescend);
            QSignalBlocker bk2(m_sortGroup);
            m_sortDescend->setChecked(order == Qt::DescendingOrder);
            QList<QAction *> actions = m_sortGroup->actions();
            for (auto ac : qAsConst(actions)) {
                if (ac->data().toInt() == ix) {
                    ac->setChecked(true);
                    break;
                }
            }
            KdenliveSettings::setBinSorting(ix + (order == Qt::DescendingOrder ? 100 : 0));
        });
        connect(view, &MyTreeView::focusView, this, &Bin::slotGotFocus);
    } else if (m_listType == BinIconView) {
        m_itemView->setItemDelegate(m_binListViewDelegate);
        auto *view = static_cast<MyListView *>(m_itemView);
        connect(view, &MyListView::updateDragMode, m_itemModel.get(), &ProjectItemModel::setDragType, Qt::DirectConnection);
        int textHeight = int(QFontInfo(font()).pixelSize() * 1.5);
        view->setGridSize(QSize(m_iconSize.width() + 2, m_iconSize.height() + textHeight));
        connect(view, &MyListView::focusView, this, &Bin::slotGotFocus);
        connect(view, &MyListView::displayBinFrame, this, &Bin::showBinFrame);
        connect(view, &MyListView::processDragEnd, this, &Bin::processDragEnd);
        if (!rootFolder.isEmpty()) {
            // Open view in a specific folder
            std::shared_ptr<AbstractProjectItem> folder = m_itemModel->getItemByBinId(rootFolder);
            auto parentIx = m_itemModel->getIndexFromItem(folder);
            m_itemView->setRootIndex(m_proxyModel->mapFromSource(parentIx));
            m_upAction->setEnabled(rootFolder != QLatin1String("-1"));
        }
    }
    if (!selectedItems.isEmpty()) {
        for (auto &item : selectedItems) {
            std::shared_ptr<AbstractProjectItem> clip = m_itemModel->getItemByBinId(item);
            QModelIndex ix = m_itemModel->getIndexFromItem(clip);
            int row = ix.row();
            const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
            const QModelIndex id2 = m_itemModel->index(row, m_itemModel->columnCount() - 1, ix.parent());
            if (id.isValid() && id2.isValid()) {
                m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)),
                                                       QItemSelectionModel::Select);
            }
        }
    }
    m_itemView->setEditTriggers(QAbstractItemView::NoEditTriggers); // DoubleClicked);
    m_itemView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_itemView->setDragDropMode(QAbstractItemView::DragDrop);
    m_itemView->setAlternatingRowColors(true);
    m_itemView->setFocus();
}

void Bin::slotSetIconSize(int size)
{
    if (!m_itemView) {
        return;
    }
    KdenliveSettings::setBin_zoom(size);
    m_iconSize = m_baseIconSize * ((m_listType == BinIconView ? qMax(1, size) : size) / 4.0);
    m_itemView->setIconSize(m_iconSize);
    if (m_listType == BinIconView) {
        auto *view = static_cast<MyListView *>(m_itemView);
        int textHeight = int(QFontInfo(font()).pixelSize() * 1.5);
        view->setGridSize(QSize(m_iconSize.width() + 2, m_iconSize.height() + textHeight));
    }
    QPixmap pix(m_iconSize);
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
}

void Bin::contextMenuEvent(QContextMenuEvent *event)
{
    bool enableClipActions = false;
    bool isFolder = false;
    QString clipService;
    bool clickInView = false;
    if (m_itemView) {
        QRect viewRect(m_itemView->mapToGlobal(QPoint(0, 0)), m_itemView->mapToGlobal(QPoint(m_itemView->width(), m_itemView->height())));
        if (viewRect.contains(event->globalPos())) {
            clickInView = true;
            QModelIndex idx = m_itemView->indexAt(m_itemView->viewport()->mapFromGlobal(event->globalPos()));
            if (idx.isValid()) {
                // User right clicked on a clip
                std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(idx));
                if (currentItem) {
                    enableClipActions = true;
                    if (currentItem->itemType() == AbstractProjectItem::FolderItem) {
                        isFolder = true;
                    }
                }
            }
        }
    }
    if (!clickInView) {
        return;
    }

    // New folder can be created from level of another folder.
    if (isFolder) {
        m_menu->insertAction(m_deleteAction, m_createFolderAction);
        m_menu->insertAction(m_createFolderAction, m_sequencesFolderAction);
        m_menu->insertAction(m_createFolderAction, m_audioCapturesFolderAction);
        m_menu->insertAction(m_sequencesFolderAction, m_openInBin);
    } else {
        m_menu->removeAction(m_createFolderAction);
        m_menu->removeAction(m_openInBin);
        m_menu->removeAction(m_sequencesFolderAction);
        m_menu->removeAction(m_audioCapturesFolderAction);
    }

    // Show menu
    event->setAccepted(true);
    if (enableClipActions) {
        m_menu->exec(event->globalPos());
    } else {
        // Clicked in empty area, will show main menu.
        // Before that `createFolderAction` is inserted - it allows to distinguish between showing that item by clicking on empty area and by clicking on "Add
        // Clip" menu.
        m_addButton->menu()->insertAction(m_addClip, m_createFolderAction);

        m_addButton->menu()->exec(event->globalPos());

        // Clear up this action to not show it on "Add Clip" menu from toolbar icon.
        m_addButton->menu()->removeAction(m_createFolderAction);
    }
}

void Bin::slotItemDoubleClicked(const QModelIndex &ix, const QPoint &pos, uint modifiers)
{
    std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
    if (m_listType == BinIconView) {
        if (item->childCount() > 0 || item->itemType() == AbstractProjectItem::FolderItem) {
            m_itemView->setRootIndex(ix);
            parentWidget()->setWindowTitle(item->name());
            m_upAction->setEnabled(true);
            return;
        }
    } else {
        if (!m_isMainBin && item->itemType() == AbstractProjectItem::FolderItem) {
            // Double click a folder in secondary bin will set it as bin root
            m_itemView->setRootIndex(ix);
            parentWidget()->setWindowTitle(item->name());
            m_upAction->setEnabled(true);
            return;
        }
        if (ix.column() == 0 && item->childCount() > 0) {
            QRect IconRect = m_itemView->visualRect(ix);
            IconRect.setWidth(int(double(IconRect.height()) / m_itemView->iconSize().height() * m_itemView->iconSize().width()));
            if (!pos.isNull() && (IconRect.contains(pos) || pos.y() > (IconRect.y() + IconRect.height() / 2))) {
                auto *view = static_cast<QTreeView *>(m_itemView);
                bool expand = !view->isExpanded(ix);
                // Expand all items on shift + double click
                if (modifiers & Qt::ShiftModifier) {
                    if (expand) {
                        view->expandAll();
                    } else {
                        view->collapseAll();
                    }
                } else {
                    view->setExpanded(ix, expand);
                }
                return;
            }
        }
    }
    if (ix.isValid()) {
        QRect IconRect = m_itemView->visualRect(ix);
        IconRect.setWidth(int(double(IconRect.height()) / m_itemView->iconSize().height() * m_itemView->iconSize().width()));
        if (!pos.isNull() && ((ix.column() == 2 && item->itemType() == AbstractProjectItem::ClipItem) ||
                              (!IconRect.contains(pos) && pos.y() < (IconRect.y() + IconRect.height() / 2)))) {
            // User clicked outside icon, trigger rename
            m_itemView->edit(ix);
            return;
        }
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            std::shared_ptr<ProjectClip> clip = std::static_pointer_cast<ProjectClip>(item);
            if (clip) {
                if (clip->clipType() == ClipType::Timeline) {
                    const QUuid uuid = clip->getSequenceUuid();
                    pCore->projectManager()->openTimeline(clip->binId(), uuid);
                } else if (clip->clipType() == ClipType::Text || clip->clipType() == ClipType::TextTemplate) {
                    // m_propertiesPanel->setEnabled(false);
                    showTitleWidget(clip);
                } else {
                    slotSwitchClipProperties(clip);
                }
            }
        }
    }
}

void Bin::slotEditClip()
{
    if (m_propertiesPanel == nullptr) {
        return;
    }
    QString panelId = m_propertiesPanel->property("clipId").toString();
    QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(current));
    if (item->clipId() != panelId) {
        // wrong clip
        return;
    }
    auto clip = std::static_pointer_cast<ProjectClip>(item);
    QString parentFolder = getCurrentFolder();
    switch (clip->clipType()) {
    case ClipType::Text:
    case ClipType::TextTemplate:
        showTitleWidget(clip);
        break;
    case ClipType::SlideShow:
        showSlideshowWidget(clip);
        break;
    case ClipType::QText:
        ClipCreationDialog::createQTextClip(parentFolder, this, clip.get());
        break;
    default:
        break;
    }
}

void Bin::slotSwitchClipProperties()
{
    QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    if (current.isValid()) {
        // User clicked in the icon, open clip properties
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(current));
        std::shared_ptr<ProjectClip> currentItem = nullptr;
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            currentItem = std::static_pointer_cast<ProjectClip>(item);
        } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
            currentItem = std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip();
        }
        if (currentItem) {
            slotSwitchClipProperties(currentItem);
            return;
        }
    }
    slotSwitchClipProperties(nullptr);
}

void Bin::slotSwitchClipProperties(const std::shared_ptr<ProjectClip> &clip)
{
    if (m_propertiesPanel == nullptr) {
        return;
    }
    if (clip == nullptr) {
        m_propertiesPanel->setEnabled(false);
        return;
    }
    if (clip->clipType() == ClipType::SlideShow) {
        m_propertiesPanel->setEnabled(false);
        showSlideshowWidget(clip);
    } else if (clip->clipType() == ClipType::QText) {
        m_propertiesPanel->setEnabled(false);
        QString parentFolder = getCurrentFolder();
        ClipCreationDialog::createQTextClip(parentFolder, this, clip.get());
    } else {
        m_propertiesPanel->setEnabled(true);
        Q_EMIT requestShowClipProperties(clip);
        m_propertiesDock->show();
        m_propertiesDock->raise();
    }
    // Check if properties panel is not tabbed under Bin
    // if (!pCore->window()->isTabbedWith(m_propertiesDock, QStringLiteral("project_bin"))) {
}

void Bin::doRefreshPanel(const QString &id)
{
    std::shared_ptr<ProjectClip> currentItem = getFirstSelectedClip();
    if ((currentItem != nullptr) && currentItem->AbstractProjectItem::clipId() == id) {
        Q_EMIT requestShowClipProperties(currentItem, true);
    }
}

QAction *Bin::addAction(const QString &name, const QString &text, const QIcon &icon, const QString &category)
{
    auto *action = new QAction(text, this);
    if (!icon.isNull()) {
        action->setIcon(icon);
    }
    pCore->window()->addAction(name, action, {}, category);
    return action;
}

void Bin::setupAddClipAction(QMenu *addClipMenu, ClipType::ProducerType type, const QString &name, const QString &text, const QIcon &icon)
{
    QAction *action = addAction(name, text, icon, QStringLiteral("addclip"));
    action->setData(static_cast<QVariant>(type));
    addClipMenu->addAction(action);
    connect(action, &QAction::triggered, this, &Bin::slotCreateProjectClip);
    if (name == QLatin1String("add_animation_clip") && !KdenliveSettings::producerslist().contains(QLatin1String("glaxnimate"))) {
        action->setEnabled(false);
    } else if (name == QLatin1String("add_text_clip") && !KdenliveSettings::producerslist().contains(QLatin1String("kdenlivetitle"))) {
        action->setEnabled(false);
    }
}

void Bin::showClipProperties(const std::shared_ptr<ProjectClip> &clip, bool forceRefresh)
{
    if (m_propertiesPanel == nullptr) {
        return;
    }
    if ((clip == nullptr) || !clip->statusReady()) {
        for (QWidget *w : m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
            delete w;
        }
        m_propertiesPanel->setProperty("clipId", QString());
        m_propertiesPanel->setEnabled(false);
        Q_EMIT setupTargets(false, {});
        return;
    }
    m_propertiesPanel->setEnabled(true);
    QString panelId = m_propertiesPanel->property("clipId").toString();
    if (!forceRefresh && panelId == clip->AbstractProjectItem::clipId()) {
        // the properties panel is already displaying current clip, do nothing
        return;
    }
    // Cleanup widget for new content
    for (QWidget *w : m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
        delete w;
    }
    m_propertiesPanel->setProperty("clipId", clip->AbstractProjectItem::clipId());
    // Setup timeline targets
    Q_EMIT setupTargets(clip->hasVideo(), clip->activeStreams());
    auto *lay = static_cast<QVBoxLayout *>(m_propertiesPanel->layout());
    if (lay == nullptr) {
        lay = new QVBoxLayout(m_propertiesPanel);
        m_propertiesPanel->setLayout(lay);
    }
    ClipPropertiesController *panel = clip->buildProperties(m_propertiesPanel);
    connect(panel, &ClipPropertiesController::updateClipProperties, this, &Bin::slotEditClipCommand);
    connect(panel, &ClipPropertiesController::seekToFrame, m_monitor, static_cast<void (Monitor::*)(int)>(&Monitor::slotSeek));
    connect(panel, &ClipPropertiesController::editClip, this, &Bin::slotEditClip);
    connect(panel, &ClipPropertiesController::editAnalysis, this, &Bin::slotAddClipExtraData);

    lay->addWidget(panel);
}

void Bin::slotEditClipCommand(const QString &id, const QMap<QString, QString> &oldProps, const QMap<QString, QString> &newProps)
{
    auto *command = new EditClipCommand(this, id, oldProps, newProps, true);
    m_doc->commandStack()->push(command);
}

void Bin::reloadClip(const QString &id)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (!clip) {
        return;
    }
    clip->reloadProducer(false, false);
}

void Bin::reloadMonitorIfActive(const QString &id)
{
    if (m_monitor->activeClipId() == id || m_monitor->activeClipId().isEmpty()) {
        slotOpenCurrent();
        if (m_monitor->activeClipId() == id) {
            // Ensure proxy action is correctly updated
            if (m_doc->useProxy()) {
                std::shared_ptr<AbstractProjectItem> item = m_itemModel->getItemByBinId(id);
                if (item) {
                    AbstractProjectItem::PROJECTITEMTYPE itemType = item->itemType();
                    if (itemType == AbstractProjectItem::ClipItem) {
                        std::shared_ptr<ProjectClip> clip = std::static_pointer_cast<ProjectClip>(item);
                        if (clip && clip->statusReady()) {
                            m_proxyAction->blockSignals(true);
                            m_proxyAction->setChecked(clip->hasProxy());
                            m_proxyAction->blockSignals(false);
                        }
                    }
                }
            }
            Q_EMIT requestShowClipProperties(getBinClip(id), true);
        }
    }
}

void Bin::reloadMonitorStreamIfActive(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
        m_monitor->reloadActiveStream();
    }
}

void Bin::updateTargets(QString id)
{
    if (id == QLatin1String("-1")) {
        id = m_monitor->activeClipId();
    } else if (m_monitor->activeClipId() != id) {
        qDebug() << "ABOIRT A";
        return;
    }
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (clip) {
        Q_EMIT setupTargets(clip->hasVideo(), clip->activeStreams());
    } else {
        Q_EMIT setupTargets(false, {});
    }
}

QStringList Bin::getBinFolderClipIds(const QString &id) const
{
    QStringList ids;
    std::shared_ptr<ProjectFolder> folder = m_itemModel->getFolderByBinId(id);
    if (folder) {
        for (int i = 0; i < folder->childCount(); i++) {
            std::shared_ptr<AbstractProjectItem> child = std::static_pointer_cast<AbstractProjectItem>(folder->child(i));
            if (child->itemType() == AbstractProjectItem::ClipItem) {
                ids << child->clipId();
            }
        }
    }
    return ids;
}

std::shared_ptr<ProjectClip> Bin::getBinClip(const QString &id)
{
    return m_itemModel->getClipByBinID(id);
}

const QString Bin::getBinClipName(const QString &id) const
{
    std::shared_ptr<ProjectClip> clip = nullptr;
    if (id.contains(QLatin1Char('_'))) {
        clip = m_itemModel->getClipByBinID(id.section(QLatin1Char('_'), 0, 0));
    } else if (!id.isEmpty()) {
        clip = m_itemModel->getClipByBinID(id);
    }
    if (clip) {
        return clip->clipName();
    }
    return QString();
}

void Bin::setWaitingStatus(const QString &id)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (clip) {
        clip->setClipStatus(FileStatus::StatusWaiting);
    }
}

void Bin::slotRemoveInvalidClip(const QString &id, bool replace, const QString &errorMessage)
{
    Q_UNUSED(replace);

    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (!clip) {
        return;
    }
    Q_EMIT requesteInvalidRemoval(id, clip->url(), errorMessage);
}

void Bin::selectClip(const std::shared_ptr<ProjectClip> &clip)
{
    QModelIndex ix = m_itemModel->getIndexFromItem(clip);
    int row = ix.row();
    const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
    // Ensure parent folder is expanded
    if (m_listType == BinTreeView) {
        // Make sure parent folder is expanded
        auto *view = static_cast<QTreeView *>(m_itemView);
        view->expand(m_proxyModel->mapFromSource(ix.parent()));
        const QModelIndex id2 = m_itemModel->index(row, m_itemModel->columnCount() - 1, ix.parent());
        if (id.isValid() && id2.isValid()) {
            m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)),
                                                   QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        }
    } else {
        // Ensure parent folder is currently opened
        m_itemView->setRootIndex(m_proxyModel->mapFromSource(ix.parent()));
        m_upAction->setEnabled(!ix.parent().data(AbstractProjectItem::DataId).toString().isEmpty());
        if (id.isValid()) {
            m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        }
    }
    m_itemView->scrollTo(m_proxyModel->mapFromSource(ix), QAbstractItemView::EnsureVisible);
}

void Bin::slotOpenCurrent()
{
    std::shared_ptr<ProjectClip> currentItem = getFirstSelectedClip();
    if (currentItem) {
        Q_EMIT openClip(currentItem);
    }
}

void Bin::openProducer(std::shared_ptr<ProjectClip> controller)
{
    Q_EMIT openClip(std::move(controller));
}

void Bin::openProducer(std::shared_ptr<ProjectClip> controller, int in, int out)
{
    Q_EMIT openClip(std::move(controller), in, out);
}

void Bin::emitItemUpdated(std::shared_ptr<AbstractProjectItem> item)
{
    Q_EMIT itemUpdated(std::move(item));
}

void Bin::emitRefreshPanel(const QString &id)
{
    Q_EMIT refreshPanel(id);
}

void Bin::setupGeneratorMenu()
{
    if (!m_menu) {
        qCDebug(KDENLIVE_LOG) << "Warning, menu was not created, something is wrong";
        return;
    }

    auto *addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("generators"), pCore->window()));
    if (addMenu) {
        QMenu *menu = m_addButton->menu();
        menu->addMenu(addMenu);
        addMenu->setEnabled(!addMenu->isEmpty());
        m_addButton->setMenu(menu);
    }

    addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("extract_audio"), pCore->window()));
    if (addMenu) {
        m_menu->addMenu(addMenu);
        addMenu->setEnabled(!addMenu->isEmpty());
        m_extractAudioAction = addMenu;
        m_extractAudioAction->setEnabled(false);
    }

    addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("clip_actions"), pCore->window()));
    if (addMenu) {
        m_menu->addMenu(addMenu);
        addMenu->setEnabled(!addMenu->isEmpty());
        m_clipsActionsMenu = addMenu;
        m_clipsActionsMenu->setEnabled(false);
    }

    addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("clip_in_timeline"), pCore->window()));
    if (addMenu) {
        m_inTimelineAction = m_menu->addMenu(addMenu);
    }

    if (m_locateAction) {
        m_menu->addAction(m_locateAction);
    }
    if (m_reloadAction) {
        m_menu->addAction(m_reloadAction);
    }
    if (m_replaceAction) {
        m_menu->addAction(m_replaceAction);
    }
    if (m_replaceInTimelineAction) {
        m_menu->addAction(m_replaceInTimelineAction);
    }
    if (m_duplicateAction) {
        m_menu->addAction(m_duplicateAction);
    }
    if (m_transcodeAction) {
        m_menu->addAction(m_transcodeAction);
    }
    if (m_proxyAction) {
        m_menu->addAction(m_proxyAction);
    }

    addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("clip_timeline"), pCore->window()));
    if (addMenu) {
        m_menu->addMenu(addMenu);
        addMenu->setEnabled(false);
    }
    m_menu->addAction(m_editAction);
    m_menu->addAction(m_openAction);
    m_menu->addAction(m_renameAction);
    m_menu->addAction(m_deleteAction);
    m_menu->insertSeparator(m_deleteAction);
}

void Bin::setupMenu()
{
    auto *addClipMenu = new QMenu(this);

    m_addClip =
        addAction(QStringLiteral("add_clip"), i18n("Add Clip or Folder…"), QIcon::fromTheme(QStringLiteral("kdenlive-add-clip")), QStringLiteral("addclip"));
    m_addClip->setWhatsThis(xi18nc("@info:whatsthis", "Main dialog to add source material to your project bin (videos, images, audio, titles, animations).<nl/>"
                                                      "Click on the down-arrow icon to get a list of source types to select from.<nl/>"
                                                      "Click on the media icon to open a window to select source files."));
    addClipMenu->addAction(m_addClip);
    connect(m_addClip, &QAction::triggered, this, &Bin::slotAddClip);

    setupAddClipAction(addClipMenu, ClipType::Color, QStringLiteral("add_color_clip"), i18n("Add Color Clip…"),
                       QIcon::fromTheme(QStringLiteral("kdenlive-add-color-clip")));
    setupAddClipAction(addClipMenu, ClipType::SlideShow, QStringLiteral("add_slide_clip"), i18n("Add Image Sequence…"),
                       QIcon::fromTheme(QStringLiteral("kdenlive-add-slide-clip")));
    setupAddClipAction(addClipMenu, ClipType::Text, QStringLiteral("add_text_clip"), i18n("Add Title Clip…"),
                       QIcon::fromTheme(QStringLiteral("kdenlive-add-text-clip")));
    setupAddClipAction(addClipMenu, ClipType::TextTemplate, QStringLiteral("add_text_template_clip"), i18n("Add Template Title…"),
                       QIcon::fromTheme(QStringLiteral("kdenlive-add-text-clip")));
    setupAddClipAction(addClipMenu, ClipType::Animation, QStringLiteral("add_animation_clip"), i18n("Create Animation…"),
                       QIcon::fromTheme(QStringLiteral("motion_path_animations")));
    setupAddClipAction(addClipMenu, ClipType::Timeline, QStringLiteral("add_playlist_clip"), i18n("Add Sequence…"),
                       QIcon::fromTheme(QStringLiteral("list-add")));
    QAction *downloadResourceAction =
        addAction(QStringLiteral("download_resource"), i18n("Online Resources"), QIcon::fromTheme(QStringLiteral("edit-download")));
    addClipMenu->addAction(downloadResourceAction);
    connect(downloadResourceAction, &QAction::triggered, pCore->window(), &MainWindow::slotDownloadResources);

    m_locateAction = addAction(QStringLiteral("locate_clip"), i18n("Locate Clip…"), QIcon::fromTheme(QStringLiteral("find-location")));
    m_locateAction->setData("locate_clip");
    m_locateAction->setEnabled(false);
    connect(m_locateAction, &QAction::triggered, this, &Bin::slotLocateClip);

    m_reloadAction = addAction(QStringLiteral("reload_clip"), i18n("Reload Clip"), QIcon::fromTheme(QStringLiteral("view-refresh")));
    m_reloadAction->setData("reload_clip");
    m_reloadAction->setEnabled(false);
    connect(m_reloadAction, &QAction::triggered, this, &Bin::slotReloadClip);

    m_transcodeAction =
        addAction(QStringLiteral("friendly_transcoder"), i18n("Transcode to Edit Friendly Format…"), QIcon::fromTheme(QStringLiteral("edit-copy")));
    m_transcodeAction->setData("transcode_clip");
    m_transcodeAction->setEnabled(false);
    connect(m_transcodeAction, &QAction::triggered, this, &Bin::requestSelectionTranscoding);

    m_replaceAction = addAction(QStringLiteral("replace_clip"), i18n("Replace Clip…"), QIcon::fromTheme(QStringLiteral("edit-find-replace")));
    m_replaceAction->setData("replace_clip");
    m_replaceAction->setEnabled(false);
    connect(m_replaceAction, &QAction::triggered, this, &Bin::slotReplaceClip);

    m_replaceInTimelineAction =
        addAction(QStringLiteral("replace_in_timeline"), i18n("Replace Clip In Timeline…"), QIcon::fromTheme(QStringLiteral("edit-find-replace")));
    m_replaceInTimelineAction->setData("replace_timeline_clip");
    m_replaceInTimelineAction->setEnabled(false);
    connect(m_replaceInTimelineAction, &QAction::triggered, this, &Bin::slotReplaceClipInTimeline);

    m_duplicateAction = addAction(QStringLiteral("duplicate_clip"), i18n("Duplicate Clip"), QIcon::fromTheme(QStringLiteral("edit-copy")));
    m_duplicateAction->setData("duplicate_clip");
    m_duplicateAction->setEnabled(false);
    connect(m_duplicateAction, &QAction::triggered, this, &Bin::slotDuplicateClip);

    m_proxyAction = new QAction(i18n("Proxy Clip"), pCore->window());
    pCore->window()->addAction(QStringLiteral("proxy_clip"), m_proxyAction);
    m_proxyAction->setData(QStringList() << QString::number(static_cast<int>(AbstractTask::PROXYJOB)));
    m_proxyAction->setCheckable(true);
    m_proxyAction->setChecked(false);
    m_proxyAction->setEnabled(false);

    m_editAction = addAction(QStringLiteral("clip_properties"), i18n("Clip Properties"), QIcon::fromTheme(QStringLiteral("document-edit")));
    m_editAction->setData("clip_properties");
    m_editAction->setEnabled(false);
    connect(m_editAction, &QAction::triggered, this, static_cast<void (Bin::*)()>(&Bin::slotSwitchClipProperties));

    m_openAction = addAction(QStringLiteral("edit_clip"), i18n("Edit Clip"), QIcon::fromTheme(QStringLiteral("document-open")));
    m_openAction->setData("edit_clip");
    m_openAction->setEnabled(false);
    connect(m_openAction, &QAction::triggered, this, &Bin::slotOpenClipExtern);

    m_renameAction = KStandardAction::renameFile(this, SLOT(slotRenameItem()), this);
    m_renameAction->setEnabled(false);
    if (m_itemView) {
        m_itemView->addAction(m_renameAction);
        m_renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

    m_deleteAction = addAction(QStringLiteral("delete_clip"), i18n("Delete Clip"), QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_deleteAction->setData("delete_clip");
    m_deleteAction->setEnabled(false);
    connect(m_deleteAction, &QAction::triggered, this, &Bin::slotDeleteClip);

    m_openInBin = addAction(QStringLiteral("add_bin"), i18n("Open in new bin"), QIcon::fromTheme(QStringLiteral("document-open")));
    connect(m_openInBin, &QAction::triggered, this, [&]() {
        QModelIndex ix = m_proxyModel->selectionModel()->currentIndex();
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        QString id;
        if (currentItem) {
            id = currentItem->clipId();
        }
        pCore.get()->addBin(id);
    });

    m_sequencesFolderAction =
        addAction(QStringLiteral("sequence_folder"), i18n("Default Target Folder for Sequences"), QIcon::fromTheme(QStringLiteral("favorite")));
    m_sequencesFolderAction->setCheckable(true);
    connect(m_sequencesFolderAction, &QAction::triggered, this, &Bin::setDefaultSequenceFolder);

    m_audioCapturesFolderAction =
        addAction(QStringLiteral("audioCapture_folder"), i18n("Default Target Folder for Audio Captures"), QIcon::fromTheme(QStringLiteral("favorite")));
    m_audioCapturesFolderAction->setCheckable(true);
    connect(m_audioCapturesFolderAction, &QAction::triggered, this, &Bin::setDefaultAudioCaptureFolder);

    m_createFolderAction = addAction(QStringLiteral("create_folder"), i18n("Create Folder"), QIcon::fromTheme(QStringLiteral("folder-new")));
    m_createFolderAction->setWhatsThis(
        xi18nc("@info:whatsthis",
               "Creates a folder in the current position in the project bin. Allows for better organization of source files. Folders can be nested."));
    connect(m_createFolderAction, &QAction::triggered, this, &Bin::slotAddFolder);

    m_upAction = KStandardAction::up(this, SLOT(slotBack()), pCore->window()->actionCollection());

    // Setup actions
    QAction *first = m_toolbar->actions().at(0);
    m_toolbar->insertAction(first, m_deleteAction);
    m_toolbar->insertAction(m_deleteAction, m_createFolderAction);
    m_toolbar->insertAction(m_createFolderAction, m_upAction);

    auto *m = new QMenu(this);
    m->addActions(addClipMenu->actions());
    m_addButton = new QToolButton(this);
    m_addButton->setMenu(m);
    m_addButton->setDefaultAction(m_addClip);
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->insertWidget(m_upAction, m_addButton);
    m_menu = new QMenu(this);
    if (m_isMainBin) {
        m_propertiesDock = pCore->window()->addDock(i18n("Clip Properties"), QStringLiteral("clip_properties"), m_propertiesPanel);
        m_propertiesDock->close();
    }
    connect(m_menu, &QMenu::aboutToShow, this, &Bin::updateTimelineOccurrences);
}

const QString Bin::getDocumentProperty(const QString &key)
{
    return m_doc->getDocumentProperty(key);
}

void Bin::doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QList<QAction *> &actions, bool showCloseButton,
                           BinMessage::BinCategory messageCategory)
{
    // Remove existing actions if any
    QList<QAction *> acts = m_infoMessage->actions();
    while (!acts.isEmpty()) {
        QAction *a = acts.takeFirst();
        m_infoMessage->removeAction(a);
        delete a;
    }
    m_currentMessage = messageCategory;
    m_infoMessage->setText(text);
    m_infoMessage->setWordWrap(text.length() > 35);
    for (QAction *action : actions) {
        m_infoMessage->addAction(action);
        connect(action, &QAction::triggered, this, &Bin::slotMessageActionTriggered);
    }
    m_infoMessage->setCloseButtonVisible(showCloseButton || actions.isEmpty());
    m_infoMessage->setMessageType(type);
    m_infoMessage->animatedShow();
}

void Bin::doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QString logInfo)
{
    // Remove existing actions if any
    m_currentMessage = BinMessage::BinCategory::InformationMessage;
    QList<QAction *> acts = m_infoMessage->actions();
    while (!acts.isEmpty()) {
        QAction *a = acts.takeFirst();
        m_infoMessage->removeAction(a);
        delete a;
    }
    m_infoMessage->setText(text);
    m_infoMessage->setWordWrap(text.length() > 35);
    QAction *ac = new QAction(i18n("Show log"), this);
    m_infoMessage->addAction(ac);
    connect(ac, &QAction::triggered, this, [this, logInfo](bool) {
        KMessageBox::error(this, logInfo, i18n("Detailed log"));
        slotMessageActionTriggered();
    });
    m_infoMessage->setCloseButtonVisible(false);
    m_infoMessage->setMessageType(type);
    m_infoMessage->animatedShow();
}

void Bin::refreshClip(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
        if (pCore->monitorManager()->clipMonitorVisible()) {
            m_monitor->refreshMonitor(true);
        }
    }
}

void Bin::slotCreateProjectClip()
{
    auto *act = qobject_cast<QAction *>(sender());
    if (act == nullptr) {
        // Cannot access triggering action, something is wrong
        qCDebug(KDENLIVE_LOG) << "// Error in clip creation action";
        return;
    }
    ClipType::ProducerType type = ClipType::ProducerType(act->data().toInt());
    QString parentFolder = getCurrentFolder();
    switch (type) {
    case ClipType::Color:
        ClipCreationDialog::createColorClip(m_doc, parentFolder, m_itemModel);
        break;
    case ClipType::SlideShow:
        ClipCreationDialog::createSlideshowClip(m_doc, parentFolder, m_itemModel);
        break;
    case ClipType::Text:
        ClipCreationDialog::createTitleClip(m_doc, parentFolder, QString(), m_itemModel);
        break;
    case ClipType::TextTemplate:
        ClipCreationDialog::createTitleTemplateClip(m_doc, parentFolder, m_itemModel);
        break;
    case ClipType::QText:
        ClipCreationDialog::createQTextClip(parentFolder, this);
        break;
    case ClipType::Animation:
        ClipCreationDialog::createAnimationClip(m_doc, parentFolder);
        break;
    case ClipType::Timeline:
        buildSequenceClip();
        break;
    default:
        break;
    }
    pCore->window()->raiseBin();
}

void Bin::buildSequenceClip(int aTracks, int vTracks)
{
    QScopedPointer<QDialog> dia(new QDialog(this));
    Ui::NewTimeline_UI dia_ui;
    dia_ui.setupUi(dia.data());
    dia->setWindowTitle(i18nc("@title:window", "Create New Sequence"));
    int timelinesCount = pCore->projectManager()->getTimelinesCount() + 1;
    const QStringList seqNames = m_doc->getSequenceNames();
    QString newSeqName = i18n("Sequence %1", timelinesCount);
    while (seqNames.contains(newSeqName)) {
        timelinesCount++;
        newSeqName = i18n("Sequence %1", timelinesCount);
    }
    dia_ui.sequence_name->setText(newSeqName);
    dia_ui.video_tracks->setValue(vTracks == -1 ? KdenliveSettings::videotracks() : vTracks);
    dia_ui.audio_tracks->setValue(aTracks == -1 ? KdenliveSettings::audiotracks() : aTracks);
    dia_ui.sequence_name->setFocus();
    dia_ui.sequence_name->selectAll();
    if (dia->exec() == QDialog::Accepted) {
        int videoTracks = dia_ui.video_tracks->value();
        int audioTracks = dia_ui.audio_tracks->value();
        QString parentFolder = getCurrentFolder();
        if (m_itemModel->defaultSequencesFolder() > -1) {
            const QString sequenceFolder = QString::number(m_itemModel->defaultSequencesFolder());
            std::shared_ptr<ProjectFolder> folderItem = m_itemModel->getFolderByBinId(sequenceFolder);
            if (folderItem) {
                parentFolder = sequenceFolder;
            }
        }
        ClipCreationDialog::createPlaylistClip(dia_ui.sequence_name->text(), {audioTracks, videoTracks}, parentFolder, m_itemModel);
    }
}

const QString Bin::buildSequenceClipWithUndo(Fun &undo, Fun &redo, int aTracks, int vTracks, QString suggestedName)
{
    QScopedPointer<QDialog> dia(new QDialog(this));
    Ui::NewTimeline_UI dia_ui;
    dia_ui.setupUi(dia.data());
    dia->setWindowTitle(i18nc("@title:window", "Create New Sequence"));
    if (suggestedName.isEmpty()) {
        int timelinesCount = pCore->projectManager()->getTimelinesCount() + 1;
        suggestedName = i18n("Sequence %1", timelinesCount);
    }
    dia_ui.sequence_name->setText(suggestedName);
    dia_ui.video_tracks->setValue(vTracks == -1 ? KdenliveSettings::videotracks() : vTracks);
    dia_ui.audio_tracks->setValue(aTracks == -1 ? KdenliveSettings::audiotracks() : aTracks);
    if (dia->exec() == QDialog::Accepted) {
        int videoTracks = dia_ui.video_tracks->value();
        int audioTracks = dia_ui.audio_tracks->value();
        QString parentFolder = getCurrentFolder();
        if (m_itemModel->defaultSequencesFolder() > -1) {
            const QString sequenceFolder = QString::number(m_itemModel->defaultSequencesFolder());
            std::shared_ptr<ProjectFolder> folderItem = m_itemModel->getFolderByBinId(sequenceFolder);
            if (folderItem) {
                parentFolder = sequenceFolder;
            }
        }
        return ClipCreator::createPlaylistClipWithUndo(dia_ui.sequence_name->text(), {audioTracks, videoTracks}, parentFolder, m_itemModel, undo, redo);
    }
    return QString();
}

void Bin::slotItemDropped(const QStringList ids, const QModelIndex parent)
{
    std::shared_ptr<AbstractProjectItem> parentItem;
    if (parent.isValid()) {
        parentItem = m_itemModel->getBinItemByIndex(parent);
        parentItem = parentItem->getEnclosingFolder(false);
    } else {
        parentItem = m_itemModel->getRootFolder();
    }
    auto *moveCommand = new QUndoCommand();
    moveCommand->setText(i18np("Move Clip", "Move Clips", ids.count()));
    QStringList folderIds;
    QMap<QString, std::pair<QString, QString>> idsMap;
    for (const QString &id : ids) {
        if (id.contains(QLatin1Char('/'))) {
            // trying to move clip zone, not allowed. Ignore
            continue;
        }
        if (id.startsWith(QLatin1Char('#'))) {
            // moving a folder, keep it for later
            folderIds << id;
            continue;
        }
        std::shared_ptr<ProjectClip> currentItem = m_itemModel->getClipByBinID(id);
        if (!currentItem) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> currentParent = currentItem->parent();
        if (currentParent != parentItem) {
            // Item was dropped on a different folder
            idsMap.insert(id, {parentItem->clipId(), currentParent->clipId()});
        }
    }
    if (idsMap.count() > 0) {
        new MoveBinClipCommand(this, idsMap, moveCommand);
    }

    if (!folderIds.isEmpty()) {
        for (QString id : qAsConst(folderIds)) {
            id.remove(0, 1);
            std::shared_ptr<ProjectFolder> currentItem = m_itemModel->getFolderByBinId(id);
            if (!currentItem || currentItem == parentItem) {
                continue;
            }
            std::shared_ptr<AbstractProjectItem> currentParent = currentItem->parent();
            if (currentParent != parentItem) {
                // Item was dropped on a different folder
                new MoveBinFolderCommand(this, id, currentParent->clipId(), parentItem->clipId(), moveCommand);
            }
        }
    }
    if (moveCommand->childCount() == 0) {
        pCore->displayMessage(i18n("No valid clip to insert"), MessageType::ErrorMessage, 500);
    } else {
        m_doc->commandStack()->push(moveCommand);
    }
}

void Bin::slotAddEffect(std::vector<QString> ids, const QStringList &effectData)
{
    if (ids.size() == 0) {
        // Apply effect to all selected clips
        ids = selectedClipsIds();
    }
    if (ids.size() == 0) {
        pCore->displayMessage(i18n("Select a clip to apply an effect"), MessageType::ErrorMessage, 500);
    }
    for (auto &id : ids) {
        std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
        if (clip) {
            if (effectData.count() == 5) {
                // Paste effect from another stack
                std::shared_ptr<EffectStackModel> sourceStack =
                    pCore->getItemEffectStack(QUuid(effectData.at(4)), effectData.at(1).toInt(), effectData.at(2).toInt());
                clip->copyEffect(sourceStack, effectData.at(3).toInt());
            } else {
                clip->addEffect(effectData.constFirst());
            }
        }
    }
}

void Bin::slotEffectDropped(const QStringList &effectData, const QModelIndex &parent)
{
    if (parent.isValid()) {
        std::shared_ptr<AbstractProjectItem> parentItem = m_itemModel->getBinItemByIndex(parent);
        if (parentItem->itemType() == AbstractProjectItem::FolderItem) {
            // effect not supported on folder items
            Q_EMIT displayBinMessage(i18n("Cannot apply effects on folders"), KMessageWidget::Information);
            return;
        }
        int row = 0;
        QModelIndex parentIndex;
        if (parentItem->itemType() == AbstractProjectItem::SubClipItem) {
            // effect only supported on clip items
            parentItem = std::static_pointer_cast<ProjectSubClip>(parentItem)->getMasterClip();
            QModelIndex ix = m_itemModel->getIndexFromItem(parentItem);
            row = ix.row();
            parentIndex = ix.parent();
        } else if (parentItem->itemType() == AbstractProjectItem::ClipItem) {
            // effect only supported on clip items
            row = parent.row();
            parentIndex = parent.parent();
        }
        bool res = false;
        if (effectData.count() == 5) {
            // Paste effect from another stack
            std::shared_ptr<EffectStackModel> sourceStack =
                pCore->getItemEffectStack(QUuid(effectData.at(4)), effectData.at(1).toInt(), effectData.at(2).toInt());
            res = std::static_pointer_cast<ProjectClip>(parentItem)->copyEffect(sourceStack, effectData.at(3).toInt());
        } else {
            res = std::static_pointer_cast<ProjectClip>(parentItem)->addEffect(effectData.constFirst());
        }
        if (!res) {
            pCore->displayMessage(i18n("Cannot add effect to clip"), MessageType::ErrorMessage);
        } else {
            m_proxyModel->selectionModel()->clearSelection();
            const QModelIndex id = m_itemModel->index(row, 0, parentIndex);
            const QModelIndex id2 = m_itemModel->index(row, m_itemModel->columnCount() - 1, parentIndex);
            if (id.isValid() && id2.isValid()) {
                m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)),
                                                       QItemSelectionModel::Select);
            }
            setCurrent(parentItem);
        }
    }
}

void Bin::slotTagDropped(const QString &tag, const QModelIndex &parent)
{
    if (parent.isValid()) {
        std::shared_ptr<AbstractProjectItem> parentItem = m_itemModel->getBinItemByIndex(parent);
        if (parentItem->itemType() == AbstractProjectItem::ClipItem || parentItem->itemType() == AbstractProjectItem::SubClipItem) {
            if (parentItem->itemType() == AbstractProjectItem::SubClipItem) {
                qDebug() << "TAG DROPPED ON CLIPZOINE\n\n!!!!!!!!!!!!!!!";
            }
            // effect only supported on clip/subclip items
            QString currentTag = parentItem->tags();
            QMap<QString, QString> oldProps;
            oldProps.insert(QStringLiteral("kdenlive:tags"), currentTag);
            QMap<QString, QString> newProps;
            if (currentTag.isEmpty()) {
                currentTag = tag;
            } else if (!currentTag.contains(tag)) {
                currentTag.append(QStringLiteral(";") + tag);
            }
            newProps.insert(QStringLiteral("kdenlive:tags"), currentTag);
            slotEditClipCommand(parentItem->clipId(), oldProps, newProps);
            return;
        }
        if (parentItem->itemType() == AbstractProjectItem::FolderItem) {
            QList<QString> allClips;
            QList<std::shared_ptr<ProjectClip>> children = std::static_pointer_cast<ProjectFolder>(parentItem)->childClips();
            for (auto &clp : children) {
                allClips << clp->clipId();
            }
            editTags(allClips, tag, true);
            return;
        }
    }
    pCore->displayMessage(i18n("Select a clip to add a tag"), MessageType::ErrorMessage);
}

void Bin::switchTag(const QString &tag, bool add)
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        pCore->displayMessage(i18n("Select a clip to add a tag"), MessageType::ErrorMessage);
    }
    // Check for folders
    QList<QString> allClips;
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> parentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        if (parentItem->itemType() == AbstractProjectItem::FolderItem) {
            QList<std::shared_ptr<ProjectClip>> children = std::static_pointer_cast<ProjectFolder>(parentItem)->childClips();
            for (auto &clp : children) {
                allClips << clp->clipId();
            }
        } else {
            allClips << parentItem->clipId();
        }
    }
    editTags(allClips, tag, add);
}

void Bin::updateTags(const QMap<int, QStringList> &previousTags, const QMap<int, QStringList> &tags)
{
    Fun undo = [this, previousTags, tags]() {
        m_tagsWidget->rebuildTags(previousTags);
        rebuildFilters(previousTags.size());
        pCore->updateProjectTags(tags.size(), previousTags);
        return true;
    };
    Fun redo = [this, previousTags, tags]() {
        m_tagsWidget->rebuildTags(tags);
        rebuildFilters(tags.size());
        pCore->updateProjectTags(previousTags.size(), tags);
        return true;
    };
    // Check if some tags were removed
    QList<QStringList> previous = previousTags.values();
    QList<QStringList> updated = tags.values();
    QStringList deletedTags;
    QMap<QString, QString> modifiedTags;
    for (auto p : previous) {
        bool tagExists = false;
        for (auto n : updated) {
            if (n.first() == p.first()) {
                // Tag still exists
                if (n.at(1) != p.at(1)) {
                    // Tag color has changed
                    modifiedTags.insert(p.at(1), n.at(1));
                }
                tagExists = true;
                break;
            }
        }
        if (!tagExists) {
            // Tag was removed
            deletedTags << p.at(1);
        }
    }
    if (!deletedTags.isEmpty()) {
        // Remove tag from clips
        for (auto &t : deletedTags) {
            // Find clips with the tag
            const QList<QString> clips = getAllClipsWithTag(t);
            Fun update_tags_redo = [this, clips, t]() {
                for (auto &cid : clips) {
                    std::shared_ptr<ProjectClip> clip = getBinClip(cid);
                    if (clip) {
                        QString tags = clip->tags();
                        QStringList tagsList = tags.split(QLatin1Char(';'));
                        tagsList.removeAll(t);
                        QMap<QString, QString> props;
                        props.insert(QStringLiteral("kdenlive:tags"), tagsList.join(QLatin1Char(';')));
                        slotUpdateClipProperties(cid, props, false);
                    }
                }
                return true;
            };
            Fun update_tags_undo = [this, clips, t]() {
                for (auto &cid : clips) {
                    std::shared_ptr<ProjectClip> clip = getBinClip(cid);
                    if (clip) {
                        QString tags = clip->tags();
                        QStringList tagsList = tags.split(QLatin1Char(';'));
                        if (!tagsList.contains(t)) {
                            tagsList << t;
                        }
                        QMap<QString, QString> props;
                        props.insert(QStringLiteral("kdenlive:tags"), tagsList.join(QLatin1Char(';')));
                        slotUpdateClipProperties(cid, props, false);
                    }
                }
                return true;
            };
            UPDATE_UNDO_REDO(update_tags_redo, update_tags_undo, undo, redo);
        }
    }
    if (!modifiedTags.isEmpty()) {
        // Replace tag in clips
        QMapIterator<QString, QString> i(modifiedTags);
        while (i.hasNext()) {
            // Find clips with the tag
            i.next();
            const QList<QString> clips = getAllClipsWithTag(i.key());
            Fun update_tags_redo = [this, clips, previous = i.key(), updated = i.value()]() {
                for (auto &cid : clips) {
                    std::shared_ptr<ProjectClip> clip = getBinClip(cid);
                    if (clip) {
                        QString tags = clip->tags();
                        QStringList tagsList = tags.split(QLatin1Char(';'));
                        tagsList.removeAll(previous);
                        tagsList << updated;
                        QMap<QString, QString> props;
                        props.insert(QStringLiteral("kdenlive:tags"), tagsList.join(QLatin1Char(';')));
                        slotUpdateClipProperties(cid, props, false);
                    }
                }
                return true;
            };
            Fun update_tags_undo = [this, clips, previous = i.key(), updated = i.value()]() {
                for (auto &cid : clips) {
                    std::shared_ptr<ProjectClip> clip = getBinClip(cid);
                    if (clip) {
                        QString tags = clip->tags();
                        QStringList tagsList = tags.split(QLatin1Char(';'));
                        tagsList.removeAll(updated);
                        if (!tagsList.contains(previous)) {
                            tagsList << previous;
                        }
                        QMap<QString, QString> props;
                        props.insert(QStringLiteral("kdenlive:tags"), tagsList.join(QLatin1Char(';')));
                        slotUpdateClipProperties(cid, props, false);
                    }
                }
                return true;
            };
            UPDATE_UNDO_REDO(update_tags_redo, update_tags_undo, undo, redo);
        }
    }
    redo();
    pCore->pushUndo(undo, redo, i18n("Edit Tags"));
}

const QList<QString> Bin::getAllClipsWithTag(const QString &tag)
{
    QList<QString> list;
    QList<std::shared_ptr<ProjectClip>> allClipIds = m_itemModel->getRootFolder()->childClips();
    for (const auto &clip : qAsConst(allClipIds)) {
        if (clip->tags().contains(tag)) {
            list << clip->clipId();
        }
    }
    return list;
}

void Bin::editTags(const QList<QString> &allClips, const QString &tag, bool add)
{
    for (const QString &id : allClips) {
        std::shared_ptr<AbstractProjectItem> clip = m_itemModel->getItemByBinId(id);
        if (clip) {
            // effect only supported on clip/subclip items
            QString currentTag = clip->tags();
            QMap<QString, QString> oldProps;
            oldProps.insert(QStringLiteral("kdenlive:tags"), currentTag);
            QMap<QString, QString> newProps;
            if (add) {
                if (currentTag.isEmpty()) {
                    currentTag = tag;
                } else if (!currentTag.contains(tag)) {
                    currentTag.append(QStringLiteral(";") + tag);
                }
                newProps.insert(QStringLiteral("kdenlive:tags"), currentTag);
            } else {
                QStringList tags = currentTag.split(QLatin1Char(';'));
                tags.removeAll(tag);
                newProps.insert(QStringLiteral("kdenlive:tags"), tags.join(QLatin1Char(';')));
            }
            slotEditClipCommand(id, oldProps, newProps);
        }
    }
}

void Bin::editMasterEffect(const std::shared_ptr<AbstractProjectItem> &clip)
{
    if (m_gainedFocus) {
        // Widget just gained focus, updating stack is managed in the eventfilter event, not from item
        return;
    }
    if (clip) {
        if (m_monitor->activeClipId() != clip->clipId()) {
            setCurrent(clip);
        }
        if (clip->itemType() == AbstractProjectItem::ClipItem) {
            std::shared_ptr<ProjectClip> clp = std::static_pointer_cast<ProjectClip>(clip);
            Q_EMIT requestShowEffectStack(clp->clipName(), clp->m_effectStack, clp->getFrameSize(), false);
            return;
        }
        if (clip->itemType() == AbstractProjectItem::SubClipItem) {
            if (auto ptr = clip->parentItem().lock()) {
                std::shared_ptr<ProjectClip> clp = std::static_pointer_cast<ProjectClip>(ptr);
                Q_EMIT requestShowEffectStack(clp->clipName(), clp->m_effectStack, clp->getFrameSize(), false);
            }
            return;
        }
    }
    Q_EMIT requestShowEffectStack(QString(), nullptr, QSize(), false);
}

void Bin::slotGotFocus()
{
    m_gainedFocus = true;
}

void Bin::blockBin(bool block)
{
    m_proxyModel->selectionModel()->blockSignals(block);
}

void Bin::doMoveClips(QMap<QString, std::pair<QString, QString>> ids, bool redo)
{
    const QString clipId = m_monitor->activeClipId();
    // Don't update selection in all bins while moving clips
    if (pCore->window()) {
        pCore->window()->blockBins(true);
    }
    QMapIterator<QString, std::pair<QString, QString>> i(ids);
    qDebug() << ":::: MOVING CLIPS: " << ids.count();
    while (i.hasNext()) {
        i.next();
        std::shared_ptr<ProjectClip> currentItem = m_itemModel->getClipByBinID(i.key());
        if (!currentItem) {
            continue;
        }
        std::shared_ptr<ProjectFolder> newParent = m_itemModel->getFolderByBinId(redo ? i.value().first : i.value().second);
        if (newParent) {
            currentItem->changeParent(newParent);
        }
    }
    if (pCore->window()) {
        pCore->window()->blockBins(false);
    }
    if (!clipId.isEmpty()) {
        selectClipById(clipId);
    }
}

void Bin::doMoveFolder(const QString &id, const QString &newParentId)
{
    std::shared_ptr<ProjectFolder> currentItem = m_itemModel->getFolderByBinId(id);
    std::shared_ptr<AbstractProjectItem> currentParent = currentItem->parent();
    std::shared_ptr<ProjectFolder> newParent = m_itemModel->getFolderByBinId(newParentId);
    currentParent->removeChild(currentItem);
    currentItem->changeParent(newParent);
}

void Bin::droppedUrls(const QList<QUrl> &urls, const QString &folderInfo)
{
    QModelIndex current;
    if (folderInfo.isEmpty()) {
        current = m_proxyModel->mapToSource(m_proxyModel->selectionModel()->currentIndex());
    } else {
        // get index for folder
        std::shared_ptr<ProjectFolder> folder = m_itemModel->getFolderByBinId(folderInfo);
        if (!folder) {
            folder = m_itemModel->getRootFolder();
        }
        current = m_itemModel->getIndexFromItem(folder);
    }
    slotUrlsDropped(urls, current);
}

const QString Bin::slotAddClipToProject(const QUrl &url)
{
    QList<QUrl> urls;
    urls << url;
    QModelIndex current = m_proxyModel->mapToSource(m_proxyModel->selectionModel()->currentIndex());
    return slotUrlsDropped(urls, current);
}

const QString Bin::slotUrlsDropped(const QList<QUrl> urls, const QModelIndex parent)
{
    QString parentFolder = m_itemModel->getRootFolder()->clipId();
    if (parent.isValid()) {
        // Check if drop occurred on a folder
        std::shared_ptr<AbstractProjectItem> parentItem = m_itemModel->getBinItemByIndex(parent);
        while (parentItem->itemType() != AbstractProjectItem::FolderItem) {
            parentItem = parentItem->parent();
        }
        parentFolder = parentItem->clipId();
    }
    const QString id = ClipCreator::createClipsFromList(urls, true, parentFolder, m_itemModel);
    if (!id.isEmpty()) {
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getItemByBinId(id);
        if (item) {
            QModelIndex ix = m_itemModel->getIndexFromItem(item);
            m_itemView->scrollTo(m_proxyModel->mapFromSource(ix), QAbstractItemView::EnsureVisible);
        }
    }
    return id;
}

void Bin::slotExpandUrl(const ItemInfo &info, const QString &url, QUndoCommand *command)
{
    Q_UNUSED(info)
    Q_UNUSED(url)
    Q_UNUSED(command)
    // TODO reimplement this
    /*
    // Create folder to hold imported clips
    QString folderName = QFileInfo(url).fileName().section(QLatin1Char('.'), 0, 0);
    QString folderId = QString::number(getFreeFolderId());
    new AddBinFolderCommand(this, folderId, folderName.isEmpty() ? i18n("Folder") : folderName, m_itemModel->getRootFolder()->clipId(), false, command);

    // Parse playlist clips
    QDomDocument doc;
    QFile file(url);
    doc.setContent(&file, false); //TODO: use Xml::docContentFromFile
    file.close();
    bool invalid = false;
    if (doc.documentElement().isNull()) {
        invalid = true;
    }
    QDomNodeList producers = doc.documentElement().elementsByTagName(QStringLiteral("producer"));
    QDomNodeList tracks = doc.documentElement().elementsByTagName(QStringLiteral("track"));
    if (invalid || producers.isEmpty()) {
        doDisplayMessage(i18n("Playlist clip %1 is invalid.", QFileInfo(url).fileName()), KMessageWidget::Warning);
        delete command;
        return;
    }
    if (tracks.count() > pCore->projectManager()->currentTimeline()->visibleTracksCount() + 1) {
        doDisplayMessage(
            i18n("Playlist clip %1 has too many tracks (%2) to be imported. Add new tracks to your project.", QFileInfo(url).fileName(), tracks.count()),
            KMessageWidget::Warning);
        delete command;
        return;
    }
    // Maps playlist producer IDs to (project) bin producer IDs.
    QMap<QString, QString> idMap;
    // Maps hash IDs to (project) first playlist producer instance ID. This is
    // necessary to detect duplicate producer serializations produced by MLT.
    // This covers, for instance, images and titles.
    QMap<QString, QString> hashToIdMap;
    QDir mltRoot(doc.documentElement().attribute(QStringLiteral("root")));
    for (int i = 0; i < producers.count(); i++) {
        QDomElement prod = producers.at(i).toElement();
        QString originalId = prod.attribute(QStringLiteral("id"));
        // track producer
        if (originalId.contains(QLatin1Char('_'))) {
            originalId = originalId.section(QLatin1Char('_'), 0, 0);
        }
        // slowmotion producer
        if (originalId.contains(QLatin1Char(':'))) {
            originalId = originalId.section(QLatin1Char(':'), 1, 1);
        }

        // We already have seen and mapped this producer.
        if (idMap.contains(originalId)) {
            continue;
        }

        // Check for duplicate producers, based on hash value of producer.
        // Be careful as to the kdenlive:file_hash! It is not unique for
        // title clips, nor color clips. Also not sure about image sequences.
        // So we use mlt service-specific hashes to identify duplicate producers.
        QString hash;
        QString mltService = Xml::getXmlProperty(prod, QStringLiteral("mlt_service"));
        if (mltService == QLatin1String("pixbuf") || mltService == QLatin1String("qimage") || mltService == QLatin1String("kdenlivetitle") ||
            mltService == QLatin1String("color") || mltService == QLatin1String("colour")) {
            hash = mltService + QLatin1Char(':') + Xml::getXmlProperty(prod, QStringLiteral("kdenlive:clipname")) + QLatin1Char(':') +
                   Xml::getXmlProperty(prod, QStringLiteral("kdenlive:folderid")) + QLatin1Char(':');
            if (mltService == QLatin1String("kdenlivetitle")) {
                // Calculate hash based on title contents.
                hash.append(
                    QString(QCryptographicHash::hash(Xml::getXmlProperty(prod, QStringLiteral("xmldata")).toUtf8(), QCryptographicHash::Md5).toHex()));
            } else if (mltService == QLatin1String("pixbuf") || mltService == QLatin1String("qimage") || mltService == QLatin1String("color") ||
                       mltService == QLatin1String("colour")) {
                hash.append(Xml::getXmlProperty(prod, QStringLiteral("resource")));
            }

            QString singletonId = hashToIdMap.value(hash, QString());
            if (singletonId.length() != 0) {
                // map duplicate producer ID to single bin clip producer ID.
                qCDebug(KDENLIVE_LOG) << "found duplicate producer:" << hash << ", reusing newID:" << singletonId;
                idMap.insert(originalId, singletonId);
                continue;
            }
        }

        // First occurrence of a producer, so allocate new bin clip producer ID.
        QString newId = QString::number(getFreeClipId());
        idMap.insert(originalId, newId);
        qCDebug(KDENLIVE_LOG) << "originalId: " << originalId << ", newId: " << newId;

        // Ensure to register new bin clip producer ID in hash hashmap for
        // those clips that MLT likes to serialize multiple times. This is
        // indicated by having a hash "value" unqual "". See also above.
        if (hash.length() != 0) {
            hashToIdMap.insert(hash, newId);
        }

        // Add clip
        QDomElement clone = prod.cloneNode(true).toElement();
        EffectsList::setProperty(clone, QStringLiteral("kdenlive:folderid"), folderId);
        // Do we have a producer that uses a resource property that contains a path?
        if (mltService == QLatin1String("avformat-novalidate") // av clip
            || mltService == QLatin1String("avformat")         // av clip
            || mltService == QLatin1String("pixbuf")           // image (sequence) clip
            || mltService == QLatin1String("qimage")           // image (sequence) clip
            || mltService == QLatin1String("xml")              // MLT playlist clip, someone likes recursion :)
            ) {
            // Make sure to correctly resolve relative resource paths based on
            // the playlist's root, not on this project's root
            QString resource = Xml::getXmlProperty(clone, QStringLiteral("resource"));
            if (QFileInfo(resource).isRelative()) {
                QFileInfo rootedResource(mltRoot, resource);
                qCDebug(KDENLIVE_LOG) << "fixed resource path for producer, newId:" << newId << "resource:" << rootedResource.absoluteFilePath();
                EffectsList::setProperty(clone, QStringLiteral("resource"), rootedResource.absoluteFilePath());
            }
        }

        ClipCreationDialog::createClipsCommand(this, clone, newId, command);
    }
    pCore->projectManager()->currentTimeline()->importPlaylist(info, idMap, doc, command);
    */
}

void Bin::slotItemEdited(const QModelIndex &ix, const QModelIndex &, const QVector<int> &roles)
{
    if (ix.isValid() && roles.contains(AbstractProjectItem::DataName)) {
        // Clip renamed
        std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(ix);
        auto clip = std::static_pointer_cast<ProjectClip>(item);
        if (clip) {
            Q_EMIT clipNameChanged(clip->AbstractProjectItem::clipId().toInt(), clip->clipName());
        }
    }
}

void Bin::renameSubClipCommand(const QString &id, const QString &newName, const QString &oldName, int in, int out)
{
    auto *command = new RenameBinSubClipCommand(this, id, newName, oldName, in, out);
    m_doc->commandStack()->push(command);
}

void Bin::renameSubClip(const QString &id, const QString &newName, int in, int out)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (!clip) {
        return;
    }
    std::shared_ptr<ProjectSubClip> sub = clip->getSubClip(in, out);
    if (!sub) {
        return;
    }
    sub->setName(newName.isEmpty() ? i18n("Unnamed") : newName);
    clip->updateZones();
    Q_EMIT itemUpdated(sub);
}

void Bin::slotStartFilterJob(const ItemInfo &info, const QString &id, QMap<QString, QString> &filterParams, QMap<QString, QString> &consumerParams,
                             QMap<QString, QString> &extraParams)
{
    Q_UNUSED(info)
    Q_UNUSED(id)
    Q_UNUSED(filterParams)
    Q_UNUSED(consumerParams)
    Q_UNUSED(extraParams)
    // TODO refac
    /*
    std::shared_ptr<ProjectClip> clip = getBinClip(id);
    if (!clip) {
        return;
    }

    QMap<QString, QString> producerParams = QMap<QString, QString>();
    producerParams.insert(QStringLiteral("producer"), clip->url());
    if (info.cropDuration != GenTime()) {
        producerParams.insert(QStringLiteral("in"), QString::number((int)info.cropStart.frames(pCore->getCurrentFps())));
        producerParams.insert(QStringLiteral("out"), QString::number((int)(info.cropStart + info.cropDuration).frames(pCore->getCurrentFps())));
        extraParams.insert(QStringLiteral("clipStartPos"), QString::number((int)info.startPos.frames(pCore->getCurrentFps())));
        extraParams.insert(QStringLiteral("clipTrack"), QString::number(info.track));
    } else {
        // We want to process whole clip
        producerParams.insert(QStringLiteral("in"), QString::number(0));
        producerParams.insert(QStringLiteral("out"), QString::number(-1));
    }
    */
}

void Bin::focusBinView() const
{
    m_itemView->setFocus();
}

void Bin::slotOpenClipExtern()
{
    std::shared_ptr<ProjectClip> clip = getFirstSelectedClip();
    if (!clip) {
        return;
    }
    QString errorString;
    switch (clip->clipType()) {
    case ClipType::Text:
    case ClipType::TextTemplate:
        showTitleWidget(clip);
        break;
    case ClipType::Image: {
        if (KdenliveSettings::defaultimageapp().isEmpty()) {
            QUrl url = KUrlRequesterDialog::getUrl(QUrl(), this, i18n("Enter path for your image editing application"));
            if (!url.isEmpty()) {
                KdenliveSettings::setDefaultimageapp(url.toLocalFile());
                KdenliveSettingsDialog *d = static_cast<KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
                if (d) {
                    d->updateExternalApps();
                }
            }
        }
        if (!KdenliveSettings::defaultimageapp().isEmpty()) {
            errorString = pCore->openExternalApp(KdenliveSettings::defaultimageapp(), {clip->url()});
        } else {
            KMessageBox::error(QApplication::activeWindow(), i18n("Please set a default application to open image files"));
        }
    } break;
    case ClipType::Audio: {
        if (KdenliveSettings::defaultaudioapp().isEmpty()) {
            QUrl url = KUrlRequesterDialog::getUrl(QUrl(), this, i18n("Enter path for your audio editing application"));
            if (!url.isEmpty()) {
                KdenliveSettings::setDefaultaudioapp(url.toLocalFile());
                KdenliveSettingsDialog *d = static_cast<KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
                if (d) {
                    d->updateExternalApps();
                }
            }
        }
        if (!KdenliveSettings::defaultaudioapp().isEmpty()) {
            errorString = pCore->openExternalApp(KdenliveSettings::defaultaudioapp(), {clip->url()});
        } else {
            KMessageBox::error(QApplication::activeWindow(), i18n("Please set a default application to open audio files"));
        }
    } break;
    case ClipType::Animation: {
        GlaxnimateLauncher::instance().openFile(clip->url());
    } break;
    default:
        break;
    }
    if (!errorString.isEmpty()) {
        KMessageBox::detailedError(QApplication::activeWindow(), i18n("Cannot open file %1", clip->url()), errorString);
    }
}

/*
void Bin::slotGotFilterJobResults(const QString &id, int startPos, int track, const stringMap &results, const stringMap &filterInfo)
{
    if (filterInfo.contains(QStringLiteral("finalfilter"))) {
        if (filterInfo.contains(QStringLiteral("storedata"))) {
            // Store returned data as clip extra data
            std::shared_ptr<ProjectClip> clip = getBinClip(id);
            if (clip) {
                QString key = filterInfo.value(QStringLiteral("key"));
                QStringList newValue = clip->updatedAnalysisData(key, results.value(key), filterInfo.value(QStringLiteral("offset")).toInt());
                slotAddClipExtraData(id, newValue.at(0), newValue.at(1));
            }
        }
        if (startPos == -1) {
            // Processing bin clip
            std::shared_ptr<ProjectClip> currentItem = m_itemModel->getClipByBinID(id);
            if (!currentItem) {
                return;
            }
            std::shared_ptr<ClipController> ctl = std::static_pointer_cast<ClipController>(currentItem);
            EffectsList list = ctl->effectList();
            QDomElement effect = list.effectById(filterInfo.value(QStringLiteral("finalfilter")));
            QDomDocument doc;
            QDomElement e = doc.createElement(QStringLiteral("test"));
            doc.appendChild(e);
            e.appendChild(doc.importNode(effect, true));
            if (!effect.isNull()) {
                QDomElement newEffect = effect.cloneNode().toElement();
                QMap<QString, QString>::const_iterator i = results.constBegin();
                while (i != results.constEnd()) {
                    EffectsList::setParameter(newEffect, i.key(), i.value());
                    ++i;
                }
                ctl->updateEffect(newEffect, effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
                Q_EMIT requestClipShow(currentItem);
                // TODO use undo / redo for bin clip edit effect
                //
                EditEffectCommand *command = new EditEffectCommand(this, clip->track(), clip->startPos(), effect, newEffect, clip->selectedEffectIndex(),
                true, true);
                m_commandStack->push(command);
                Q_EMIT clipItemSelected(clip);
            }

            // Q_EMIT gotFilterJobResults(id, startPos, track, results, filterInfo);
            return;
        }
        // This is a timeline filter, forward results
        Q_EMIT gotFilterJobResults(id, startPos, track, results, filterInfo);
        return;
    }
    // Currently, only the first value of results is used
    std::shared_ptr<ProjectClip> clip = getBinClip(id);
    if (!clip) {
        return;
    }
    // Check for return value
    int markersType = -1;
    if (filterInfo.contains(QStringLiteral("addmarkers"))) {
        markersType = filterInfo.value(QStringLiteral("addmarkers")).toInt();
    }
    if (results.isEmpty()) {
        Q_EMIT displayBinMessage(i18n("No data returned from clip analysis"), KMessageWidget::Warning);
        return;
    }
    bool dataProcessed = false;
    QString label = filterInfo.value(QStringLiteral("label"));
    QString key = filterInfo.value(QStringLiteral("key"));
    int offset = filterInfo.value(QStringLiteral("offset")).toInt();
    QStringList value = results.value(key).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    // qCDebug(KDENLIVE_LOG)<<"// RESULT; "<<key<<" = "<<value;
    if (filterInfo.contains(QStringLiteral("resultmessage"))) {
        QString mess = filterInfo.value(QStringLiteral("resultmessage"));
        mess.replace(QLatin1String("%count"), QString::number(value.count()));
        Q_EMIT displayBinMessage(mess, KMessageWidget::Information);
    } else {
        Q_EMIT displayBinMessage(i18n("Processing data analysis"), KMessageWidget::Information);
    }
    if (filterInfo.contains(QStringLiteral("cutscenes"))) {
        // Check if we want to cut scenes from returned data
        dataProcessed = true;
        int cutPos = 0;
        auto *command = new QUndoCommand();
        command->setText(i18n("Auto Split Clip"));
        for (const QString &pos : value) {
            if (!pos.contains(QLatin1Char('='))) {
                continue;
            }
            int newPos = pos.section(QLatin1Char('='), 0, 0).toInt();
            // Don't use scenes shorter than 1 second
            if (newPos - cutPos < 24) {
                continue;
            }
            new AddBinClipCutCommand(this, id, cutPos + offset, newPos + offset, true, command);
            cutPos = newPos;
        }
        if (command->childCount() == 0) {
            delete command;
        } else {
            m_doc->commandStack()->push(command);
        }
    }
    if (markersType >= 0) {
        // Add markers from returned data
        dataProcessed = true;
        int cutPos = 0;
        int index = 1;
        bool simpleList = false;
        double sourceFps = clip->getOriginalFps();
        if (qFuzzyIsNull(sourceFps)) {
            sourceFps = pCore->getCurrentFps();
        }
        if (filterInfo.contains(QStringLiteral("simplelist"))) {
            // simple list
            simpleList = true;
        }
        for (const QString &pos : value) {
            if (simpleList) {
                clip->getMarkerModel()->addMarker(GenTime((int)(pos.toInt() * pCore->getCurrentFps() / sourceFps), pCore->getCurrentFps()), label + pos,
                                                  markersType);
                index++;
                continue;
            }
            if (!pos.contains(QLatin1Char('='))) {
                continue;
            }
            int newPos = pos.section(QLatin1Char('='), 0, 0).toInt();
            // Don't use scenes shorter than 1 second
            if (newPos - cutPos < 24) {
                continue;
            }
            clip->getMarkerModel()->addMarker(GenTime(newPos + offset, pCore->getCurrentFps()), label + QString::number(index), markersType);
            index++;
            cutPos = newPos;
        }
    }
    if (!dataProcessed || filterInfo.contains(QStringLiteral("storedata"))) {
        // Store returned data as clip extra data
        QStringList newValue = clip->updatedAnalysisData(key, results.value(key), offset);
        slotAddClipExtraData(id, newValue.at(0), newValue.at(1));
    }
}
*/

// TODO: move title editing into a better place...
void Bin::showTitleWidget(const std::shared_ptr<ProjectClip> &clip)
{
    QString path = clip->getProducerProperty(QStringLiteral("resource"));
    QDir titleFolder(m_doc->projectDataFolder() + QStringLiteral("/titles"));
    titleFolder.mkpath(QStringLiteral("."));
    QList<int> clips = clip->timelineInstances();
    // Temporarily hide this title clip in timeline so that it does not appear when requesting background frame
    pCore->temporaryUnplug(clips, true);
    TitleWidget dia_ui(QUrl(), titleFolder.absolutePath(), pCore->monitorManager()->projectMonitor(), pCore->window());
    QDomDocument doc;
    QString xmldata = clip->getProducerProperty(QStringLiteral("xmldata"));
    if (xmldata.isEmpty() && QFile::exists(path)) {
        if (!Xml::docContentFromFile(doc, path, false)) {
            return;
        }
    } else {
        doc.setContent(xmldata);
    }
    dia_ui.setXml(doc, clip->clipId());
    int res = dia_ui.exec();
    if (res == QDialog::Accepted) {
        pCore->temporaryUnplug(clips, false);
        QMap<QString, QString> newprops;
        newprops.insert(QStringLiteral("xmldata"), dia_ui.xml().toString());
        if (dia_ui.duration() != clip->duration().frames(pCore->getCurrentFps())) {
            // duration changed, we need to update duration
            newprops.insert(QStringLiteral("out"), clip->framesToTime(dia_ui.duration() - 1));
            int currentLength = clip->getProducerDuration();
            if (currentLength != dia_ui.duration()) {
                newprops.insert(QStringLiteral("kdenlive:duration"), clip->framesToTime(dia_ui.duration()));
            }
        }
        if (clip->clipName().contains(i18n("(copy)"))) {
            // We edited a duplicated title clip, update name from new content text
            newprops.insert(QStringLiteral("kdenlive:clipname"), dia_ui.titleSuggest());
            if (!path.isEmpty()) {
                newprops.insert(QStringLiteral("resource"), QString());
            }
        }
        if (!path.isEmpty()) {
            // we are editing an external file, asked if we want to detach from that file or save the result to that title file.
            if (KMessageBox::questionTwoActions(pCore->window(),
                                                i18n("You are editing an external title clip (%1). Do you want to save your changes to the title "
                                                     "file or save the changes for this project only?",
                                                     path),
                                                i18n("Save Title"), KGuiItem(i18n("Save to title file")),
                                                KGuiItem(i18n("Save in project only"))) == KMessageBox::PrimaryAction) {
                // save to external file
                dia_ui.saveTitle(QUrl::fromLocalFile(path));
                return;
            } else {
                newprops.insert(QStringLiteral("resource"), QString());
            }
        }
        // trigger producer reload
        newprops.insert(QStringLiteral("force_reload"), QStringLiteral("1"));
        slotEditClipCommand(clip->AbstractProjectItem::clipId(), clip->currentProperties(newprops), newprops);
        // when edit is triggered from the timeline, project monitor refresh is necessary after an edit is made
        pCore->refreshProjectMonitorOnce();
    } else {
        pCore->temporaryUnplug(clips, false);
        if (res == QDialog::Accepted + 1) {
            // Ready, create clip xml
            std::unordered_map<QString, QString> properties;
            properties[QStringLiteral("xmldata")] = dia_ui.xml().toString();
            QString titleSuggestion = dia_ui.titleSuggest();
            ClipCreator::createTitleClip(properties, dia_ui.duration(), titleSuggestion.isEmpty() ? i18n("Title clip") : titleSuggestion,
                                         clip->parent()->clipId(), m_itemModel);
        }
    }
}

void Bin::slotResetInfoMessage()
{
    m_errorLog.clear();
    m_currentMessage = BinMessage::BinCategory::NoMessage;
    // We cannot delete actions here because of concurrency, it might delete actions meant for the upcoming message
    /*QList<QAction *> actions = m_infoMessage->actions();
    for (int i = 0; i < actions.count(); ++i) {
        m_infoMessage->removeAction(actions.at(i));
    }*/
}

void Bin::slotSetSorting()
{
    if (m_listType == BinIconView) {
        m_proxyModel->setFilterKeyColumn(0);
        return;
    }
    auto *view = qobject_cast<QTreeView *>(m_itemView);
    if (view) {
        int ix = view->header()->sortIndicatorSection();
        m_proxyModel->setFilterKeyColumn(ix);
    }
}

void Bin::slotShowColumn(bool show)
{
    auto *act = qobject_cast<QAction *>(sender());
    if (act == nullptr) {
        return;
    }
    auto *view = qobject_cast<QTreeView *>(m_itemView);
    if (view) {
        view->setColumnHidden(act->data().toInt(), !show);
    }
}

void Bin::slotQueryRemoval(const QString &id, const QString &url, const QString &errorMessage)
{
    if (m_invalidClipDialog) {
        if (!url.isEmpty()) {
            m_invalidClipDialog->addClip(id, url);
        }
        return;
    }
    QString message = i18n("Clip is invalid, will be removed from project.");
    if (!errorMessage.isEmpty()) {
        message.append("\n" + errorMessage);
    }
    m_invalidClipDialog = new InvalidDialog(i18n("Invalid clip"), message, true, this);
    m_invalidClipDialog->addClip(id, url);
    int result = m_invalidClipDialog->exec();
    if (result == QDialog::Accepted) {
        const QStringList ids = m_invalidClipDialog->getIds();
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        for (const QString &i : ids) {
            auto item = m_itemModel->getClipByBinID(i);
            m_itemModel->requestBinClipDeletion(item, undo, redo);
        }
    }
    delete m_invalidClipDialog;
    m_invalidClipDialog = nullptr;
}

void Bin::slotRefreshClipThumbnail(const QString &id)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (!clip) {
        return;
    }
    ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, id.toInt(), QUuid()), QDomElement(), true, -1, -1, this);
}

void Bin::slotAddClipExtraData(const QString &id, const QString &key, const QString &clipData)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (!clip) {
        return;
    }
    QString oldValue = clip->getProducerProperty(key);
    QMap<QString, QString> oldProps;
    oldProps.insert(key, oldValue);
    QMap<QString, QString> newProps;
    newProps.insert(key, clipData);
    auto *command = new EditClipCommand(this, id, oldProps, newProps, true);
    m_doc->commandStack()->push(command);
}

void Bin::slotUpdateClipProperties(const QString &id, const QMap<QString, QString> &properties, bool refreshPropertiesPanel)
{
    std::shared_ptr<AbstractProjectItem> item = m_itemModel->getItemByBinId(id);
    if (!item) {
        // Clip might have been deleted
        return;
    }
    if (item->itemType() == AbstractProjectItem::ClipItem) {
        std::shared_ptr<ProjectClip> clip = std::static_pointer_cast<ProjectClip>(item);
        if (clip) {
            clip->setProperties(properties, refreshPropertiesPanel);
        }
    } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
        std::shared_ptr<ProjectSubClip> clip = std::static_pointer_cast<ProjectSubClip>(item);
        if (clip) {
            clip->setProperties(properties);
        }
    }
}

void Bin::showSlideshowWidget(const std::shared_ptr<ProjectClip> &clip)
{
    QString folder = QFileInfo(clip->url()).absolutePath();
    qCDebug(KDENLIVE_LOG) << " ** * CLIP ABS PATH: " << clip->url() << " = " << folder;
    SlideshowClip *dia = new SlideshowClip(m_doc->timecode(), folder, clip.get(), this);
    if (dia->exec() == QDialog::Accepted) {
        // edit clip properties
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("out"), clip->framesToTime(m_doc->getFramePos(dia->clipDuration()) * dia->imageCount() - 1));
        properties.insert(QStringLiteral("kdenlive:duration"), clip->framesToTime(m_doc->getFramePos(dia->clipDuration()) * dia->imageCount()));
        properties.insert(QStringLiteral("kdenlive:clipname"), dia->clipName());
        properties.insert(QStringLiteral("ttl"), QString::number(m_doc->getFramePos(dia->clipDuration())));
        properties.insert(QStringLiteral("loop"), QString::number(static_cast<int>(dia->loop())));
        properties.insert(QStringLiteral("crop"), QString::number(static_cast<int>(dia->crop())));
        properties.insert(QStringLiteral("fade"), QString::number(static_cast<int>(dia->fade())));
        properties.insert(QStringLiteral("luma_duration"), QString::number(m_doc->getFramePos(dia->lumaDuration())));
        properties.insert(QStringLiteral("luma_file"), dia->lumaFile());
        properties.insert(QStringLiteral("softness"), QString::number(dia->softness()));
        properties.insert(QStringLiteral("animation"), dia->animation());
        properties.insert(QStringLiteral("low-pass"), QString::number(dia->lowPass()));

        QMap<QString, QString> oldProperties;
        oldProperties.insert(QStringLiteral("out"), clip->getProducerProperty(QStringLiteral("out")));
        oldProperties.insert(QStringLiteral("kdenlive:duration"), clip->getProducerProperty(QStringLiteral("kdenlive:duration")));
        oldProperties.insert(QStringLiteral("kdenlive:clipname"), clip->name());
        oldProperties.insert(QStringLiteral("ttl"), clip->getProducerProperty(QStringLiteral("ttl")));
        oldProperties.insert(QStringLiteral("loop"), clip->getProducerProperty(QStringLiteral("loop")));
        oldProperties.insert(QStringLiteral("crop"), clip->getProducerProperty(QStringLiteral("crop")));
        oldProperties.insert(QStringLiteral("fade"), clip->getProducerProperty(QStringLiteral("fade")));
        oldProperties.insert(QStringLiteral("luma_duration"), clip->getProducerProperty(QStringLiteral("luma_duration")));
        oldProperties.insert(QStringLiteral("luma_file"), clip->getProducerProperty(QStringLiteral("luma_file")));
        oldProperties.insert(QStringLiteral("softness"), clip->getProducerProperty(QStringLiteral("softness")));
        oldProperties.insert(QStringLiteral("animation"), clip->getProducerProperty(QStringLiteral("animation")));
        oldProperties.insert(QStringLiteral("low-pass"), clip->getProducerProperty(QStringLiteral("low-pass")));

        slotEditClipCommand(clip->AbstractProjectItem::clipId(), oldProperties, properties);
    }
    delete dia;
}

void Bin::setBinEffectsEnabled(bool enabled, bool refreshMonitor)
{
    m_itemModel->setBinEffectsEnabled(enabled);
    pCore->projectManager()->disableBinEffects(!enabled, refreshMonitor);
}

void Bin::slotRenameItem()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selection().indexes();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        m_itemView->setCurrentIndex(ix);
        m_itemView->edit(ix);
        return;
    }
}

void Bin::refreshProxySettings()
{
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    auto *masterCommand = new QUndoCommand();
    masterCommand->setText(m_doc->useProxy() ? i18n("Enable proxies") : i18n("Disable proxies"));
    // en/disable proxy option in clip properties
    if (m_propertiesPanel) {
        for (QWidget *w : m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
            Q_EMIT static_cast<ClipPropertiesController *>(w)->enableProxy(m_doc->useProxy());
        }
    }
    bool isFolder = false;
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        if (currentItem) {
            AbstractProjectItem::PROJECTITEMTYPE itemType = currentItem->itemType();
            isFolder = itemType == AbstractProjectItem::FolderItem;
        }
        break;
    }
    m_proxyAction->setEnabled(m_doc->useProxy() && !isFolder);
    if (!m_doc->useProxy()) {
        // Disable all proxies
        m_doc->slotProxyCurrentItem(false, clipList, false, masterCommand);
    } else if (m_doc->autoGenerateProxy(-1) || m_doc->autoGenerateImageProxy(-1)) {
        QList<std::shared_ptr<ProjectClip>> toProxy;
        for (const std::shared_ptr<ProjectClip> &clp : qAsConst(clipList)) {
            ClipType::ProducerType t = clp->clipType();
            if (t == ClipType::Playlist && m_doc->autoGenerateProxy(pCore->getCurrentFrameDisplaySize().width())) {
                toProxy << clp;
                continue;
            } else if ((t == ClipType::AV || t == ClipType::Video) &&
                       m_doc->autoGenerateProxy(clp->getProducerIntProperty(QStringLiteral("meta.media.width")))) {
                // Start proxy
                toProxy << clp;
                continue;
            } else if (t == ClipType::Image && m_doc->autoGenerateImageProxy(clp->getProducerIntProperty(QStringLiteral("meta.media.width")))) {
                // Start proxy
                toProxy << clp;
                continue;
            }
        }
        if (!toProxy.isEmpty()) {
            m_doc->slotProxyCurrentItem(true, toProxy, false, masterCommand);
        }
    }
    if (masterCommand->childCount() > 0) {
        m_doc->commandStack()->push(masterCommand);
    } else {
        delete masterCommand;
    }
}

QStringList Bin::getProxyHashList()
{
    QStringList list;
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clp : qAsConst(clipList)) {
        if (clp->clipType() == ClipType::AV || clp->clipType() == ClipType::Video || clp->clipType() == ClipType::Playlist) {
            list << clp->hash();
        }
    }
    return list;
}

bool Bin::hasUserClip() const
{
    QList<std::shared_ptr<ProjectClip>> allClips = m_itemModel->getRootFolder()->childClips();
    for (auto &c : allClips) {
        if (c->clipType() != ClipType::Timeline) {
            return true;
        }
    }
    return false;
}

void Bin::reloadAllProducers(bool reloadThumbs)
{
    if (m_itemModel->getRootFolder() == nullptr || m_itemModel->getRootFolder()->childCount() == 0 || !isEnabled()) {
        return;
    }
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    Q_EMIT openClip(std::shared_ptr<ProjectClip>());
    if (clipList.count() == 1) {
        // We only have one clip in the project, so this was called on a reset profile event.
        // Check if the clip is included in timeline to update it afterwards
        clipList.first()->updateTimelineOnReload();
    }
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        ClipType::ProducerType type = clip->clipType();
        if (type == ClipType::Timeline) {
            continue;
        }
        QDomDocument doc;
        QDomElement xml = clip->toXml(doc, false, false);
        // Make sure we reload clip length
        if (type == ClipType::AV || type == ClipType::Video || type == ClipType::Audio || type == ClipType::Playlist) {
            xml.removeAttribute(QStringLiteral("out"));
            Xml::removeXmlProperty(xml, QStringLiteral("length"));
            Xml::removeXmlProperty(xml, QStringLiteral("kdenlive:duration"));
        }
        if (clip->isValid()) {
            clip->resetProducerProperty(QStringLiteral("kdenlive:duration"));
            if (clip->hasLimitedDuration()) {
                clip->resetProducerProperty(QStringLiteral("length"));
            }
        }
        if (!xml.isNull()) {
            clip->discardAudioThumb();
            if (reloadThumbs) {
                ThumbnailCache::get()->invalidateThumbsForClip(clip->clipId());
            }
            clip->setClipStatus(FileStatus::StatusWaiting);
            ObjectId oid(KdenliveObjectType::BinClip, clip->clipId().toInt(), QUuid());
            pCore->taskManager.discardJobs(oid, AbstractTask::NOJOBTYPE, true,
                                           {AbstractTask::TRANSCODEJOB, AbstractTask::PROXYJOB, AbstractTask::AUDIOTHUMBJOB});
            ClipLoadTask::start(oid, xml, false, -1, -1, this);
        }
    }
}

void Bin::checkAudioThumbs()
{
    if (!KdenliveSettings::audiothumbnails() || m_itemModel->getRootFolder() == nullptr || m_itemModel->getRootFolder()->childCount() == 0 || !isEnabled()) {
        return;
    }
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    for (const auto &clip : qAsConst(clipList)) {
        ClipType::ProducerType type = clip->clipType();
        if (type == ClipType::AV || type == ClipType::Audio || type == ClipType::Playlist || type == ClipType::Unknown) {
            AudioLevelsTask::start(ObjectId(KdenliveObjectType::BinClip, clip->clipId().toInt(), QUuid()), clip.get(), false);
        }
    }
}

void Bin::slotMessageActionTriggered()
{
    m_infoMessage->animatedHide();
}

void Bin::getBinStats(uint *used, uint *unused, qint64 *usedSize, qint64 *unusedSize)
{
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        // Don't count sequence clips here
        if (clip->clipType() == ClipType::Timeline) {
            continue;
        }
        if (clip->refCount() == 0) {
            *unused += 1;
            *unusedSize += clip->getProducerInt64Property(QStringLiteral("kdenlive:file_size"));
        } else {
            *used += 1;
            *usedSize += clip->getProducerInt64Property(QStringLiteral("kdenlive:file_size"));
        }
    }
}

void Bin::rebuildProxies()
{
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    QList<std::shared_ptr<ProjectClip>> toProxy;
    for (const std::shared_ptr<ProjectClip> &clp : qAsConst(clipList)) {
        if (clp->hasProxy()) {
            toProxy << clp;
            // Abort all pending jobs
            pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, clp->clipId().toInt(), QUuid()), AbstractTask::PROXYJOB);
            clp->deleteProxy(false);
        }
    }
    if (toProxy.isEmpty()) {
        return;
    }
    auto *masterCommand = new QUndoCommand();
    masterCommand->setText(i18n("Rebuild proxies"));
    m_doc->slotProxyCurrentItem(true, toProxy, true, masterCommand);
    if (masterCommand->childCount() > 0) {
        m_doc->commandStack()->push(masterCommand);
    } else {
        delete masterCommand;
    }
}

void Bin::showClearButton(bool show)
{
    m_searchLine->setClearButtonEnabled(show);
}

void Bin::saveZone(const QStringList &info, const QDir &dir)
{
    if (info.size() != 3) {
        return;
    }
    std::shared_ptr<ProjectClip> clip = getBinClip(info.constFirst());
    if (clip) {
        QPoint zone(info.at(1).toInt(), info.at(2).toInt());
        clip->saveZone(zone, dir);
    }
}

void Bin::setCurrent(const std::shared_ptr<AbstractProjectItem> &item)
{
    switch (item->itemType()) {
    case AbstractProjectItem::ClipItem: {
        std::shared_ptr<ProjectClip> clp = std::static_pointer_cast<ProjectClip>(item);
        if (clp && clp->statusReady()) {
            openProducer(clp);
            Q_EMIT requestShowEffectStack(clp->clipName(), clp->m_effectStack, clp->getFrameSize(), false);
        }
        break;
    }
    case AbstractProjectItem::SubClipItem: {
        auto subClip = std::static_pointer_cast<ProjectSubClip>(item);
        QPoint zone = subClip->zone();
        std::shared_ptr<ProjectClip> master = subClip->getMasterClip();
        if (master && master->statusReady()) {
            openProducer(master, zone.x(), zone.y() + 1);
        }
        break;
    }
    case AbstractProjectItem::FolderItem:
        openProducer(nullptr);
    }
}

void Bin::cleanupUnused()
{
    m_itemModel->requestCleanupUnused();
}

size_t Bin::getClipDuration(int itemId) const
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(QString::number(itemId));
    Q_ASSERT(clip != nullptr);
    return clip->frameDuration();
}

QSize Bin::getFrameSize(int itemId) const
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(QString::number(itemId));
    Q_ASSERT(clip != nullptr);
    return clip->frameSize();
}

PlaylistState::ClipState Bin::getClipState(int itemId) const
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(QString::number(itemId));
    Q_ASSERT(clip != nullptr);
    bool audio = clip->hasAudio();
    bool video = clip->hasVideo();
    return audio ? (video ? PlaylistState::Disabled : PlaylistState::AudioOnly) : PlaylistState::VideoOnly;
}

QString Bin::getCurrentFolder()
{
    // Check parent item
    QModelIndex ix = m_proxyModel->selectionModel()->currentIndex();
    std::shared_ptr<ProjectFolder> parentFolder = m_itemModel->getRootFolder();
    if (ix.isValid() && m_proxyModel->selectionModel()->isSelected(ix)) {
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
        parentFolder = std::static_pointer_cast<ProjectFolder>(currentItem->getEnclosingFolder());
    } else {
        QModelIndex parentIx = m_itemView->rootIndex();
        if (parentIx.isValid()) {
            std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(parentIx));
            if (item && item != parentFolder) {
                parentFolder = std::static_pointer_cast<ProjectFolder>(item->getEnclosingFolder());
            }
        }
    }
    return parentFolder->clipId();
}

void Bin::adjustProjectProfileToItem()
{
    const QString clipId = m_monitor->activeClipId();
    slotCheckProfile(clipId);
}

void Bin::slotCheckProfile(const QString &binId)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(binId);
    if (clip && clip->statusReady()) {
        checkProfile(clip->originalProducer());
    }
}

// static
void Bin::checkProfile(const std::shared_ptr<Mlt::Producer> &producer)
{
    // Check if clip profile matches
    QString service = producer->get("mlt_service");
    // Check for image producer
    if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
        // This is an image, create profile from image size
        int width = producer->get_int("meta.media.width");
        if (width % 2 > 0) {
            width += width % 2;
        }
        int height = producer->get_int("meta.media.height");
        height += height % 2;
        if (width > 100 && height > 100 && pCore->getCurrentFrameSize() != QSize(width, height)) {
            std::unique_ptr<ProfileParam> projectProfile(new ProfileParam(pCore->getCurrentProfile().get()));
            projectProfile->m_width = width;
            projectProfile->m_height = height;
            projectProfile->m_sample_aspect_num = 1;
            projectProfile->m_sample_aspect_den = 1;
            projectProfile->m_display_aspect_num = width;
            projectProfile->m_display_aspect_den = height;
            projectProfile->m_description.clear();
            QMetaObject::invokeMethod(pCore->currentDoc(), "switchProfile", Q_ARG(ProfileParam *, new ProfileParam(projectProfile.get())),
                                      Q_ARG(QString, QFileInfo(producer->get("resource")).fileName()));
        } else {
            // Very small image, we probably don't want to use this as profile
        }
    } else if (service.contains(QStringLiteral("avformat"))) {
        std::unique_ptr<Mlt::Profile> blankProfile(new Mlt::Profile());
        blankProfile->set_explicit(0);
        blankProfile->from_producer(*producer);
        std::unique_ptr<ProfileParam> clipProfile(new ProfileParam(blankProfile.get()));
        std::unique_ptr<ProfileParam> projectProfile(new ProfileParam(pCore->getCurrentProfile().get()));
        clipProfile->adjustDimensions();
        if (*clipProfile.get() == *projectProfile.get()) {
            if (KdenliveSettings::default_profile().isEmpty()) {
                // Confirm default project format
                KdenliveSettings::setDefault_profile(pCore->getCurrentProfile()->path());
            }
        } else {
            // Profiles do not match, propose profile adjustment
            QMetaObject::invokeMethod(pCore->currentDoc(), "switchProfile", Q_ARG(ProfileParam *, new ProfileParam(clipProfile.get())),
                                      Q_ARG(QString, QFileInfo(producer->get("resource")).fileName()));
        }
    }
}

void Bin::showBinFrame(const QModelIndex &ix, int frame, bool storeFrame)
{
    std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
    if (item) {
        ClipType::ProducerType type = item->clipType();
        if (type != ClipType::AV && type != ClipType::Video && type != ClipType::Playlist && type != ClipType::SlideShow) {
            return;
        }
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            auto clip = std::static_pointer_cast<ProjectClip>(item);
            if (clip && (clip->clipType() == ClipType::AV || clip->clipType() == ClipType::Video || clip->clipType() == ClipType::Playlist)) {
                clip->getThumbFromPercent(frame, storeFrame);
            }
        } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
            auto clip = std::static_pointer_cast<ProjectSubClip>(item);
            if (clip && (clip->clipType() == ClipType::AV || clip->clipType() == ClipType::Video || clip->clipType() == ClipType::Playlist)) {
                clip->getThumbFromPercent(frame);
            }
        }
    }
}

void Bin::invalidateClip(const QString &binId)
{
    std::shared_ptr<ProjectClip> clip = getBinClip(binId);
    if (clip && clip->clipType() != ClipType::Audio) {
        QMap<QUuid, QList<int>> allIds = clip->getAllTimelineInstances();
        QMapIterator<QUuid, QList<int>> i(allIds);
        while (i.hasNext()) {
            i.next();
            QList<int> values = i.value();
            for (int j : qAsConst(values)) {
                pCore->invalidateItem(ObjectId(KdenliveObjectType::TimelineClip, j, i.key()));
            }
        }
    }
}

QSize Bin::sizeHint() const
{
    return QSize(350, pCore->window()->height() / 2);
}

void Bin::slotBack()
{
    QModelIndex currentRootIx = m_itemView->rootIndex();
    if (!currentRootIx.isValid()) {
        return;
    }
    std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(currentRootIx));
    if (!item) {
        qDebug() << "=== ERROR CANNOT FIND ROOT FOR CURRENT VIEW";
        return;
    }
    std::shared_ptr<AbstractProjectItem> parentItem = item->parent();
    if (!parentItem) {
        qDebug() << "=== ERROR CANNOT FIND PARENT FOR CURRENT VIEW";
        return;
    }
    if (parentItem != m_itemModel->getRootFolder()) {
        // We are entering a parent folder
        QModelIndex parentId = getIndexForId(parentItem->clipId(), parentItem->itemType() == AbstractProjectItem::FolderItem);
        if (parentId.isValid()) {
            m_itemView->setRootIndex(m_proxyModel->mapFromSource(parentId));
            parentWidget()->setWindowTitle(parentItem->name());
        }
    } else {
        m_itemView->setRootIndex(QModelIndex());
        m_upAction->setEnabled(false);
        parentWidget()->setWindowTitle(i18nc("@title:window", "Project Bin"));
    }
}

void Bin::checkProjectAudioTracks(QString clipId, int minimumTracksCount)
{
    if (m_currentMessage == BinMessage::BinCategory::ProfileMessage) {
        // Don't show this message if another one is active
        return;
    }
    int requestedTracks = minimumTracksCount - pCore->projectManager()->avTracksCount().first;
    if (requestedTracks > 0) {
        if (clipId.isEmpty()) {
            clipId = m_monitor->activeClipId();
        }
        QList<QAction *> list;
        QAction *ac = new QAction(QIcon::fromTheme(QStringLiteral("dialog-ok")), i18n("Add Tracks"), this);
        connect(ac, &QAction::triggered, [requestedTracks]() { pCore->projectManager()->addAudioTracks(requestedTracks); });
        QAction *ac2 = new QAction(QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Edit Streams"), this);
        connect(ac2, &QAction::triggered, this, [this, clipId]() {
            selectClipById(clipId);
            if (m_propertiesPanel) {
                for (QWidget *w : m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
                    if (w->parentWidget() && w->parentWidget()->parentWidget()) {
                        // Raise panel
                        w->parentWidget()->parentWidget()->show();
                        w->parentWidget()->parentWidget()->raise();
                    }
                    // Show audio tab
                    static_cast<ClipPropertiesController *>(w)->activatePage(2);
                }
            }
        });
        QAction *ac3 = new QAction(QIcon::fromTheme(QStringLiteral("dialog-ok")), i18n("Don't ask again"), this);
        connect(ac3, &QAction::triggered, [&]() { KdenliveSettings::setMultistream_checktrack(false); });
        // QAction *ac4 = new QAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")), i18n("Cancel"), this);
        list << ac << ac2 << ac3; // << ac4;
        doDisplayMessage(i18n("Your project needs more audio tracks to handle all streams. Add %1 audio tracks ?", requestedTracks),
                         KMessageWidget::Information, list, true, BinMessage::BinCategory::StreamsMessage);
    } else if (m_currentMessage == BinMessage::BinCategory::StreamsMessage) {
        // Clip streams number ok for the project, hide message
        m_infoMessage->animatedHide();
    }
}

void Bin::addClipMarker(const QString &binId, const QList<int> &positions, const QStringList &comments)
{
    std::shared_ptr<ProjectClip> clip = getBinClip(binId);
    if (!clip) {
        pCore->displayMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
        return;
    }
    QMap<GenTime, QString> markers;
    int ix = 0;
    for (int pos : positions) {
        GenTime p(pos, pCore->getCurrentFps());
        if (comments.size() == positions.size()) {
            markers.insert(p, comments.at(ix));
        } else {
            markers.insert(p, pCore->currentDoc()->timecode().getDisplayTimecode(p, false));
        }
        ix++;
    }
    clip->getMarkerModel()->addMarkers(markers, KdenliveSettings::default_marker_type());
}

void Bin::checkMissingProxies()
{
    if (m_itemModel->getRootFolder() == nullptr || m_itemModel->getRootFolder()->childCount() == 0) {
        return;
    }
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    QList<std::shared_ptr<ProjectClip>> toProxy;
    for (const auto &clip : qAsConst(clipList)) {
        if (clip->getProducerIntProperty(QStringLiteral("_replaceproxy")) > 0) {
            clip->resetProducerProperty(QStringLiteral("_replaceproxy"));
            toProxy << clip;
        }
    }
    if (!toProxy.isEmpty()) {
        pCore->currentDoc()->slotProxyCurrentItem(true, toProxy);
    }
}

void Bin::saveFolderState()
{
    // Check folder state (expanded or not)
    if (m_itemView == nullptr || m_listType != BinTreeView) {
        // Folder state is only valid in tree view mode
        return;
    }
    auto *view = static_cast<QTreeView *>(m_itemView);
    QList<std::shared_ptr<ProjectFolder>> folders = m_itemModel->getFolders();
    QStringList expandedFolders;
    for (const auto &folder : qAsConst(folders)) {
        QModelIndex ix = m_itemModel->getIndexFromItem(folder);
        if (view->isExpanded(m_proxyModel->mapFromSource(ix))) {
            // Save expanded state
            expandedFolders << folder->clipId();
        }
    }
    m_itemModel->saveProperty(QStringLiteral("kdenlive:expandedFolders"), expandedFolders.join(QLatin1Char(';')));
    m_itemModel->saveProperty(QStringLiteral("kdenlive:binZoom"), QString::number(KdenliveSettings::bin_zoom()));
}

void Bin::loadBinProperties(const QStringList &foldersToExpand, int zoomLevel)
{
    // Check folder state (expanded or not)
    if (m_itemView == nullptr || m_listType != BinTreeView) {
        // Folder state is only valid in tree view mode
        return;
    }
    auto *view = static_cast<QTreeView *>(m_itemView);
    for (const QString &id : foldersToExpand) {
        std::shared_ptr<ProjectFolder> folder = m_itemModel->getFolderByBinId(id);
        if (folder) {
            QModelIndex ix = m_itemModel->getIndexFromItem(folder);
            view->setExpanded(m_proxyModel->mapFromSource(ix), true);
        }
    }
    if (zoomLevel > -1) {
        m_slider->setValue(zoomLevel);
    }
}

QList<int> Bin::getUsedClipIds()
{
    QList<int> timelineClipIds;
    QList<std::shared_ptr<ProjectClip>> allClipIds = m_itemModel->getRootFolder()->childClips();
    for (const auto &clip : qAsConst(allClipIds)) {
        if (clip->isIncludedInTimeline()) {
            timelineClipIds.push_back(clip->binId().toInt());
        }
    }
    return timelineClipIds;
}

void Bin::savePlaylist(const QString &binId, const QString &savePath, const QVector<QPoint> &zones, const QMap<QString, QString> &properties, bool createNew)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(binId);
    if (!clip) {
        pCore->displayMessage(i18n("Could not find master clip"), MessageType::ErrorMessage, 300);
        return;
    }
    Mlt::Tractor t(pCore->getProjectProfile());
    std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(clip->originalProducer().get()));
    Mlt::Playlist main(pCore->getProjectProfile());
    main.set("id", "main_bin");
    main.set("xml_retain", 1);
    // Here we could store some kdenlive settings in the main playlist
    /*QMapIterator<QString, QString> i(properties);
    while (i.hasNext()) {
        i.next();
        main.set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
    }*/
    main.append(*prod.get());
    t.set("xml_retain main_bin", main.get_service(), 0);
    Mlt::Playlist pl(pCore->getProjectProfile());
    for (auto &zone : zones) {
        std::shared_ptr<Mlt::Producer> cut(prod->cut(zone.x(), zone.y()));
        pl.append(*cut.get());
    }
    t.set_track(pl, 0);
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer cons(pCore->getProjectProfile(), "xml", savePath.toUtf8().constData());
    cons.set("store", "kdenlive");
    cons.connect(t);
    cons.run();
    lock.unlock();
    if (createNew) {
        const QString id = slotAddClipToProject(QUrl::fromLocalFile(savePath));
        // Set properties directly on the clip
        std::shared_ptr<ProjectClip> playlistClip = m_itemModel->getClipByBinID(id);
        QMapIterator<QString, QString> i(properties);
        while (i.hasNext()) {
            i.next();
            playlistClip->setProducerProperty(i.key(), i.value());
        }
        selectClipById(id);
    }
}

void Bin::requestSelectionTranscoding(bool forceReplace)
{
    if (m_transcodingDialog == nullptr) {
        m_transcodingDialog = new TranscodeSeek(false, true, this);
        connect(m_transcodingDialog, &QDialog::accepted, this, [&]() {
            bool replace = m_transcodingDialog->replace_original->isChecked();
            if (!forceReplace) {
                KdenliveSettings::setTranscodingReplace(replace);
            }
            QMap<QString, QStringList> ids = m_transcodingDialog->ids();
            QMapIterator<QString, QStringList> i(ids);
            while (i.hasNext()) {
                i.next();
                std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(i.key());
                ObjectId oid(KdenliveObjectType::BinClip, i.key().toInt(), QUuid());
                if (clip->clipType() == ClipType::Timeline) {
                    // Ensure we use the correct out point
                    TranscodeTask::start(oid, i.value().first(), m_transcodingDialog->preParams(), m_transcodingDialog->params(i.value().at(1).toInt()), 0,
                                         clip->frameDuration(), replace, clip.get(), false, false);
                } else {
                    TranscodeTask::start(oid, i.value().first(), m_transcodingDialog->preParams(), m_transcodingDialog->params(i.value().at(1).toInt()), -1, -1,
                                         replace, clip.get(), false, false);
                }
            }
            m_transcodingDialog->deleteLater();
            m_transcodingDialog = nullptr;
        });
        connect(m_transcodingDialog, &QDialog::rejected, this, [&]() {
            m_transcodingDialog->deleteLater();
            m_transcodingDialog = nullptr;
        });
    }
    std::vector<QString> ids = selectedClipsIds();
    for (const auto &id : ids) {
        std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
        if (clip) {
            const QString clipService = clip->getProducerProperty(QStringLiteral("mlt_service"));
            if (clipService.startsWith(QLatin1String("avformat"))) {
                QString resource = clip->clipUrl();
                ClipType::ProducerType type = clip->clipType();
                int integerFps = qRound(clip->originalFps());
                QString suffix = QString("-%1fps").arg(integerFps);
                m_transcodingDialog->addUrl(resource, id, suffix, type, QString());
            } else {
                m_transcodingDialog->addUrl(clip->clipName(), id, QString(), clip->clipType(), QString());
            }
        }
    }
    m_transcodingDialog->show();
}

void Bin::requestTranscoding(const QString &url, const QString &id, int type, bool checkProfile, const QString &suffix, const QString &message)
{
    if (m_transcodingDialog == nullptr) {
        m_transcodingDialog = new TranscodeSeek(false, false, this);
        m_transcodingDialog->replace_original->setVisible(false);
        connect(m_transcodingDialog, &QDialog::accepted, this, [&, checkProfile]() {
            QMap<QString, QStringList> ids = m_transcodingDialog->ids();
            if (!ids.isEmpty()) {
                QString firstId = ids.firstKey();
                QMapIterator<QString, QStringList> i(ids);
                while (i.hasNext()) {
                    i.next();
                    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(i.key());
                    TranscodeTask::start(ObjectId(KdenliveObjectType::BinClip, i.key().toInt(), QUuid()), i.value().first(), m_transcodingDialog->preParams(),
                                         m_transcodingDialog->params(i.value().at(1).toInt()), -1, -1, true, clip.get(), false,
                                         i.key() == firstId ? checkProfile : false);
                }
            }
            m_transcodingDialog->deleteLater();
            m_transcodingDialog = nullptr;
        });
        connect(m_transcodingDialog, &QDialog::rejected, this, [&, checkProfile]() {
            QMap<QString, QStringList> ids = m_transcodingDialog->ids();
            QString firstId;
            if (!ids.isEmpty()) {
                firstId = ids.firstKey();
            }
            m_transcodingDialog->deleteLater();
            m_transcodingDialog = nullptr;
            if (checkProfile && !firstId.isEmpty()) {
                pCore->bin()->slotCheckProfile(firstId);
            }
        });
    }
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(id);
    if (clip) {
        ClipType::ProducerType cType = (ClipType::ProducerType)type;
        if (cType == ClipType::Unknown) {
            cType = clip->clipType();
        }
        if (url.isEmpty()) {
            QString resource = clip->clipUrl();
            m_transcodingDialog->addUrl(resource, id, suffix, cType, message);
        } else {
            m_transcodingDialog->addUrl(url, id, suffix, cType, message);
        }
    }
    m_transcodingDialog->show();
}

void Bin::addFilterToClip(const QString &sourceClipId, const QString &filterId, stringMap params)
{
    std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(sourceClipId);
    if (clip) {
        clip->addEffect(filterId, params);
    }
}

bool Bin::addProjectClipInFolder(const QString &path, const QString &sourceClipId, const QString &sourceFolder, const QString &jobId)
{
    // Check if the clip is already inserted in the project, if yes exit
    QStringList existingIds = m_itemModel->getClipByUrl(QFileInfo(path));
    if (!existingIds.isEmpty()) {
        return true;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::pair<ClipJobManager::JobCompletionAction, QString> jobAction = ClipJobManager::getJobAction(jobId);
    if (jobAction.first == ClipJobManager::JobCompletionAction::ReplaceOriginal) {
        // Simply replace source clip with stabilized version
        replaceSingleClip(sourceClipId, path);
        return true;
    }
    // Check if folder exists
    QString folderId = QStringLiteral("-1");
    std::shared_ptr<ProjectFolder> baseFolder = nullptr;
    if (jobAction.first == ClipJobManager::JobCompletionAction::SubFolder) {
        baseFolder = m_itemModel->getFolderByBinId(sourceFolder);
    }
    if (baseFolder == nullptr) {
        baseFolder = m_itemModel->getRootFolder();
    }
    if (jobAction.second.isEmpty()) {
        // Put clip in sourceFolder if we don't have a folder name
        folderId = sourceFolder;
    } else {
        bool found = false;
        for (int i = 0; i < baseFolder->childCount(); ++i) {
            auto currentItem = std::static_pointer_cast<AbstractProjectItem>(baseFolder->child(i));
            if (currentItem->itemType() == AbstractProjectItem::FolderItem && currentItem->name() == jobAction.second) {
                found = true;
                folderId = currentItem->clipId();
                break;
            }
        }

        if (!found) {
            // if it was not found, create folder
            m_itemModel->requestAddFolder(folderId, jobAction.second, baseFolder->clipId(), undo, redo);
        }
    }
    std::function<void(const QString &)> callBack = [this, sourceClipId, jobAction, path](const QString &binId) {
        if (!binId.isEmpty()) {
            if (jobAction.first != ClipJobManager::JobCompletionAction::ReplaceOriginal) {
                // Clip was added to Bin, select it if replaced clip is still selected
                QModelIndex ix = m_proxyModel->selectionModel()->currentIndex();
                if (ix.isValid()) {
                    std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
                    if (currentItem->clipId() == sourceClipId) {
                        selectClipById(binId);
                    }
                }
            }
        }
    };
    auto id = ClipCreator::createClipFromFile(path, folderId, m_itemModel, undo, redo, callBack);
    bool ok = (id != QStringLiteral("-1"));
    if (ok) {
        pCore->pushUndo(undo, redo, i18nc("@action", "Add clip"));
    }
    return ok;
}

void Bin::updateClipsCount()
{
    int count = m_itemModel->clipsCount();
    if (count < 2) {
        m_clipsCountMessage = QString();
    } else {
        int selected = 0;
        const QModelIndexList indexes = m_proxyModel->selectionModel()->selection().indexes();
        for (const QModelIndex &ix : indexes) {
            if (ix.isValid() && ix.column() == 0) {
                std::shared_ptr<AbstractProjectItem> item = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(ix));
                if (item->itemType() == AbstractProjectItem::ClipItem) {
                    selected++;
                }
            }
        }
        if (selected == 0) {
            m_clipsCountMessage = i18n("<b>%1</b> clips | ", count);
        } else {
            m_clipsCountMessage = i18n("<b>%1</b> clips (%2 selected) | ", count, selected);
        }
    }
    showBinInfo();
}

void Bin::updateKeyBinding(const QString &bindingMessage)
{
    m_keyBindingMessage = bindingMessage;
    showBinInfo();
}

void Bin::showBinInfo()
{
    pCore->window()->showKeyBinding(QString("%1%2").arg(m_clipsCountMessage, m_keyBindingMessage));
}

bool Bin::containsId(const QString &clipId) const
{
    if (m_listType == BinIconView) {
        // Check if the clip is in our folder
        const QString rootId = m_itemView->rootIndex().data(AbstractProjectItem::DataId).toString();
        std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(clipId);
        if (clip) {
            std::shared_ptr<AbstractProjectItem> par = clip->parent();
            if (par && par->clipId() == rootId) {
                return true;
            }
        }
        return false;
    } else {
    }
    return true;
}

void Bin::processMultiStream(const QString &clipId, QList<int> videoStreams, QList<int> audioStreams)
{
    auto binClip = m_itemModel->getClipByBinID(clipId);
    // We retrieve the folder containing our clip, because we will set the other streams in the same
    std::shared_ptr<AbstractProjectItem> baseFolder = binClip->parent();
    if (!baseFolder) {
        baseFolder = m_itemModel->getRootFolder();
    }
    const QString parentId = baseFolder->clipId();
    std::shared_ptr<Mlt::Producer> producer = binClip->originalProducer();
    // This helper lambda request addition of a given stream
    auto addStream = [this, parentId, producer](int vindex, int vstream, int aindex, int astream, Fun &undo, Fun &redo) {
        auto clone = ProjectClip::cloneProducer(producer);
        clone->set("video_index", vindex);
        clone->set("audio_index", aindex);
        clone->set("vstream", vstream);
        clone->set("astream", astream);
        QString id;
        m_itemModel->requestAddBinClip(id, clone, parentId, undo, redo);
    };
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    if (KdenliveSettings::automultistreams()) {
        for (int i = 1; i < videoStreams.count(); ++i) {
            int vindex = videoStreams.at(i);
            int aindex = 0;
            if (i <= audioStreams.count() - 1) {
                aindex = audioStreams.at(i);
            }
            addStream(vindex, i - 1, aindex, qMin(i - 1, audioStreams.count() - 1), undo, redo);
        }
        pCore->pushUndo(undo, redo, i18np("Add additional stream for clip", "Add additional streams for clip", videoStreams.count() - 1));
        return;
    }

    int width = int(60.0 * pCore->getCurrentDar());
    if (width % 2 == 1) {
        width++;
    }

    QScopedPointer<QDialog> dialog(new QDialog(qApp->activeWindow()));
    dialog->setWindowTitle(QStringLiteral("Multi Stream Clip"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QWidget *mainWidget = new QWidget(dialog.data());
    auto *mainLayout = new QVBoxLayout;
    dialog->setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    dialog->connect(buttonBox, &QDialogButtonBox::accepted, dialog.data(), &QDialog::accept);
    dialog->connect(buttonBox, &QDialogButtonBox::rejected, dialog.data(), &QDialog::reject);
    okButton->setText(i18n("Import selected clips"));

    QLabel *lab1 = new QLabel(i18n("Additional streams for clip\n %1", binClip->clipName()), mainWidget);
    mainLayout->addWidget(lab1);
    QList<QGroupBox *> groupList;
    QList<QComboBox *> comboList;
    // We start loading the list at 1, video index 0 should already be loaded
    for (int j = 1; j < videoStreams.count(); ++j) {
        auto clone = ProjectClip::cloneProducer(producer);
        clone->set("video_index", videoStreams.at(j));
        if (clone == nullptr || !clone->is_valid()) {
            continue;
        }
        // TODO this keyframe should be cached
        QImage thumb = KThumb::getFrame(clone.get(), 0, width, 60);
        QGroupBox *streamFrame = new QGroupBox(i18n("Video stream %1", videoStreams.at(j)), mainWidget);
        mainLayout->addWidget(streamFrame);
        streamFrame->setProperty("vindex", videoStreams.at(j));
        groupList << streamFrame;
        streamFrame->setCheckable(true);
        streamFrame->setChecked(true);
        auto *vh = new QVBoxLayout(streamFrame);
        QLabel *iconLabel = new QLabel(mainWidget);
        mainLayout->addWidget(iconLabel);
        iconLabel->setPixmap(QPixmap::fromImage(thumb));
        vh->addWidget(iconLabel);
        if (audioStreams.count() > 1) {
            auto *cb = new QComboBox(mainWidget);
            mainLayout->addWidget(cb);
            for (int k = 0; k < audioStreams.count(); ++k) {
                cb->addItem(i18n("Audio stream %1", audioStreams.at(k)), audioStreams.at(k));
            }
            comboList << cb;
            cb->setCurrentIndex(qMin(j, audioStreams.count() - 1));
            vh->addWidget(cb);
        }
        mainLayout->addWidget(streamFrame);
    }
    mainLayout->addStretch(10);
    mainLayout->addWidget(buttonBox);
    if (dialog->exec() == QDialog::Accepted) {
        // import selected streams
        int importedStreams = 0;
        for (int i = 0; i < groupList.count(); ++i) {
            if (groupList.at(i)->isChecked()) {
                int vindex = groupList.at(i)->property("vindex").toInt();
                int ax = qMin(i, comboList.size() - 1);
                int aindex = -1;
                if (ax >= 0) {
                    // only check audio index if we have several audio streams
                    aindex = comboList.at(ax)->itemData(comboList.at(ax)->currentIndex()).toInt();
                }
                addStream(vindex, i, aindex, ax, undo, redo);
                importedStreams++;
            }
        }
        pCore->pushUndo(undo, redo, i18np("Add additional stream for clip", "Add additional streams for clip", importedStreams));
    }
}

int Bin::getAllClipMarkers(int category) const
{
    int markersCount = 0;
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        markersCount += clip->getMarkerModel()->getAllMarkers(category).count();
    }
    return markersCount;
}

void Bin::removeMarkerCategories(QList<int> toRemove, const QMap<int, int> remapCategories)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool found = false;
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        bool res = clip->removeMarkerCategories(toRemove, remapCategories, undo, redo);
        if (!found && res) {
            found = true;
        }
    }
    if (found) {
        pCore->pushUndo(undo, redo, i18n("Remove clip markers"));
    }
}

QStringList Bin::sequenceReferencedClips(const QUuid &uuid) const
{
    QStringList results;
    QList<std::shared_ptr<ProjectClip>> clipList = m_itemModel->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        if (clip->refCount() > 0) {
            const QString referenced = clip->isReferenced(uuid);
            if (!referenced.isEmpty()) {
                results << referenced;
            }
        }
    }
    return results;
}

void Bin::updateSequenceClip(const QUuid &uuid, int duration, int pos, bool forceUpdate)
{
    if (pos > -1) {
        m_doc->setSequenceProperty(uuid, QStringLiteral("position"), pos);
    }
    const QString binId = m_itemModel->getSequenceId(uuid);
    if (!binId.isEmpty() && m_doc->isModified()) {
        std::shared_ptr<ProjectClip> clip = m_itemModel->getClipByBinID(binId);
        Q_ASSERT(clip != nullptr);
        if (m_doc->sequenceThumbRequiresRefresh(uuid) || forceUpdate) {
            // Store general sequence properties
            QMap<QString, QString> properties;
            properties.insert(QStringLiteral("length"), QString::number(duration));
            properties.insert(QStringLiteral("out"), QString::number(duration - 1));
            properties.insert(QStringLiteral("kdenlive:duration"), clip->framesToTime(duration));
            properties.insert(QStringLiteral("kdenlive:maxduration"), QString::number(duration));
            clip->setProperties(properties);
            // Reset thumbs producer
            m_doc->sequenceThumbUpdated(uuid);
            clip->resetSequenceThumbnails();
            clip->reloadTimeline();
            ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, binId.toInt(), QUuid()), QDomElement(), true, -1, -1, this);
        }
    }
}

void Bin::updateTimelineOccurrences()
{
    // Update the clip in timeline menu
    QModelIndex currentSelection = m_proxyModel->selectionModel()->currentIndex();
    bool triggered = false;
    if (currentSelection.isValid()) {
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(currentSelection));
        if (currentItem) {
            if (currentItem->itemType() == AbstractProjectItem::ClipItem) {
                auto clip = std::static_pointer_cast<ProjectClip>(currentItem);
                if (clip) {
                    triggered = true;
                    Q_EMIT findInTimeline(clip->clipId(), clip->timelineInstances());
                }
            } else if (currentItem->itemType() == AbstractProjectItem::FolderItem) {
                m_sequencesFolderAction->setChecked(currentItem->clipId().toInt() == m_itemModel->defaultSequencesFolder());
                m_audioCapturesFolderAction->setChecked(currentItem->clipId().toInt() == m_itemModel->defaultAudioCaptureFolder());
            }
        }
    }
    if (!triggered) {
        Q_EMIT findInTimeline(QString());
    }
}

void Bin::setSequenceThumbnail(const QUuid &uuid, int frame)
{
    const QString bid = m_itemModel->getSequenceId(uuid);
    if (!bid.isEmpty()) {
        std::shared_ptr<ProjectClip> sequenceClip = getBinClip(bid);
        if (sequenceClip) {
            m_doc->setSequenceProperty(uuid, QStringLiteral("thumbnailFrame"), frame);
            ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, bid.toInt(), QUuid()), QDomElement(), true, -1, -1, this);
        }
    }
}

void Bin::updateSequenceAVType(const QUuid &uuid, int tracksCount)
{
    const QString bId = m_itemModel->getSequenceId(uuid);
    if (!bId.isEmpty()) {
        std::shared_ptr<ProjectClip> sequenceClip = getBinClip(bId);
        if (sequenceClip) {
            sequenceClip->refreshTracksState(tracksCount);
        }
    }
}

void Bin::setDefaultSequenceFolder(bool enable)
{
    QModelIndex currentSelection = m_proxyModel->selectionModel()->currentIndex();
    if (currentSelection.isValid()) {
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(currentSelection));
        if (currentItem) {
            m_itemModel->setSequencesFolder(enable ? currentItem->clipId().toInt() : -1);
        }
    }
}

void Bin::setDefaultAudioCaptureFolder(bool enable)
{
    QModelIndex currentSelection = m_proxyModel->selectionModel()->currentIndex();
    if (currentSelection.isValid()) {
        std::shared_ptr<AbstractProjectItem> currentItem = m_itemModel->getBinItemByIndex(m_proxyModel->mapToSource(currentSelection));
        if (currentItem) {
            m_itemModel->setAudioCaptureFolder(enable ? currentItem->clipId().toInt() : -1);
        }
    }
}

void Bin::moveTimeWarpToFolder(const QDir sequenceFolder, bool copy)
{
    QList<std::shared_ptr<ProjectClip>> allClips = m_itemModel->getRootFolder()->childClips();
    for (auto &c : allClips) {
        c->copyTimeWarpProducers(sequenceFolder, copy);
    }
}

void Bin::sequenceActivated()
{
    updateTargets();
    QList<std::shared_ptr<ProjectClip>> allClips = m_itemModel->getRootFolder()->childClips();
    for (auto &c : allClips) {
        c->refreshBounds();
    }
}

bool Bin::usesVariableFpsClip()
{
    QList<std::shared_ptr<ProjectClip>> allClips = m_itemModel->getRootFolder()->childClips();
    for (auto &c : allClips) {
        ClipType::ProducerType type = c->clipType();
        if ((type == ClipType::AV || type == ClipType::Video || type == ClipType::Audio) && c->hasVariableFps()) {
            return true;
        }
    }
    return false;
}

void Bin::transcodeUsedClips()
{
    if (m_doc->isModified()) {
        // Recommend saving before the transcode operation
        if (KMessageBox::questionTwoActions(QApplication::activeWindow(),
                                            i18nc("@label:textbox", "We recommend that you save the project before the transcode operation"), {},
                                            KGuiItem(i18nc("@action:button", "Save Project")),
                                            KGuiItem(i18nc("@action:button", "Transcode Without Saving"))) == KMessageBox::PrimaryAction) {
            if (!pCore->projectManager()->saveFile()) {
                return;
            }
        }
    }
    QList<std::shared_ptr<ProjectClip>> allClips = m_itemModel->getRootFolder()->childClips();
    m_proxyModel->selectionModel()->clearSelection();
    // Select all variable fps clips
    for (auto &c : allClips) {
        ClipType::ProducerType type = c->clipType();
        if ((type == ClipType::AV || type == ClipType::Video || type == ClipType::Audio) && c->hasVariableFps()) {
            QModelIndex ix = m_itemModel->getIndexFromItem(c);
            int row = ix.row();
            const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
            const QModelIndex id2 = m_itemModel->index(row, m_itemModel->columnCount() - 1, ix.parent());
            m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)),
                                                   QItemSelectionModel::Select);
        }
    }
    requestSelectionTranscoding(true);
}
