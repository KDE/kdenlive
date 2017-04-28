/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#include "bin.h"
#include "kdenlive_debug.h"
#include "mainwindow.h"
#include "projectitemmodel.h"
#include "projectclip.h"
#include "projectsubclip.h"
#include "projectfolder.h"
#include "projectfolderup.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include "project/clipmanager.h"
#include "project/dialogs/slideshowclip.h"
#include "project/jobs/jobmanager.h"
#include "monitor/monitor.h"
#include "doc/kdenlivedoc.h"
#include "dialogs/clipcreationdialog.h"
#include "ui_qtextclip_ui.h"
#include "titler/titlewidget.h"
#include "core.h"
#include "utils/KoIconUtils.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/clippropertiescontroller.h"
#include "project/projectcommands.h"
#include "project/invaliddialog.h"
#include "projectsortproxymodel.h"
#include "bincommands.h"
#include "doc/documentchecker.h"
#include "mlt++/Mlt.h"

#include <QToolBar>
#include <KColorScheme>
#include <KMessageBox>
#include <KXMLGUIFactory>

#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDialogButtonBox>
#include <QDrag>
#include <QVBoxLayout>
#include <QTimeLine>
#include <QSlider>
#include <QMenu>
#include "kdenlive_debug.h"
#include <QtConcurrent>
#include <QUndoCommand>
#include <QCryptographicHash>

MyListView::MyListView(QWidget *parent) : QListView(parent)
{
    setViewMode(QListView::IconMode);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setUniformItemSizes(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setAcceptDrops(true);
    setDragEnabled(true);
    viewport()->setAcceptDrops(true);
}

void MyListView::focusInEvent(QFocusEvent *event)
{
    QListView::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason) {
        emit focusView();
    }
}

MyTreeView::MyTreeView(QWidget *parent) : QTreeView(parent)
{
    setEditing(false);
}

void MyTreeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_startPos = event->pos();
    }
    QTreeView::mousePressEvent(event);
}

void MyTreeView::focusInEvent(QFocusEvent *event)
{
    QTreeView::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason) {
        emit focusView();
    }
}

void MyTreeView::mouseMoveEvent(QMouseEvent *event)
{
    bool dragged = false;
    if (event->buttons() & Qt::LeftButton) {
        int distance = (event->pos() - m_startPos).manhattanLength();
        if (distance >= QApplication::startDragDistance()) {
            dragged = performDrag();
        }
    }
    if (!dragged) {
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

void MyTreeView::keyPressEvent(QKeyEvent *event)
{
    if (isEditing()) {
        QTreeView::keyPressEvent(event);
        return;
    }
    QModelIndex currentIndex = this->currentIndex();
    if (event->key() == Qt::Key_Return && currentIndex.isValid()) {
        if (this->isExpanded(currentIndex)) {
            this->collapse(currentIndex);
        } else {
            this->expand(currentIndex);
        }
    }
    QTreeView::keyPressEvent(event);
}

bool MyTreeView::isEditing() const
{
    return state() == QAbstractItemView::EditingState;
}

void MyTreeView::setEditing(bool edit)
{
    setState(edit ? QAbstractItemView::EditingState : QAbstractItemView::NoState);
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
    QDrag *drag = new QDrag(this);
    drag->setMimeData(model()->mimeData(indexes));
    QModelIndex ix = indexes.first();
    if (ix.isValid()) {
        QIcon icon = ix.data(AbstractProjectItem::DataThumbnail).value<QIcon>();
        QPixmap pix = icon.pixmap(iconSize());
        QSize size = pix.size();
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setOpacity(0.7);
        p.drawPixmap(0, 0, pix);
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
    return true;
}

BinMessageWidget::BinMessageWidget(QWidget *parent) : KMessageWidget(parent) {}
BinMessageWidget::BinMessageWidget(const QString &text, QWidget *parent) : KMessageWidget(text, parent) {}

bool BinMessageWidget::event(QEvent *ev)
{
    if (ev->type() == QEvent::Hide || ev->type() == QEvent::Close) {
        emit messageClosing();
    }
    return KMessageWidget::event(ev);
}

SmallJobLabel::SmallJobLabel(QWidget *parent) : QPushButton(parent)
    , m_action(nullptr)
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
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor bg = scheme.background(KColorScheme::LinkBackground).color();
    QColor fg = scheme.foreground(KColorScheme::LinkText).color();
    QString style = QStringLiteral("QPushButton {margin:3px;padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}").arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(fg.red()).arg(fg.green()).arg(fg.blue());

    bg = scheme.background(KColorScheme::ActiveBackground).color();
    fg = scheme.foreground(KColorScheme::ActiveText).color();
    style.append(QStringLiteral("\nQPushButton:hover {margin:3px;padding:2px;background-color: rgb(%1, %2, %3);border-radius: 4px;border: none;color: rgb(%4, %5, %6)}").arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(fg.red()).arg(fg.green()).arg(fg.blue()));

    return style;
}

void SmallJobLabel::setAction(QAction *action)
{
    m_action = action;
}

void SmallJobLabel::slotTimeLineChanged(qreal value)
{
    setFixedWidth(qMin(value * 2, qreal(1.0)) * sizeHint().width());
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
    if (jobCount > 0) {
        // prepare animation
        setText(i18np("%1 job", "%1 jobs", jobCount));
        setToolTip(i18np("%1 pending job", "%1 pending jobs", jobCount));

        if (style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) {
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
        if (style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) {
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

/**
 * @class BinItemDelegate
 * @brief This class is responsible for drawing items in the QTreeView.
 */

class BinItemDelegate: public QStyledItemDelegate
{
public:
    explicit BinItemDelegate(QObject *parent = nullptr): QStyledItemDelegate(parent)
    {
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE
    {
        if (index.column() != 0) {
            return QStyledItemDelegate::updateEditorGeometry(editor, option, index);
        }
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QRect r1 = option.rect;
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        double factor = (double) opt.decorationSize.height() / r1.height();
        int decoWidth = 2 * textMargin;
        int mid = 0;
        if (factor > 0) {
            decoWidth += opt.decorationSize.width() / factor;
        }
        if (type == AbstractProjectItem::ClipItem || type == AbstractProjectItem::SubClipItem) {
            mid = (int)((r1.height() / 2));
        }
        r1.adjust(decoWidth, 0, 0, -mid);
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QRect r2 = fm.boundingRect(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString()).toRect();
        editor->setGeometry(r2);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        QString text = index.data(AbstractProjectItem::DataName).toString();
        QRectF r = option.rect;
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        int width = fm.boundingRect(r, Qt::AlignLeft | Qt::AlignTop, text).width() + option.decorationSize.width() + 2 * textMargin;
        hint.setWidth(width);
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        if (type == AbstractProjectItem::FolderItem || type == AbstractProjectItem::FolderUpItem) {
            return QSize(hint.width(), qMin(option.fontMetrics.lineSpacing() + 4, hint.height()));
        }
        if (type == AbstractProjectItem::ClipItem) {
            return QSize(hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, qMax(hint.height(), option.decorationSize.height())));
        }
        if (type == AbstractProjectItem::SubClipItem) {
            return QSize(hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, qMin(hint.height(), (int)(option.decorationSize.height() / 1.5))));
        }
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QString line1 = index.data(Qt::DisplayRole).toString();
        QString line2 = index.data(Qt::UserRole).toString();

        int textW = qMax(option.fontMetrics.width(line1), option.fontMetrics.width(line2));
        QSize iconSize = icon.actualSize(option.decorationSize);
        return QSize(qMax(textW, iconSize.width()) + 4, option.fontMetrics.lineSpacing() * 2 + 4);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE
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
            //QRect r = QStyle::alignedRect(opt.direction, Qt::AlignVCenter | Qt::AlignLeft, opt.decorationSize, r1);

            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.highlightedText().color());
            } else {
                painter->setPen(option.palette.text().color());
            }
            QRect r = r1;
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            if (type == AbstractProjectItem::ClipItem || type == AbstractProjectItem::SubClipItem) {
                double factor = (double) opt.decorationSize.height() / r1.height();
                int decoWidth = 2 * textMargin;
                if (factor > 0) {
                    r.setWidth(opt.decorationSize.width() / factor);
                    // Draw thumbnail
                    opt.icon.paint(painter, r);
                    decoWidth += r.width();
                }
                int mid = (int)((r1.height() / 2));
                r1.adjust(decoWidth, 0, 0, -mid);
                QRect r2 = option.rect;
                r2.adjust(decoWidth, mid, 0, 0);
                QRectF bounding;
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString(), &bounding);
                font.setBold(false);
                painter->setFont(font);
                QString subText = index.data(AbstractProjectItem::DataDuration).toString();
                if (!subText.isEmpty()) {
                    r2.adjust(0, bounding.bottom() - r2.top(), 0, 0);
                    QColor subTextColor = painter->pen().color();
                    subTextColor.setAlphaF(.5);
                    painter->setPen(subTextColor);
                    painter->drawText(r2, Qt::AlignLeft | Qt::AlignTop, subText, &bounding);
                    // Draw usage counter
                    int usage = index.data(AbstractProjectItem::UsageCount).toInt();
                    if (usage > 0) {
                        bounding.moveLeft(bounding.right() + (2 * textMargin));
                        QString us = QString().sprintf("[%d]", usage);
                        painter->drawText(bounding, Qt::AlignLeft | Qt::AlignTop, us, &bounding);
                    }
                }
                if (type == AbstractProjectItem::ClipItem) {
                    // Overlay icon if necessary
                    QVariant v = index.data(AbstractProjectItem::IconOverlay);
                    if (!v.isNull()) {
                        QIcon reload = QIcon::fromTheme(v.toString());
                        r.setTop(r.bottom() - bounding.height());
                        r.setWidth(bounding.height());
                        reload.paint(painter, r);
                    }

                    int jobProgress = index.data(AbstractProjectItem::JobProgress).toInt();
                    if (jobProgress > 0 || jobProgress == JobWaiting) {
                        // Draw job progress bar
                        int progressWidth = option.fontMetrics.averageCharWidth() * 8;
                        int progressHeight = option.fontMetrics.ascent() / 4;
                        QRect progress(r1.x() + 1, opt.rect.bottom() - progressHeight - 2, progressWidth, progressHeight);
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(Qt::darkGray);
                        if (jobProgress > 0) {
                            painter->drawRoundedRect(progress, 2, 2);
                            painter->setBrush(option.state & QStyle::State_Selected ? option.palette.text() : option.palette.highlight());
                            progress.setWidth((progressWidth - 2) * jobProgress / 100);
                            painter->drawRoundedRect(progress, 2, 2);
                        } else if (jobProgress == JobWaiting) {
                            // Draw kind of a pause icon
                            progress.setWidth(3);
                            painter->drawRect(progress);
                            progress.moveLeft(progress.right() + 3);
                            painter->drawRect(progress);
                        }
                    } else if (jobProgress == JobCrashed) {
                        QString jobText = index.data(AbstractProjectItem::JobMessage).toString();
                        if (!jobText.isEmpty()) {
                            QRectF txtBounding = painter->boundingRect(r2, Qt::AlignRight | Qt::AlignVCenter, " " + jobText + " ");
                            painter->setPen(Qt::NoPen);
                            painter->setBrush(option.palette.highlight());
                            painter->drawRoundedRect(txtBounding, 2, 2);
                            painter->setPen(option.palette.highlightedText().color());
                            painter->drawText(txtBounding, Qt::AlignCenter, jobText);
                        }
                    }
                }
            } else {
                // Folder or Folder Up items
                double factor = (double) opt.decorationSize.height() / r1.height();
                int decoWidth = 2 * textMargin;
                if (factor > 0) {
                    r.setWidth(opt.decorationSize.width() / factor);
                    // Draw thumbnail
                    opt.icon.paint(painter, r);
                    decoWidth += r.width();
                }
                r1.adjust(decoWidth, 0, 0, 0);
                QRectF bounding;
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString(), &bounding);
            }
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

LineEventEater::LineEventEater(QObject *parent) : QObject(parent)
{
}

bool LineEventEater::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        if (((QKeyEvent *)event)->key() == Qt::Key_Escape) {
            emit clearSearchLine();
        }
        break;
    case QEvent::Resize:
        // Workaround Qt BUG 54676
        emit showClearButton(((QResizeEvent *)event)->size().width() > QFontMetrics(QApplication::font()).averageCharWidth() * 8);
        break;
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

Bin::Bin(QWidget *parent) :
    QWidget(parent)
    , isLoading(false)
    , m_itemModel(nullptr)
    , m_itemView(nullptr)
    , m_rootFolder(nullptr)
    , m_folderUp(nullptr)
    , m_jobManager(nullptr)
    , m_doc(nullptr)
    , m_extractAudioAction(nullptr)
    , m_transcodeAction(nullptr)
    , m_clipsActionsMenu(nullptr)
    , m_inTimelineAction(nullptr)
    , m_listType((BinViewType) KdenliveSettings::binMode())
    , m_iconSize(160, 90)
    , m_propertiesPanel(nullptr)
    , m_blankThumb()
    , m_invalidClipDialog(nullptr)
    , m_gainedFocus(false)
    , m_audioDuration(0)
    , m_processedAudio(0)
{
    m_layout = new QVBoxLayout(this);

    // Create toolbar for buttons
    m_toolbar = new QToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_layout->addWidget(m_toolbar);

    // Search line
    m_proxyModel = new ProjectSortProxyModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_searchLine = new QLineEdit(this);
    m_searchLine->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    //m_searchLine->setClearButtonEnabled(true);
    m_searchLine->setPlaceholderText(i18n("Search"));
    m_searchLine->setFocusPolicy(Qt::ClickFocus);
    connect(m_searchLine, &QLineEdit::textChanged, m_proxyModel, &ProjectSortProxyModel::slotSetSearchString);

    LineEventEater *leventEater = new LineEventEater(this);
    m_searchLine->installEventFilter(leventEater);
    connect(leventEater, &LineEventEater::clearSearchLine, m_searchLine, &QLineEdit::clear);
    connect(leventEater, &LineEventEater::showClearButton, this, &Bin::showClearButton);

    setFocusPolicy(Qt::ClickFocus);
    // Build item view model
    m_itemModel = new ProjectItemModel(this);

    // Connect models
    m_proxyModel->setSourceModel(m_itemModel);
    connect(m_itemModel, SIGNAL(dataChanged(QModelIndex, QModelIndex)), m_proxyModel, SLOT(slotDataChanged(const QModelIndex &, const
            QModelIndex &)));
    connect(m_itemModel, &QAbstractItemModel::rowsInserted, this, &Bin::rowsInserted);
    connect(m_itemModel, &QAbstractItemModel::rowsRemoved, this, &Bin::rowsRemoved);
    connect(m_proxyModel, &ProjectSortProxyModel::selectModel, this, &Bin::selectProxyModel);
    connect(m_itemModel, SIGNAL(itemDropped(QStringList, QModelIndex)), this, SLOT(slotItemDropped(QStringList, QModelIndex)));
    connect(m_itemModel, SIGNAL(itemDropped(QList<QUrl>, QModelIndex)), this, SLOT(slotItemDropped(QList<QUrl>, QModelIndex)));
    connect(m_itemModel, SIGNAL(effectDropped(QString, QModelIndex)), this, SLOT(slotEffectDropped(QString, QModelIndex)));
    connect(m_itemModel, &QAbstractItemModel::dataChanged, this, &Bin::slotItemEdited);
    connect(m_itemModel, &ProjectItemModel::addClipCut, this, &Bin::slotAddClipCut);
    connect(this, &Bin::refreshPanel, this, &Bin::doRefreshPanel);

    // Zoom slider
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMaximumWidth(100);
    m_slider->setMinimumWidth(40);
    m_slider->setRange(0, 10);
    m_slider->setValue(KdenliveSettings::bin_zoom());
    connect(m_slider, &QAbstractSlider::valueChanged, this, &Bin::slotSetIconSize);
    QWidgetAction *widgetslider = new QWidgetAction(this);
    widgetslider->setDefaultWidget(m_slider);

    // View type
    KSelectAction *listType = new KSelectAction(KoIconUtils::themedIcon(QStringLiteral("view-list-tree")), i18n("View Mode"), this);
    pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode"), listType);
    QAction *treeViewAction = listType->addAction(KoIconUtils::themedIcon(QStringLiteral("view-list-tree")), i18n("Tree View"));
    listType->addAction(treeViewAction);
    treeViewAction->setData(BinTreeView);
    if (m_listType == treeViewAction->data().toInt()) {
        listType->setCurrentAction(treeViewAction);
    }
    pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode_tree"), treeViewAction);

    QAction *iconViewAction = listType->addAction(KoIconUtils::themedIcon(QStringLiteral("view-list-icons")), i18n("Icon View"));
    iconViewAction->setData(BinIconView);
    if (m_listType == iconViewAction->data().toInt()) {
        listType->setCurrentAction(iconViewAction);
    }
    pCore->window()->actionCollection()->addAction(QStringLiteral("bin_view_mode_icon"), iconViewAction);

    QAction *disableEffects = new QAction(i18n("Disable Bin Effects"), this);
    connect(disableEffects, &QAction::triggered, this, &Bin::slotDisableEffects);
    disableEffects->setIcon(KoIconUtils::themedIcon(QStringLiteral("favorite")));
    disableEffects->setData("disable_bin_effects");
    disableEffects->setCheckable(true);
    disableEffects->setChecked(false);
    pCore->window()->actionCollection()->addAction(QStringLiteral("disable_bin_effects"), disableEffects);

#if KXMLGUI_VERSION_MINOR > 24 || KXMLGUI_VERSION_MAJOR > 5
    m_renameAction = KStandardAction::renameFile(this, SLOT(slotRenameItem()), this);
    m_renameAction->setText(i18n("Rename"));
#else
    m_renameAction = new QAction(i18n("Rename"), this);
    connect(m_renameAction, &QAction::triggered, this, &Bin::slotRenameItem);
    m_renameAction->setShortcut(Qt::Key_F2);
#endif
    m_renameAction->setData("rename");
    pCore->window()->actionCollection()->addAction(QStringLiteral("rename"), m_renameAction);

    listType->setToolBarMode(KSelectAction::MenuMode);
    connect(listType, SIGNAL(triggered(QAction *)), this, SLOT(slotInitView(QAction *)));

    // Settings menu
    QMenu *settingsMenu = new QMenu(i18n("Settings"), this);
    settingsMenu->addAction(listType);
    QMenu *sliderMenu = new QMenu(i18n("Zoom"), this);
    sliderMenu->setIcon(KoIconUtils::themedIcon(QStringLiteral("zoom-in")));
    sliderMenu->addAction(widgetslider);
    settingsMenu->addMenu(sliderMenu);

    // Column show / hide actions
    m_showDate = new QAction(i18n("Show date"), this);
    m_showDate->setCheckable(true);
    connect(m_showDate, &QAction::triggered, this, &Bin::slotShowDateColumn);
    m_showDesc = new QAction(i18n("Show description"), this);
    m_showDesc->setCheckable(true);
    connect(m_showDesc, &QAction::triggered, this, &Bin::slotShowDescColumn);
    settingsMenu->addAction(m_showDate);
    settingsMenu->addAction(m_showDesc);
    settingsMenu->addAction(disableEffects);
    QToolButton *button = new QToolButton;
    button->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-menu")));
    button->setToolTip(i18n("Options"));
    button->setMenu(settingsMenu);
    button->setPopupMode(QToolButton::InstantPopup);
    m_toolbar->addWidget(button);

    // small info button for pending jobs
    m_infoLabel = new SmallJobLabel(this);
    m_infoLabel->setStyleSheet(SmallJobLabel::getStyleSheet(palette()));
    QAction *infoAction = m_toolbar->addWidget(m_infoLabel);
    m_jobsMenu = new QMenu(this);
    connect(m_jobsMenu, &QMenu::aboutToShow, this, &Bin::slotPrepareJobsMenu);
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

    // Hack, create toolbar spacer
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_toolbar->addWidget(spacer);

    // Add search line
    m_toolbar->addWidget(m_searchLine);

    m_binTreeViewDelegate = new BinItemDelegate(this);
    //connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
    m_headerInfo = QByteArray::fromBase64(KdenliveSettings::treeviewheaders().toLatin1());
    m_propertiesPanel = new QScrollArea(this);
    m_propertiesPanel->setFrameShape(QFrame::NoFrame);
    // Info widget for failed jobs, other errors
    m_infoMessage = new BinMessageWidget;
    m_layout->addWidget(m_infoMessage);
    m_infoMessage->setCloseButtonVisible(false);
    connect(m_infoMessage, &KMessageWidget::linkActivated, this, &Bin::slotShowJobLog);
    connect(m_infoMessage, &BinMessageWidget::messageClosing, this, &Bin::slotResetInfoMessage);
    //m_infoMessage->setWordWrap(true);
    m_infoMessage->hide();
    connect(this, SIGNAL(requesteInvalidRemoval(QString, QString, QString)), this, SLOT(slotQueryRemoval(QString, QString, QString)));
    connect(this, &Bin::refreshAudioThumbs, this, &Bin::doRefreshAudioThumbs);
    connect(this, SIGNAL(displayBinMessage(QString, KMessageWidget::MessageType)), this, SLOT(doDisplayMessage(QString, KMessageWidget::MessageType)));
}

Bin::~Bin()
{
    blockSignals(true);
    m_proxyModel->selectionModel()->blockSignals(true);
    setEnabled(false);
    abortOperations();
    delete m_infoMessage;
    delete m_propertiesPanel;
}

QDockWidget *Bin::clipPropertiesDock()
{
    return m_propertiesDock;
}

void Bin::slotAbortAudioThumb(const QString &id, long duration)
{
    if (!m_audioThumbsThread.isRunning()) {
        return;
    }
    QMutexLocker aMutex(&m_audioThumbMutex);
    if (m_audioThumbsList.removeAll(id) > 0) {
        m_audioDuration -= duration;
    }
}

void Bin::requestAudioThumbs(const QString &id, long duration)
{
    if (!m_audioThumbsList.contains(id) && m_processingAudioThumb != id) {
        m_audioThumbMutex.lock();
        m_audioThumbsList.append(id);
        m_audioDuration += duration;
        m_audioThumbMutex.unlock();
        processAudioThumbs();
    }
}

void Bin::doUpdateThumbsProgress(long ms)
{
    int progress = (m_processedAudio + ms) * 100 / m_audioDuration;
    emitMessage(i18n("Creating audio thumbnails"), progress, ProcessingJobMessage);
}

void Bin::processAudioThumbs()
{
    if (m_audioThumbsThread.isRunning()) {
        return;
    }
    m_audioThumbsThread = QtConcurrent::run(this, &Bin::slotCreateAudioThumbs);
}

void Bin::abortOperations()
{
    blockSignals(true);
    abortAudioThumbs();
    if (m_propertiesPanel) {
        foreach (QWidget *w, m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
            delete w;
        }
    }
    if (m_rootFolder) {
        while (!m_rootFolder->isEmpty()) {
            AbstractProjectItem *child = m_rootFolder->at(0);
            m_rootFolder->removeChild(child);
            delete child;
        }
    }
    delete m_rootFolder;
    m_rootFolder = nullptr;
    delete m_itemView;
    m_itemView = nullptr;
    delete m_jobManager;
    m_jobManager = nullptr;
    blockSignals(false);
}

void Bin::abortAudioThumbs()
{
    if (!m_audioThumbsThread.isRunning()) {
        return;
    }
    if (!m_processingAudioThumb.isEmpty()) {
        ProjectClip *clip = m_rootFolder->clip(m_processingAudioThumb);
        if (clip) {
            clip->abortAudioThumbs();
        }
    }
    m_audioThumbMutex.lock();
    foreach (const QString &id, m_audioThumbsList) {
        ProjectClip *clip = m_rootFolder->clip(id);
        if (clip) {
            clip->setJobStatus(AbstractClipJob::THUMBJOB, JobDone, 0);
        }
    }
    m_audioThumbsList.clear();
    m_audioThumbMutex.unlock();
    m_audioThumbsThread.waitForFinished();
}

void Bin::slotCreateAudioThumbs()
{
    int max = m_audioThumbsList.count();
    int count = 0;
    m_processedAudio = 0;
    while (!m_audioThumbsList.isEmpty()) {
        m_audioThumbMutex.lock();
        max = qMax(max, m_audioThumbsList.count());
        m_processingAudioThumb = m_audioThumbsList.takeFirst();
        count++;
        m_audioThumbMutex.unlock();
        ProjectClip *clip = m_rootFolder->clip(m_processingAudioThumb);
        if (clip) {
            clip->slotCreateAudioThumbs();
            m_processedAudio += clip->duration().ms();
        }
    }
    m_audioThumbMutex.lock();
    m_processingAudioThumb.clear();
    m_processedAudio = 0;
    m_audioDuration = 0;
    m_audioThumbMutex.unlock();
    emitMessage(i18n("Audio thumbnails done"), 100, OperationCompletedMessage);
}

bool Bin::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        m_monitor->slotActivateMonitor();
        bool success = QWidget::eventFilter(obj, event);
        if (m_gainedFocus) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            QAbstractItemView *view = qobject_cast<QAbstractItemView *>(obj->parent());
            if (view) {
                QModelIndex idx = view->indexAt(mouseEvent->pos());
                ClipController *ctl = nullptr;
                if (idx.isValid()) {
                    AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(idx).internalPointer());
                    if (item) {
                        ProjectClip *clip = qobject_cast<ProjectClip *>(item);
                        if (clip) {
                            ctl = clip->controller();
                        }
                    }
                }
                m_gainedFocus = false;
                editMasterEffect(ctl);
            }
            // make sure we discard the focus indicator
            m_gainedFocus = false;
        }
        return success;
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        QAbstractItemView *view = qobject_cast<QAbstractItemView *>(obj->parent());
        if (view) {
            QModelIndex idx = view->indexAt(mouseEvent->pos());
            if (!idx.isValid()) {
                // User double clicked on empty area
                slotAddClip();
            } else {
                slotItemDoubleClicked(idx, mouseEvent->pos());
            }
        } else {
            qCDebug(KDENLIVE_LOG) << " +++++++ NO VIEW-------!!";
        }
        return true;
    } else if (event->type() == QEvent::Wheel) {
        QWheelEvent *e = static_cast<QWheelEvent *>(event);
        if (e && e->modifiers() == Qt::ControlModifier) {
            slotZoomView(e->delta() > 0);
            //emit zoomView(e->delta() > 0);
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
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
    QList<QToolButton *> allButtons = this->findChildren<QToolButton *>();
    for (int i = 0; i < allButtons.count(); i++) {
        QToolButton *m = allButtons.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
}

void Bin::slotSaveHeaders()
{
    if (m_itemView && m_listType == BinTreeView) {
        // save current treeview state (column width)
        QTreeView *view = static_cast<QTreeView *>(m_itemView);
        m_headerInfo = view->header()->saveState();
        KdenliveSettings::setTreeviewheaders(m_headerInfo.toBase64());
    }
}

void Bin::slotZoomView(bool zoomIn)
{
    if (m_itemModel->rowCount() == 0) {
        //Don't zoom on empty bin
        return;
    }
    int progress = (zoomIn == true) ? 1 : -1;
    m_slider->setValue(m_slider->value() + progress);
}

Monitor *Bin::monitor()
{
    return m_monitor;
}

const QStringList Bin::getFolderInfo(const QModelIndex &selectedIx)
{
    QStringList folderInfo;
    QModelIndexList indexes;
    if (selectedIx.isValid()) {
        indexes << selectedIx;
    } else {
        indexes = m_proxyModel->selectionModel()->selectedIndexes();
    }
    if (indexes.isEmpty()) {
        // return root folder info
        folderInfo << QString::number(-1);
        folderInfo << QString();
        return folderInfo;
    }
    QModelIndex ix = indexes.first();
    if (ix.isValid() && (m_proxyModel->selectionModel()->isSelected(ix) || selectedIx.isValid())) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        while (currentItem->itemType() != AbstractProjectItem::FolderItem) {
            currentItem = currentItem->parent();
        }
        if (currentItem == m_rootFolder) {
            // clip was added to root folder, leave folder info empty
            folderInfo << QString::number(-1);
            folderInfo << QString();
        } else {
            folderInfo << currentItem->clipId();
            folderInfo << currentItem->name();
        }
    } else {
        // return root folder info
        folderInfo << QString::number(-1);
        folderInfo << QString();
    }
    return folderInfo;
}

void Bin::slotAddClip()
{
    // Check if we are in a folder
    QStringList folderInfo = getFolderInfo();
    ClipCreationDialog::createClipsCommand(m_doc, folderInfo, this);
}

void Bin::deleteClip(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
        emit openClip(nullptr);
    }
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        qCWarning(KDENLIVE_LOG) << "Cannot bin find clip to delete: " << id;
        return;
    }
    m_jobManager->discardJobs(id);
    ClipType type = clip->clipType();
    QString url = clip->url();
    clip->setClipStatus(AbstractProjectItem::StatusDeleting);
    AbstractProjectItem *parent = clip->parent();
    parent->removeChild(clip);
    delete clip;
    m_doc->deleteClip(id, type, url);
}

ProjectClip *Bin::getFirstSelectedClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return nullptr;
    }
    for (const QModelIndex &ix : indexes) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *clip = qobject_cast<ProjectClip *>(item);
        if (clip) {
            return clip;
        }
    }
    return nullptr;
}

void Bin::slotDeleteClip()
{
    QStringList clipIds;
    QStringList subClipIds;
    QStringList foldersIds;
    ProjectSubClip *sub = nullptr;
    QPoint zone;
    bool usedFolder = false;
    // check folders, remove child folders if there is any
    QList<ProjectFolder *> topFolders;
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        if (!item) {
            continue;
        }
        if (item->itemType() == AbstractProjectItem::SubClipItem) {
            QString subId = item->clipId();
            sub = static_cast<ProjectSubClip *>(item);
            zone = sub->zone();
            subId.append(QLatin1Char(':') + QString::number(zone.x()) + QLatin1Char(':') + QString::number(zone.y()));
            subClipIds << subId;
            continue;
        }
        if (item->itemType() != AbstractProjectItem::FolderItem) {
            continue;
        }
        ProjectFolder *current = static_cast<ProjectFolder *>(item);
        if (!usedFolder && !current->isEmpty()) {
            usedFolder = true;
        }
        if (topFolders.isEmpty()) {
            topFolders << current;
            continue;
        }
        // parse all folders to check for children
        bool isChild = false;
        foreach (ProjectFolder *f, topFolders) {
            if (f->folder(current->clipId())) {
                // Current is a child, no need to take it into account
                isChild = true;
                break;
            }
        }
        if (isChild) {
            continue;
        }
        QList<ProjectFolder *> childFolders;
        // parse all folders to check for children
        foreach (ProjectFolder *f, topFolders) {
            if (current->folder(f->clipId())) {
                childFolders << f;
            }
        }
        if (!childFolders.isEmpty()) {
            // children are in the list, remove from
            foreach (ProjectFolder *f, childFolders) {
                topFolders.removeAll(f);
            }
        }
        topFolders << current;
    }
    foreach (const ProjectFolder *f, topFolders) {
        foldersIds << f->clipId();
    }
    bool usedClips = false;
    QList<ProjectFolder *> topClips;
    // Check if clips are in already selected folders
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        if (!item || item->itemType() != AbstractProjectItem::ClipItem) {
            continue;
        }
        ProjectClip *current = static_cast<ProjectClip *>(item);
        bool isChild = false;
        foreach (const ProjectFolder *f, topFolders) {
            if (current->hasParent(f->clipId())) {
                isChild = true;
                break;
            }
        }
        if (!isChild) {
            if (!usedClips && current->refCount() > 0) {
                usedClips = true;
            }
            clipIds << current->clipId();
        }
    }
    if (usedClips && (KMessageBox::warningContinueCancel(this, i18n("This will delete all selected clips from timeline")) != KMessageBox::Continue)) {
        return;
    } else if (usedFolder && (KMessageBox::warningContinueCancel(this, i18n("This will delete all folder content")) != KMessageBox::Continue)) {
        return;
    }
    m_doc->clipManager()->deleteProjectItems(clipIds, foldersIds, subClipIds);
}

void Bin::slotReloadClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip *>(item);
        if (currentItem) {
            emit openClip(nullptr);
            if (currentItem->clipType() == Playlist) {
                //Check if a clip inside playlist is missing
                QString path = currentItem->url();
                QFile f(path);
                QDomDocument doc;
                doc.setContent(&f, false);
                f.close();
                DocumentChecker d(QUrl::fromLocalFile(path), doc);
                if (!d.hasErrorInClips() && doc.documentElement().attribute(QStringLiteral("modified")) == QLatin1String("1")) {
                    QString backupFile = path + QStringLiteral(".backup");
                    KIO::FileCopyJob *copyjob = KIO::file_copy(QUrl::fromLocalFile(path), QUrl::fromLocalFile(backupFile));
                    if (copyjob->exec()) {
                        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            KMessageBox::sorry(this, i18n("Unable to write to file %1", path));
                        } else {
                            QTextStream out(&f);
                            out << doc.toString();
                            f.close();
                            KMessageBox::information(this, i18n("Your project file was modified by Kdenlive.\nTo make sure you don't lose data, a backup copy called %1 was created.", backupFile));
                        }
                    }
                }
            }
            QDomDocument doc;
            QDomElement xml = currentItem->toXml(doc);
            qCDebug(KDENLIVE_LOG) << "*****************\n" << doc.toString() << "\n******************";
            if (!xml.isNull()) {
                currentItem->setClipStatus(AbstractProjectItem::StatusWaiting);
                // We need to set a temporary id before all outdated producers are replaced;
                m_doc->getFileProperties(xml, currentItem->clipId(), 150, true);
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
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip *>(item);
        if (currentItem) {
            QUrl url = QUrl::fromLocalFile(currentItem->url()).adjusted(QUrl::RemoveFilename);
            bool exists = QFile(url.toLocalFile()).exists();
            if (currentItem->hasUrl() && exists) {
                QDesktopServices::openUrl(url);
                qCDebug(KDENLIVE_LOG) << "  / / " + url.toString();
            } else {
                if(!exists) {
                    emitMessage(i18n("Couldn't locate ") + QString(" (" + url.toString() + QLatin1Char(')')), 100, ErrorMessage);
                }
                return;
            }
        }
    }
}

void Bin::slotDuplicateClip()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip *>(item);
        if (currentItem) {
            QStringList folderInfo = getFolderInfo(ix);
            QDomDocument doc;
            QDomElement xml = currentItem->toXml(doc);
            if (!xml.isNull()) {
                QString currentName = EffectsList::property(xml, QStringLiteral("kdenlive:clipname"));
                if (currentName.isEmpty()) {
                    QUrl url = QUrl::fromLocalFile(EffectsList::property(xml, QStringLiteral("resource")));
                    if (url.isValid()) {
                        currentName = url.fileName();
                    }
                }
                if (!currentName.isEmpty()) {
                    currentName.append(i18nc("append to clip name to indicate a copied idem", " (copy)"));
                    EffectsList::setProperty(xml, QStringLiteral("kdenlive:clipname"), currentName);
                }
                ClipCreationDialog::createClipFromXml(m_doc, xml, folderInfo, this);
            }
        }
    }
}

ProjectFolder *Bin::rootFolder()
{
    return m_rootFolder;
}

void Bin::setMonitor(Monitor *monitor)
{
    m_monitor = monitor;
    connect(m_monitor, SIGNAL(addClipToProject(QUrl)), this, SLOT(slotAddClipToProject(QUrl)));
    connect(m_monitor, SIGNAL(requestAudioThumb(QString)), this, SLOT(slotSendAudioThumb(QString)));
    connect(m_monitor, &Monitor::refreshCurrentClip, this, &Bin::slotOpenCurrent);
    connect(m_monitor, SIGNAL(updateClipMarker(QString,QList<CommentedTime>)), this, SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));
    connect(this, &Bin::openClip, m_monitor, &Monitor::slotOpenClip);
}

int Bin::getFreeFolderId()
{
    return m_folderCounter++;
}

int Bin::getFreeClipId()
{
    return m_clipCounter++;
}

int Bin::lastClipId() const
{
    return qMax(0, m_clipCounter - 1);
}

void Bin::setDocument(KdenliveDoc *project)
{
    // Remove clip from Bin's monitor
    if (m_doc) {
        emit openClip(nullptr);
    }
    m_infoMessage->hide();
    blockSignals(true);
    m_proxyModel->selectionModel()->blockSignals(true);
    setEnabled(false);

    // Cleanup previous project
    if (m_rootFolder) {
        while (!m_rootFolder->isEmpty()) {
            AbstractProjectItem *child = m_rootFolder->at(0);
            m_rootFolder->removeChild(child);
            delete child;
        }
    }
    delete m_rootFolder;
    delete m_itemView;
    m_itemView = nullptr;
    delete m_jobManager;
    m_clipCounter = 1;
    m_folderCounter = 1;
    m_doc = project;
    int iconHeight = QFontInfo(font()).pixelSize() * 3.5;
    m_iconSize = QSize(iconHeight * m_doc->dar(), iconHeight);
    m_jobManager = new JobManager(this);
    m_rootFolder = new ProjectFolder(this);
    setEnabled(true);
    blockSignals(false);
    m_proxyModel->selectionModel()->blockSignals(false);
    connect(m_jobManager, SIGNAL(addClip(QString, int)), this, SLOT(slotAddUrl(QString, int)));
    connect(m_proxyAction, SIGNAL(toggled(bool)), m_doc, SLOT(slotProxyCurrentItem(bool)));
    connect(m_jobManager, &JobManager::jobCount, m_infoLabel, &SmallJobLabel::slotSetJobCount);
    connect(m_discardCurrentClipJobs, &QAction::triggered, m_jobManager, &JobManager::slotDiscardClipJobs);
    connect(m_cancelJobs, &QAction::triggered, m_jobManager, &JobManager::slotCancelJobs);
    connect(m_discardPendingJobs, &QAction::triggered, m_jobManager, &JobManager::slotCancelPendingJobs);
    connect(m_jobManager, &JobManager::updateJobStatus, this, &Bin::slotUpdateJobStatus);

    connect(m_jobManager, SIGNAL(gotFilterJobResults(QString, int, int, stringMap, stringMap)), this, SLOT(slotGotFilterJobResults(QString, int, int, stringMap, stringMap)));

    //connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), m_itemView
    //connect(m_itemModel, SIGNAL(updateCurrentItem()), this, SLOT(autoSelect()));
    slotInitView(nullptr);
    bool binEffectsDisabled = getDocumentProperty(QStringLiteral("disablebineffects")).toInt() == 1;
    setBinEffectsDisabledStatus(binEffectsDisabled);
    autoSelect();
}

void Bin::slotAddUrl(const QString &url, int folderId, const QMap<QString, QString> &data)
{
    const QList<QUrl> urls = QList<QUrl>() << QUrl::fromLocalFile(url);
    QStringList folderInfo;
    if (folderId >= 0) {
        QModelIndex ix = getIndexForId(QString::number(folderId), true);
        if (ix.isValid()) {
            folderInfo = getFolderInfo(m_proxyModel->mapFromSource(ix));
        } else {
            folderInfo = getFolderInfo();
        }
    }
    ClipCreationDialog::createClipsCommand(m_doc, urls, folderInfo, this, data);
}

void Bin::slotAddUrl(const QString &url, const QMap<QString, QString> &data)
{
    const QList<QUrl> urls = QList<QUrl>() << QUrl::fromLocalFile(url);
    const QStringList folderInfo = getFolderInfo();
    ClipCreationDialog::createClipsCommand(m_doc, urls, folderInfo, this, data);
}

void Bin::createClip(const QDomElement &xml)
{
    // Check if clip should be in a folder
    QString groupId = ProjectClip::getXmlProperty(xml, QStringLiteral("kdenlive:folderid"));
    ProjectFolder *parentFolder = m_rootFolder;
    if (!groupId.isEmpty()) {
        parentFolder = m_rootFolder->folder(groupId);
        if (!parentFolder) {
            // parent folder does not exist, put in root folder
            parentFolder = m_rootFolder;
        }
    }
    QString path = EffectsList::property(xml, QStringLiteral("resource"));
    if (path.endsWith(QStringLiteral(".mlt")) || path.endsWith(QStringLiteral(".kdenlive"))) {
        QFile f(path);
        QDomDocument doc;
        doc.setContent(&f, false);
        f.close();
        DocumentChecker d(QUrl::fromLocalFile(path), doc);
        if (!d.hasErrorInClips() && doc.documentElement().attribute(QStringLiteral("modified")) == QLatin1String("1")) {
            QString backupFile = path + QStringLiteral(".backup");
            KIO::FileCopyJob *copyjob = KIO::file_copy(QUrl::fromLocalFile(path), QUrl::fromLocalFile(backupFile));
            if (copyjob->exec()) {
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    KMessageBox::sorry(this, i18n("Unable to write to file %1", path));
                } else {
                    QTextStream out(&f);
                    out << doc.toString();
                    f.close();
                    KMessageBox::information(this, i18n("Your project file was modified by Kdenlive.\nTo make sure you don't lose data, a backup copy called %1 was created.", backupFile));
                }
            }
        }
    }
    new ProjectClip(xml, m_blankThumb, parentFolder);
}

QString Bin::slotAddFolder(const QString &folderName)
{
    // Check parent item
    QModelIndex ix = m_proxyModel->selectionModel()->currentIndex();
    ProjectFolder *parentFolder  = m_rootFolder;
    if (ix.isValid() && m_proxyModel->selectionModel()->isSelected(ix)) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        while (currentItem->itemType() != AbstractProjectItem::FolderItem) {
            currentItem = currentItem->parent();
        }
        if (currentItem->itemType() == AbstractProjectItem::FolderItem) {
            parentFolder = qobject_cast<ProjectFolder *>(currentItem);
        }
    }
    QString newId = QString::number(getFreeFolderId());
    AddBinFolderCommand *command = new AddBinFolderCommand(this, newId, folderName.isEmpty() ? i18n("Folder") : folderName, parentFolder->clipId());
    m_doc->commandStack()->push(command);

    // Edit folder name
    if (!folderName.isEmpty()) {
        // We already have a name, no need to edit
        return newId;
    }
    ix = getIndexForId(newId, true);
    if (ix.isValid()) {
        m_proxyModel->selectionModel()->clearSelection();
        int row = ix.row();
        const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
        const QModelIndex id2 = m_itemModel->index(row, m_rootFolder->supportedDataCount() - 1, ix.parent());
        if (id.isValid() && id2.isValid()) {
            m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)), QItemSelectionModel::Select);
        }
        m_itemView->edit(m_proxyModel->mapFromSource(ix));
    }
    return newId;
}

QModelIndex Bin::getIndexForId(const QString &id, bool folderWanted) const
{
    QModelIndexList items = m_itemModel->match(m_itemModel->index(0, 0), AbstractProjectItem::DataId, QVariant::fromValue(id), 2, Qt::MatchRecursive);
    for (int i = 0; i < items.count(); i++) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(items.at(i).internalPointer());
        AbstractProjectItem::PROJECTITEMTYPE type = currentItem->itemType();
        if (folderWanted && type == AbstractProjectItem::FolderItem) {
            // We found our folder
            return items.at(i);
        } else if (!folderWanted && type == AbstractProjectItem::ClipItem) {
            // We found our clip
            return items.at(i);
        }
    }
    return QModelIndex();
}

void Bin::selectClipById(const QString &clipId, int frame, const QPoint &zone)
{
    if (m_monitor->activeClipId() == clipId) {
        if (frame > -1) {
            m_monitor->slotSeek(frame);
        }
        if (!zone.isNull()) {
            m_monitor->slotZoneMoved(zone.x(), zone.y());
        }
        return;
    }
    QModelIndex ix = getIndexForId(clipId, false);
    if (ix.isValid()) {
        m_proxyModel->selectionModel()->clearSelection();
        int row = ix.row();
        const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
        const QModelIndex id2 = m_itemModel->index(row, m_rootFolder->supportedDataCount() - 1, ix.parent());
        if (id.isValid() && id2.isValid()) {
            m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)), QItemSelectionModel::Select);
        }
        selectProxyModel(m_proxyModel->mapFromSource(ix));
        m_itemView->scrollTo(m_proxyModel->mapFromSource(ix));
        if (frame > -1) {
            m_monitor->slotSeek(frame);
        }
        if (!zone.isNull()) {
            m_monitor->slotZoneMoved(zone.x(), zone.y());
        }
    }
}

void Bin::doAddFolder(const QString &id, const QString &name, const QString &parentId)
{
    ProjectFolder *parentFolder = m_rootFolder->folder(parentId);
    if (!parentFolder) {
        qCDebug(KDENLIVE_LOG) << "  / / ERROR IN PARENT FOLDER";
        return;
    }
    //FIXME(style): constructor actually adds the new pointer to parent's children
    new ProjectFolder(id, name, parentFolder);
    emit storeFolder(id, parentId, QString(), name);
}

void Bin::renameFolder(const QString &id, const QString &name)
{
    ProjectFolder *folder = m_rootFolder->folder(id);
    if (!folder || !folder->parent()) {
        qCDebug(KDENLIVE_LOG) << "  / / ERROR IN PARENT FOLDER";
        return;
    }
    folder->setName(name);
    emit itemUpdated(folder);
    emit storeFolder(id, folder->parent()->clipId(), QString(), name);
}

void Bin::slotLoadFolders(const QMap<QString, QString> &foldersData)
{
    // Folder parent is saved in folderId, separated by a dot. for example "1.3" means parent folder id is "1" and new folder id is "3".
    ProjectFolder *parentFolder;
    QStringList folderIds = foldersData.keys();
    int maxIterations = folderIds.count() * folderIds.count();
    int iterations = 0;
    while (!folderIds.isEmpty()) {
        QString id = folderIds.takeFirst();
        QString parentId = id.section(QLatin1Char('.'), 0, 0);
        if (parentId == QLatin1String("-1")) {
            parentFolder = m_rootFolder;
        } else {
            // This is a sub-folder
            parentFolder = m_rootFolder->folder(parentId);
            if (parentFolder == m_rootFolder) {
                // parent folder not yet created, create unnamed placeholder
                parentFolder = new ProjectFolder(parentId, QString(), parentFolder);
            } else if (parentFolder == nullptr) {
                // Parent folder not yet created in hierarchy
                if (iterations > maxIterations) {
                    // Give up, place folder in root
                    parentFolder = new ProjectFolder(parentId, i18n("Folder"), m_rootFolder);
                } else {
                    // Try to process again at end of queue
                    folderIds.append(id);
                    iterations ++;
                    continue;
                }
            }
        }
        // parent was found, create our folder
        QString folderId = id.section(QLatin1Char('.'), 1, 1);
        int numericId = folderId.toInt();
        if (numericId >= m_folderCounter) {
            m_folderCounter = numericId + 1;
        }
        // Check if placeholder folder was created
        ProjectFolder *placeHolder = parentFolder->folder(folderId);
        if (placeHolder) {
            // Rename placeholder
            placeHolder->setName(foldersData.value(id));
        } else {
            // Create new folder
            //FIXME(style): constructor actually adds the new pointer to parent's children
            new ProjectFolder(folderId, foldersData.value(id), parentFolder);
        }
    }
}

void Bin::removeFolder(const QString &id, QUndoCommand *deleteCommand)
{
    // Check parent item
    ProjectFolder *folder = m_rootFolder->folder(id);
    AbstractProjectItem *parent = folder->parent();
    if (!folder->isEmpty()) {
        // Folder has clips inside, warn user
        if (KMessageBox::warningContinueCancel(this, i18np("Folder contains a clip, delete anyways ?", "Folder contains %1 clips, delete anyways ?", folder->count())) != KMessageBox::Continue) {
            return;
        }
        QStringList clipIds;
        QStringList folderIds;
        // TODO: manage subclips
        for (int i = 0; i < folder->count(); i++) {
            AbstractProjectItem *child = folder->at(i);
            switch (child->itemType()) {
            case AbstractProjectItem::ClipItem:
                clipIds << child->clipId();
                break;
            case AbstractProjectItem::FolderItem:
                folderIds << child->clipId();
                break;
            default:
                break;
            }
        }
        m_doc->clipManager()->deleteProjectItems(clipIds, folderIds, QStringList(), deleteCommand);
    }
    new AddBinFolderCommand(this, folder->clipId(), folder->name(), parent->clipId(), true, deleteCommand);
}

void Bin::removeSubClip(const QString &id, QUndoCommand *deleteCommand)
{
    // Check parent item
    QString clipId = id;
    int in = clipId.section(QLatin1Char(':'), 1, 1).toInt();
    int out = clipId.section(QLatin1Char(':'), 2, 2).toInt();
    clipId = clipId.section(QLatin1Char(':'), 0, 0);
    new AddBinClipCutCommand(this, clipId, in, out, false, deleteCommand);
}

void Bin::doRemoveFolder(const QString &id)
{
    ProjectFolder *folder = m_rootFolder->folder(id);
    if (!folder) {
        qCDebug(KDENLIVE_LOG) << "  / / FOLDER not found";
        return;
    }
    //TODO: warn user on non-empty folders
    AbstractProjectItem *parent = folder->parent();
    parent->removeChild(folder);
    emit storeFolder(id, parent->clipId(), QString(), QString());
    delete folder;
}

void Bin::emitAboutToAddItem(AbstractProjectItem *item)
{
    m_itemModel->onAboutToAddItem(item);
}

void Bin::emitItemAdded(AbstractProjectItem *item)
{
    m_itemModel->onItemAdded(item);
    if (!m_proxyModel->selectionModel()->hasSelection()) {
        QModelIndex ix = getIndexForId(item->clipId(), item->itemType() == AbstractProjectItem::FolderItem);
        int row = ix.row();
        if (row < 0) {
            row = item->index();
        }
        const QModelIndex id = m_itemModel->index(row, 0, ix.parent());
        const QModelIndex id2 = m_itemModel->index(row, m_rootFolder->supportedDataCount() - 1, ix.parent());
        m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)), QItemSelectionModel::Select);
        selectProxyModel(m_proxyModel->mapFromSource(id));
    }
}

void Bin::emitAboutToRemoveItem(AbstractProjectItem *item)
{
    QModelIndex ix = m_proxyModel->mapFromSource(getIndexForId(item->clipId(), item->itemType() == AbstractProjectItem::FolderItem));
    m_itemModel->onAboutToRemoveItem(item);
    int row = ix.row();
    if (row > 0 && item->itemType() != AbstractProjectItem::SubClipItem) {
        // Go one level up to select upper item (not on subclip because ix is the parent index for subclips)
        row--;
    }
    if (!m_proxyModel->selectionModel()->hasSelection() || m_proxyModel->selectionModel()->isSelected(ix)) {
        // we have to select item above deletion
        QModelIndex id = m_proxyModel->index(row, 0, ix.parent());
        QModelIndex id2 = m_proxyModel->index(row, m_rootFolder->supportedDataCount() - 1, ix.parent());
        if (id.isValid() && id2.isValid()) {
            m_proxyModel->selectionModel()->select(QItemSelection(id, id2), QItemSelectionModel::Select);
        }
    }
}

void Bin::emitItemRemoved(AbstractProjectItem *item)
{
    m_itemModel->onItemRemoved(item);
}

void Bin::rowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    //Moved selection stuff to emitItemAdded otherwise selection is messaed up by sorting
}

void Bin::rowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    //Moved selection stuff to emitAboutToRemoveItem otherwise selection is messaed up by sorting
}

void Bin::selectProxyModel(const QModelIndex &id)
{
    if (isLoading) {
        //return;
    }
    if (id.isValid()) {
        if (id.column() != 0) {
            return;
        }
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(id).internalPointer());
        if (currentItem) {
            // Set item as current so that it displays its content in clip monitor
            currentItem->setCurrent(true);
            if (currentItem->itemType() == AbstractProjectItem::ClipItem) {
                m_reloadAction->setEnabled(true);
                m_locateAction->setEnabled(true);
                m_duplicateAction->setEnabled(true);
                ClipType type = static_cast<ProjectClip *>(currentItem)->clipType();
                m_openAction->setEnabled(type == Image || type == Audio || type == Text || type == TextTemplate);
                showClipProperties(static_cast<ProjectClip *>(currentItem), false);
                m_deleteAction->setText(i18n("Delete Clip"));
                m_proxyAction->setText(i18n("Proxy Clip"));
                emit findInTimeline(currentItem->clipId());
            } else if (currentItem->itemType() == AbstractProjectItem::FolderItem) {
                // A folder was selected, disable editing clip
                m_openAction->setEnabled(false);
                m_reloadAction->setEnabled(false);
                m_locateAction->setEnabled(false);
                m_duplicateAction->setEnabled(false);
                m_deleteAction->setText(i18n("Delete Folder"));
                m_proxyAction->setText(i18n("Proxy Folder"));
            } else if (currentItem->itemType() == AbstractProjectItem::SubClipItem) {
                showClipProperties(static_cast<ProjectClip *>(currentItem->parent()), false);
                m_openAction->setEnabled(false);
                m_reloadAction->setEnabled(false);
                m_locateAction->setEnabled(false);
                m_duplicateAction->setEnabled(false);
                m_deleteAction->setText(i18n("Delete Clip"));
                m_proxyAction->setText(i18n("Proxy Clip"));
            }
            m_deleteAction->setEnabled(true);
        } else {
            emit findInTimeline(QString());
            m_reloadAction->setEnabled(false);
            m_locateAction->setEnabled(false);
            m_duplicateAction->setEnabled(false);
            m_openAction->setEnabled(false);
            m_deleteAction->setEnabled(false);
        }
    } else {
        // No item selected in bin
        m_openAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
        showClipProperties(nullptr);
        emit findInTimeline(QString());
        emit masterClipSelected(nullptr, m_monitor);
        // Display black bg in clip monitor
        emit openClip(nullptr);
    }
}

void Bin::autoSelect()
{
    /*QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(current).internalPointer());
    if (!currentItem) {
        QModelIndex id = m_proxyModel->index(0, 0, QModelIndex());
        //selectModel(id);
        //m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    }*/
}

QList<ProjectClip *> Bin::selectedClips()
{
    //TODO: handle clips inside folders
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QList<ProjectClip *> list;
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        ProjectClip *currentItem = qobject_cast<ProjectClip *>(item);
        if (currentItem) {
            list << currentItem;
        }
    }
    return list;
}

void Bin::slotInitView(QAction *action)
{
    if (action) {
        m_proxyModel->selectionModel()->clearSelection();
        int viewType = action->data().toInt();
        KdenliveSettings::setBinMode(viewType);
        if (viewType == m_listType) {
            return;
        }
        if (m_listType == BinTreeView) {
            // save current treeview state (column width)
            QTreeView *view = static_cast<QTreeView *>(m_itemView);
            m_headerInfo = view->header()->saveState();
            m_showDate->setEnabled(true);
            m_showDesc->setEnabled(true);
        } else {
            // remove the current folderUp item if any
            if (m_folderUp) {
                if (m_folderUp->parent()) {
                    m_folderUp->parent()->removeChild(m_folderUp);
                }
                delete m_folderUp;
                m_folderUp = nullptr;
            }
        }
        m_listType = static_cast<BinViewType>(viewType);
    }
    if (m_itemView) {
        delete m_itemView;
    }

    switch (m_listType) {
    case BinIconView:
        m_itemView = new MyListView(this);
        m_folderUp = new ProjectFolderUp(nullptr);
        m_showDate->setEnabled(false);
        m_showDesc->setEnabled(false);
        break;
    default:
        m_itemView = new MyTreeView(this);
        m_showDate->setEnabled(true);
        m_showDesc->setEnabled(true);
        break;
    }
    m_itemView->setMouseTracking(true);
    m_itemView->viewport()->installEventFilter(this);
    QSize zoom = m_iconSize * (m_slider->value() / 4.0);
    m_itemView->setIconSize(zoom);
    QPixmap pix(zoom);
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
    m_itemView->setModel(m_proxyModel);
    m_itemView->setSelectionModel(m_proxyModel->selectionModel());
    m_layout->insertWidget(1, m_itemView);

    // setup some default view specific parameters
    if (m_listType == BinTreeView) {
        m_itemView->setItemDelegate(m_binTreeViewDelegate);
        MyTreeView *view = static_cast<MyTreeView *>(m_itemView);
        view->setSortingEnabled(true);
        view->setWordWrap(true);
        connect(m_proxyModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &Bin::slotSetSorting);
        m_proxyModel->setDynamicSortFilter(true);
        if (!m_headerInfo.isEmpty()) {
            view->header()->restoreState(m_headerInfo);
        } else {
            view->header()->resizeSections(QHeaderView::ResizeToContents);
            view->resizeColumnToContents(0);
            view->setColumnHidden(1, true);
            view->setColumnHidden(2, true);
        }
        m_showDate->setChecked(!view->isColumnHidden(1));
        m_showDesc->setChecked(!view->isColumnHidden(2));
        connect(view->header(), &QHeaderView::sectionResized, this, &Bin::slotSaveHeaders);
        connect(view->header(), &QHeaderView::sectionClicked, this, &Bin::slotSaveHeaders);
        connect(view, &MyTreeView::focusView, this, &Bin::slotGotFocus);
    } else if (m_listType == BinIconView) {
        MyListView *view = static_cast<MyListView *>(m_itemView);
        connect(view, &MyListView::focusView, this, &Bin::slotGotFocus);
    }
    m_itemView->setEditTriggers(QAbstractItemView::NoEditTriggers); //DoubleClicked);
    m_itemView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_itemView->setDragDropMode(QAbstractItemView::DragDrop);
    m_itemView->setAlternatingRowColors(true);
    m_itemView->setAcceptDrops(true);
    m_itemView->setFocus();
}

void Bin::slotSetIconSize(int size)
{
    if (!m_itemView) {
        return;
    }
    KdenliveSettings::setBin_zoom(size);
    QSize zoom = m_iconSize;
    zoom = zoom * (size / 4.0);
    m_itemView->setIconSize(zoom);
    QPixmap pix(zoom);
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
}

void Bin::rebuildMenu()
{
    m_transcodeAction = static_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("transcoders"), pCore->window()));
    m_extractAudioAction = static_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("extract_audio"), pCore->window()));
    m_clipsActionsMenu = static_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("clip_actions"), pCore->window()));
    m_menu->insertMenu(m_reloadAction, m_extractAudioAction);
    m_menu->insertMenu(m_reloadAction, m_transcodeAction);
    m_menu->insertMenu(m_reloadAction, m_clipsActionsMenu);
    m_inTimelineAction = m_menu->insertMenu(m_reloadAction, static_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("clip_in_timeline"), pCore->window())));

}

void Bin::contextMenuEvent(QContextMenuEvent *event)
{
    bool enableClipActions = false;
    ClipType type = Unknown;
    bool isFolder = false;
    bool isImported = false;
    QString clipService;
    QString audioCodec;
    if (m_itemView) {
        QModelIndex idx = m_itemView->indexAt(m_itemView->viewport()->mapFromGlobal(event->globalPos()));
        if (idx.isValid()) {
            // User right clicked on a clip
            AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(idx).internalPointer());
            if (currentItem) {
                enableClipActions = true;
                if (currentItem->itemType() == AbstractProjectItem::FolderItem) {
                    isFolder = true;
                } else {
                    ProjectClip *clip = qobject_cast<ProjectClip *>(currentItem);
                    if (clip) {
                        m_proxyAction->blockSignals(true);
                        emit findInTimeline(clip->clipId());
                        clipService = clip->getProducerProperty(QStringLiteral("mlt_service"));
                        m_proxyAction->setChecked(clip->hasProxy());
                        QList<QAction *> transcodeActions;
                        if (m_transcodeAction) {
                            transcodeActions = m_transcodeAction->actions();
                        }
                        QStringList data;
                        QString condition;
                        audioCodec = clip->codec(true);
                        QString videoCodec = clip->codec(false);
                        type = clip->clipType();
                        if (clip->hasUrl()) {
                            isImported = true;
                        }
                        bool noCodecInfo = false;
                        if (audioCodec.isEmpty() && videoCodec.isEmpty()) {
                            noCodecInfo = true;
                        }
                        for (int i = 0; i < transcodeActions.count(); ++i) {
                            data = transcodeActions.at(i)->data().toStringList();
                            if (data.count() > 4) {
                                condition = data.at(4);
                                if (condition.isEmpty()) {
                                    transcodeActions.at(i)->setEnabled(true);
                                    continue;
                                }
                                if (noCodecInfo) {
                                    // No audio / video codec, this is an MLT clip, disable conditionnal transcoding
                                    transcodeActions.at(i)->setEnabled(false);
                                    continue;
                                }
                                if (condition.startsWith(QLatin1String("vcodec"))) {
                                    transcodeActions.at(i)->setEnabled(condition.section(QLatin1Char('='), 1, 1) == videoCodec);
                                } else if (condition.startsWith(QLatin1String("acodec"))) {
                                    transcodeActions.at(i)->setEnabled(condition.section(QLatin1Char('='), 1, 1) == audioCodec);
                                }
                            }
                        }
                    }
                    m_proxyAction->blockSignals(false);
                }
            }
        }
    }
    // Enable / disable clip actions
    m_proxyAction->setEnabled(m_doc->getDocumentProperty(QStringLiteral("enableproxy")).toInt() && enableClipActions);
    m_openAction->setEnabled(type == Image || type == Audio || type == TextTemplate || type == Text);
    m_reloadAction->setEnabled(enableClipActions);
    m_locateAction->setEnabled(enableClipActions);
    m_duplicateAction->setEnabled(enableClipActions);
    m_editAction->setVisible(!isFolder);
    m_clipsActionsMenu->setEnabled(enableClipActions);
    m_extractAudioAction->setEnabled(enableClipActions);
    m_openAction->setVisible(!isFolder);
    m_reloadAction->setVisible(!isFolder);
    m_duplicateAction->setVisible(!isFolder);
    m_inTimelineAction->setVisible(!isFolder);
    if (m_transcodeAction) {
        m_transcodeAction->setEnabled(enableClipActions);
        m_transcodeAction->menuAction()->setVisible(!isFolder && clipService.contains(QStringLiteral("avformat")));
    }
    m_clipsActionsMenu->menuAction()->setVisible(!isFolder && (clipService.contains(QStringLiteral("avformat")) || clipService.contains(QStringLiteral("xml")) || clipService.contains(QStringLiteral("consumer"))));
    m_extractAudioAction->menuAction()->setVisible(!isFolder && !audioCodec.isEmpty());
    m_locateAction->setVisible(!isFolder && (isImported));

    // Show menu
    event->setAccepted(true);
    if (enableClipActions) {
        m_menu->exec(event->globalPos());
    } else {
        // Clicked in empty area
        m_addButton->menu()->exec(event->globalPos());
    }
}

void Bin::slotItemDoubleClicked(const QModelIndex &ix, const QPoint pos)
{
    AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
    if (m_listType == BinIconView) {
        if (item->count() > 0 || item->itemType() == AbstractProjectItem::FolderItem) {
            m_folderUp->setParent(item);
            m_itemView->setRootIndex(ix);
            return;
        }
        if (item == m_folderUp) {
            AbstractProjectItem *parentItem = item->parent();
            QModelIndex parent = getIndexForId(parentItem->parent()->clipId(), parentItem->parent()->itemType() == AbstractProjectItem::FolderItem);
            if (parentItem->parent() != m_rootFolder) {
                // We are entering a parent folder
                m_folderUp->setParent(parentItem->parent());
            } else {
                m_folderUp->setParent(nullptr);
            }
            m_itemView->setRootIndex(m_proxyModel->mapFromSource(parent));
            return;
        }
    } else {
        if (item->count() > 0) {
            QTreeView *view = static_cast<QTreeView *>(m_itemView);
            view->setExpanded(ix, !view->isExpanded(ix));
            return;
        }
    }
    if (ix.isValid()) {
        QRect IconRect = m_itemView->visualRect(ix);
        IconRect.setSize(m_itemView->iconSize());
        if (!pos.isNull() && ((ix.column() == 2 && item->itemType() == AbstractProjectItem::ClipItem) || !IconRect.contains(pos))) {
            // User clicked outside icon, trigger rename
            m_itemView->edit(ix);
            return;
        }
        if (item->itemType() == AbstractProjectItem::ClipItem) {
            ProjectClip *clip = static_cast<ProjectClip *>(item);
            if (clip) {
                if (clip->clipType() == Text || clip->clipType() == TextTemplate) {
                    //m_propertiesPanel->setEnabled(false);
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
    QString panelId = m_propertiesPanel->property("clipId").toString();
    QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(current).internalPointer());
    if (item->clipId() != panelId) {
        // wrong clip
        return;
    }
    ProjectClip *clip = qobject_cast<ProjectClip *>(item);
    switch (clip->clipType()) {
    case Text:
    case TextTemplate:
        showTitleWidget(clip);
        break;
    case SlideShow:
        showSlideshowWidget(clip);
        break;
    case QText:
        ClipCreationDialog::createQTextClip(m_doc, getFolderInfo(), this, clip);
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
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(current).internalPointer());
        ProjectClip *clip = qobject_cast<ProjectClip *>(item);
        if (clip) {
            slotSwitchClipProperties(clip);
            return;
        }
    }
    slotSwitchClipProperties(nullptr);
}

void Bin::slotSwitchClipProperties(ProjectClip *clip)
{
    if (clip == nullptr) {
        m_propertiesPanel->setEnabled(false);
        return;
    }
    if (clip->clipType() == SlideShow) {
        m_propertiesPanel->setEnabled(false);
        showSlideshowWidget(clip);
    } else if (clip->clipType() == QText) {
        m_propertiesPanel->setEnabled(false);
        ClipCreationDialog::createQTextClip(m_doc, getFolderInfo(), this, clip);
    } else {
        m_propertiesPanel->setEnabled(true);
        showClipProperties(clip);
        m_propertiesDock->show();
        m_propertiesDock->raise();
    }
    // Check if properties panel is not tabbed under Bin
    //if (!pCore->window()->isTabbedWith(m_propertiesDock, QStringLiteral("project_bin"))) {
}

void Bin::doRefreshPanel(const QString &id)
{
    ProjectClip *currentItem = getFirstSelectedClip();
    if (currentItem && currentItem->clipId() == id) {
        showClipProperties(currentItem, true);
    }
}

void Bin::showClipProperties(ProjectClip *clip, bool forceRefresh)
{
    if (!clip || !clip->isReady()) {
        m_propertiesPanel->setEnabled(false);
        return;
    }
    m_propertiesPanel->setEnabled(true);
    QString panelId = m_propertiesPanel->property("clipId").toString();
    if (!forceRefresh && panelId == clip->clipId()) {
        // the properties panel is already displaying current clip, do nothing
        return;
    }
    // Cleanup widget for new content
    foreach (QWidget *w, m_propertiesPanel->findChildren<ClipPropertiesController *>()) {
        delete w;
    }
    m_propertiesPanel->setProperty("clipId", clip->clipId());
    QVBoxLayout *lay = static_cast<QVBoxLayout *>(m_propertiesPanel->layout());
    if (lay == nullptr) {
        lay = new QVBoxLayout(m_propertiesPanel);
        m_propertiesPanel->setLayout(lay);
    }
    ClipPropertiesController *panel = clip->buildProperties(m_propertiesPanel);
    connect(this, &Bin::refreshTimeCode, panel, &ClipPropertiesController::slotRefreshTimeCode);
    connect(this, &Bin::refreshPanelMarkers, panel, &ClipPropertiesController::slotFillMarkers);
    connect(panel, SIGNAL(updateClipProperties(QString, QMap<QString, QString>, QMap<QString, QString>)), this, SLOT(slotEditClipCommand(QString, QMap<QString, QString>, QMap<QString, QString>)));
    connect(panel, SIGNAL(seekToFrame(int)), m_monitor, SLOT(slotSeek(int)));
    connect(panel, SIGNAL(addMarkers(QString,QList<CommentedTime>)), this, SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));
    connect(panel, &ClipPropertiesController::editClip, this, &Bin::slotEditClip);
    connect(panel, SIGNAL(editAnalysis(QString, QString, QString)), this, SLOT(slotAddClipExtraData(QString, QString, QString)));

    connect(panel, &ClipPropertiesController::loadMarkers, this, &Bin::slotLoadClipMarkers);
    connect(panel, &ClipPropertiesController::saveMarkers, this, &Bin::slotSaveClipMarkers);
    lay->addWidget(panel);
}

void Bin::slotEditClipCommand(const QString &id, const QMap<QString, QString> &oldProps, const QMap<QString, QString> &newProps)
{
    EditClipCommand *command = new EditClipCommand(this, id, oldProps, newProps, true);
    m_doc->commandStack()->push(command);
}

void Bin::reloadClip(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        return;
    }
    QDomDocument doc;
    QDomElement xml = clip->toXml(doc);
    if (!xml.isNull()) {
        m_doc->getFileProperties(xml, id, 150, true);
    }
}

void Bin::slotThumbnailReady(const QString &id, const QImage &img, bool fromFile)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setThumbnail(img);
        // Save thumbnail for later reuse
        bool ok = false;
        if (!fromFile) {
            img.save(m_doc->getCacheDir(CacheThumbs, &ok).absoluteFilePath(clip->hash() + QStringLiteral(".png")));
        }
    }
}

QStringList Bin::getBinFolderClipIds(const QString &id) const
{
    QStringList ids;
    ProjectFolder *folder = m_rootFolder->folder(id);
    if (folder) {
        for (int i = 0; i < folder->count(); i++) {
            AbstractProjectItem *child = folder->at(i);
            if (child->itemType() == AbstractProjectItem::ClipItem) {
                ids << child->clipId();
            }
        }
    }
    return ids;
}

ProjectClip *Bin::getBinClip(const QString &id)
{
    ProjectClip *clip = nullptr;
    if (id.contains(QLatin1Char('_'))) {
        clip = m_rootFolder->clip(id.section(QLatin1Char('_'), 0, 0));
    } else if (!id.isEmpty()) {
        clip = m_rootFolder->clip(id);
    }
    return clip;
}

void Bin::setWaitingStatus(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setClipStatus(AbstractProjectItem::StatusWaiting);
    }
}

void Bin::slotRemoveInvalidClip(const QString &id, bool replace, const QString &errorMessage)
{
    Q_UNUSED(replace)

    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        return;
    }
    emit requesteInvalidRemoval(id, clip->url(), errorMessage);
}

void Bin::slotProducerReady(const requestClipInfo &info, ClipController *controller)
{
    ProjectClip *clip = m_rootFolder->clip(info.clipId);
    if (clip) {
        if (clip->setProducer(controller, info.replaceProducer) && !clip->hasProxy()) {
            emit producerReady(info.clipId);
            // Check for file modifications
            ClipType t = clip->clipType();
            if (t == AV || t == Audio || t == Image || t == Video || t == Playlist) {
                m_doc->watchFile(clip->url());
            }
            if (m_doc->useProxy()) {
                if (t == AV || t == Video) {
                    int width = clip->getProducerIntProperty(QStringLiteral("meta.media.width"));
                    if (m_doc->autoGenerateProxy(width)) {
                        // Start proxy
                        m_doc->slotProxyCurrentItem(true, QList<ProjectClip *>() << clip);
                    }
                } else if (t == Playlist) {
                    // always proxy playlists
                    m_doc->slotProxyCurrentItem(true, QList<ProjectClip *>() << clip);
                } else if (t == Image && m_doc->autoGenerateImageProxy(clip->getProducerIntProperty(QStringLiteral("meta.media.width")))) {
                    // Start proxy
                    m_doc->slotProxyCurrentItem(true, QList<ProjectClip *>() << clip);
                }
            }
        } else {
            emit producerReady(info.clipId);
        }
        QString currentClip = m_monitor->activeClipId();
        if (currentClip.isEmpty()) {
            //No clip displayed in monitor, check if item is selected
            QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
            foreach (const QModelIndex &ix, indexes) {
                if (!ix.isValid() || ix.column() != 0) {
                    continue;
                }
                AbstractProjectItem *item = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
                if (item && item->clipId() == info.clipId) {
                    // Item was selected, show it in monitor
                    item->setCurrent(true);
                    break;
                }
            }
        } else if (currentClip == info.clipId) {
            emit openClip(nullptr);
            clip->setCurrent(true);
        }
    } else {
        // Clip not found, create it
        QString groupId = controller->property(QStringLiteral("kdenlive:folderid"));
        ProjectFolder *parentFolder;
        if (!groupId.isEmpty()) {
            parentFolder = m_rootFolder->folder(groupId);
            if (!parentFolder) {
                // parent folder does not exist, put in root folder
                parentFolder = m_rootFolder;
            }
            if (groupId.toInt() >= m_folderCounter) {
                m_folderCounter = groupId.toInt() + 1;
            }
        } else {
            parentFolder = m_rootFolder;
        }
        //FIXME(style): constructor actually adds the new pointer to parent's children
        ProjectClip *clip = new ProjectClip(info.clipId, m_blankThumb, controller, parentFolder);
        emit producerReady(info.clipId);
        ClipType t = clip->clipType();
        if (t == AV || t == Audio || t == Image || t == Video || t == Playlist) {
            m_doc->watchFile(clip->url());
        }
        if (info.clipId.toInt() >= m_clipCounter) {
            m_clipCounter = info.clipId.toInt() + 1;
        }
    }
}

void Bin::slotOpenCurrent()
{
    ProjectClip *currentItem = getFirstSelectedClip();
    if (currentItem) {
        emit openClip(currentItem->controller());
    }
}

void Bin::openProducer(ClipController *controller)
{
    emit openClip(controller);
}

void Bin::openProducer(ClipController *controller, int in, int out)
{
    emit openClip(controller, in, out);
}

void Bin::emitItemUpdated(AbstractProjectItem *item)
{
    emit itemUpdated(item);
}

void Bin::emitRefreshPanel(const QString &id)
{
    emit refreshPanel(id);
}

void Bin::setupGeneratorMenu()
{
    if (!m_menu) {
        qCDebug(KDENLIVE_LOG) << "Warning, menu was not created, something is wrong";
        return;
    }

    QMenu *addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("generators"), pCore->window()));
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
    }

    addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("transcoders"), pCore->window()));
    if (addMenu) {
        m_menu->addMenu(addMenu);
        addMenu->setEnabled(!addMenu->isEmpty());
        m_transcodeAction = addMenu;
    }

    addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("clip_actions"), pCore->window()));
    if (addMenu) {
        m_menu->addMenu(addMenu);
        addMenu->setEnabled(!addMenu->isEmpty());
        m_clipsActionsMenu = addMenu;
    }

    addMenu = qobject_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("clip_in_timeline"), pCore->window()));
    if (addMenu) {
        m_inTimelineAction = m_menu->addMenu(addMenu);
        m_inTimelineAction->setEnabled(!addMenu->isEmpty());
    }

    if (m_locateAction) {
        m_menu->addAction(m_locateAction);
    }
    if (m_reloadAction) {
        m_menu->addAction(m_reloadAction);
    }
    if (m_duplicateAction) {
        m_menu->addAction(m_duplicateAction);
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

void Bin::setupMenu(QMenu *addMenu, QAction *defaultAction, const QHash<QString, QAction *> &actions)
{
    // Setup actions
    QAction *first = m_toolbar->actions().at(0);
    m_deleteAction = actions.value(QStringLiteral("delete"));
    m_toolbar->insertAction(first, m_deleteAction);

    QAction *folder = actions.value(QStringLiteral("folder"));
    m_toolbar->insertAction(m_deleteAction, folder);

    m_editAction = actions.value(QStringLiteral("properties"));
    m_openAction = actions.value(QStringLiteral("open"));
    m_reloadAction = actions.value(QStringLiteral("reload"));
    m_duplicateAction = actions.value(QStringLiteral("duplicate"));
    m_locateAction = actions.value(QStringLiteral("locate"));
    m_proxyAction = actions.value(QStringLiteral("proxy"));

    QMenu *m = new QMenu(this);
    m->addActions(addMenu->actions());
    m_addButton = new QToolButton(this);
    m_addButton->setMenu(m);
    m_addButton->setDefaultAction(defaultAction);
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->insertWidget(folder, m_addButton);
    m_menu = new QMenu(this);
    m_propertiesDock = pCore->window()->addDock(i18n("Clip Properties"), QStringLiteral("clip_properties"), m_propertiesPanel);
    m_propertiesDock->close();
    //m_menu->addActions(addMenu->actions());
}

const QString Bin::getDocumentProperty(const QString &key)
{
    return m_doc->getDocumentProperty(key);
}

const QSize Bin::getRenderSize()
{
    return m_doc->getRenderSize();
}

void Bin::slotUpdateJobStatus(const QString &id, int jobType, int status, const QString &label, const QString &actionName, const QString &details)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setJobStatus((AbstractClipJob::JOBTYPE) jobType, (ClipJobStatus) status);
    }
    if (status == JobCrashed) {
        QList<QAction *> actions = m_infoMessage->actions();
        if (m_infoMessage->isHidden()) {
            if (!details.isEmpty()) {
                m_infoMessage->setText(label + QStringLiteral(" <a href=\"#\">") + i18n("Show log") + QStringLiteral("</a>"));
            } else {
                m_infoMessage->setText(label);
            }
            m_infoMessage->setWordWrap(m_infoMessage->text().length() > 35);
            m_infoMessage->setMessageType(KMessageWidget::Warning);
        }

        if (!actionName.isEmpty()) {
            QAction *action = nullptr;
            QList< KActionCollection * > collections = KActionCollection::allCollections();
            for (int i = 0; i < collections.count(); ++i) {
                KActionCollection *coll = collections.at(i);
                action = coll->action(actionName);
                if (action) {
                    break;
                }
            }
            if (action && !actions.contains(action)) {
                m_infoMessage->addAction(action);
            }
        }
        if (!details.isEmpty()) {
            m_errorLog.append(details);
        }
        m_infoMessage->setCloseButtonVisible(true);
        m_infoMessage->animatedShow();
    }
}

void Bin::doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QList<QAction *> &actions)
{
    // Remove axisting actions if any
    QList<QAction *> acts = m_infoMessage->actions();
    while (!acts.isEmpty()) {
        QAction *a = acts.takeFirst();
        m_infoMessage->removeAction(a);
        delete a;
    }
    m_infoMessage->setText(text);
    m_infoMessage->setWordWrap(m_infoMessage->text().length() > 35);
    foreach (QAction *action, actions) {
        m_infoMessage->addAction(action);
        connect(action, &QAction::triggered, this, &Bin::slotMessageActionTriggered);
    }
    m_infoMessage->setCloseButtonVisible(actions.isEmpty());
    m_infoMessage->setMessageType(type);
    if (m_infoMessage->isHidden()) {
        m_infoMessage->animatedShow();
    }
}

void Bin::slotShowJobLog()
{
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout;
    QTextEdit t(&d);
    for (int i = 0; i < m_errorLog.count(); ++i) {
        if (i > 0) {
            t.insertHtml(QStringLiteral("<br><hr /><br>"));
        }
        t.insertPlainText(m_errorLog.at(i));
    }
    t.setReadOnly(true);
    l->addWidget(&t);
    mainWidget->setLayout(l);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    d.setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    mainLayout->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::accept);
    d.exec();
}

void Bin::gotProxy(const QString &id, const QString &path)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        QDomDocument doc;
        clip->setProducerProperty(QStringLiteral("kdenlive:proxy"), path);
        QDomElement xml = clip->toXml(doc, true);
        if (!xml.isNull()) {
            m_doc->getFileProperties(xml, id, 150, true);
        }
    }
}

void Bin::reloadProducer(const QString &id, const QDomElement &xml)
{
    m_doc->getFileProperties(xml, id, 150, true);
}

void Bin::refreshClip(const QString &id)
{
    emit clipNeedsReload(id, false);
    if (m_monitor->activeClipId() == id) {
        m_monitor->refreshMonitorIfActive();
    }
}

void Bin::emitRefreshAudioThumbs(const QString &id)
{
    emit refreshAudioThumbs(id);
}

void Bin::doRefreshAudioThumbs(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
        slotSendAudioThumb(id);
    }
}

void Bin::refreshClipMarkers(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
        m_monitor->updateMarkers();
    }
    if (m_propertiesPanel) {
        QString panelId = m_propertiesPanel->property("clipId").toString();
        if (panelId == id) {
            emit refreshPanelMarkers();
        }
    }
}

void Bin::discardJobs(const QString &id, AbstractClipJob::JOBTYPE type)
{
    m_jobManager->discardJobs(id, type);
}

void Bin::slotStartCutJob(const QString &id)
{
    startJob(id, AbstractClipJob::CUTJOB);
}

void Bin::startJob(const QString &id, AbstractClipJob::JOBTYPE type)
{
    ProjectClip *clip = getBinClip(id);
    if (clip && !hasPendingJob(id, type)) {
        // Launch job
        const QList<ProjectClip *> clips = {clip};
        m_jobManager->prepareJobs(clips, m_doc->fps(), type);
    }
}

bool Bin::hasPendingJob(const QString &id, AbstractClipJob::JOBTYPE type)
{
    return m_jobManager->hasPendingJob(id, type);
}

void Bin::slotCreateProjectClip()
{
    QAction *act = qobject_cast<QAction *>(sender());
    if (act == nullptr) {
        // Cannot access triggering action, something is wrong
        qCDebug(KDENLIVE_LOG) << "// Error in clip creation action";
        return;
    }
    ClipType type = (ClipType) act->data().toInt();
    QStringList folderInfo = getFolderInfo();
    switch (type) {
    case Color:
        ClipCreationDialog::createColorClip(m_doc, folderInfo, this);
        break;
    case SlideShow:
        ClipCreationDialog::createSlideshowClip(m_doc, folderInfo, this);
        break;
    case Text:
        ClipCreationDialog::createTitleClip(m_doc, folderInfo, QString(), this);
        break;
    case TextTemplate:
        ClipCreationDialog::createTitleTemplateClip(m_doc, folderInfo, this);
        break;
    case QText:
        ClipCreationDialog::createQTextClip(m_doc, folderInfo, this);
        break;
    default:
        break;
    }
}

void Bin::slotItemDropped(const QStringList &ids, const QModelIndex &parent)
{
    AbstractProjectItem *parentItem;
    if (parent.isValid()) {
        parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        while (parentItem->itemType() != AbstractProjectItem::FolderItem) {
            parentItem = parentItem->parent();
        }
    } else {
        parentItem = m_rootFolder;
    }
    QUndoCommand *moveCommand = new QUndoCommand();
    moveCommand->setText(i18np("Move Clip", "Move Clips", ids.count()));
    QStringList folderIds;
    foreach (const QString &id, ids) {
        if (id.contains(QLatin1Char('/'))) {
            // trying to move clip zone, not allowed. Ignore
            continue;
        }
        if (id.startsWith(QLatin1Char('#'))) {
            // moving a folder, keep it for later
            folderIds << id;
            continue;
        }
        ProjectClip *currentItem = m_rootFolder->clip(id);
        AbstractProjectItem *currentParent = currentItem->parent();
        if (currentParent != parentItem) {
            // Item was dropped on a different folder
            new MoveBinClipCommand(this, id, currentParent->clipId(), parentItem->clipId(), moveCommand);
        }
    }
    if (!folderIds.isEmpty()) {
        foreach (QString id, folderIds) {
            id.remove(0, 1);
            ProjectFolder *currentItem = m_rootFolder->folder(id);
            AbstractProjectItem *currentParent = currentItem->parent();
            if (currentParent != parentItem) {
                // Item was dropped on a different folder
                new MoveBinFolderCommand(this, id, currentParent->clipId(), parentItem->clipId(), moveCommand);
            }
        }
    }
    m_doc->commandStack()->push(moveCommand);
}

void Bin::slotEffectDropped(QString id, QDomElement effect)
{
    if (id.isEmpty()) {
        id = m_monitor->activeClipId();
    }
    if (id.isEmpty()) {
        return;
    }
    AddBinEffectCommand *command = new AddBinEffectCommand(this, id, effect);
    m_doc->commandStack()->push(command);
}

void Bin::slotUpdateEffect(QString id, QDomElement oldEffect, QDomElement newEffect, int ix, bool refreshStack)
{
    if (id.isEmpty()) {
        id = m_monitor->activeClipId();
    }
    if (id.isEmpty()) {
        return;
    }
    UpdateBinEffectCommand *command = new UpdateBinEffectCommand(this, id, oldEffect, newEffect, ix, refreshStack);
    m_doc->commandStack()->push(command);
}

void Bin::slotChangeEffectState(QString id, const QList<int> &indexes, bool disable)
{
    if (id.isEmpty()) {
        id = m_monitor->activeClipId();
    }
    if (id.isEmpty()) {
        return;
    }
    ChangeMasterEffectStateCommand *command = new ChangeMasterEffectStateCommand(this, id, indexes, disable);
    m_doc->commandStack()->push(command);
}

void Bin::slotEffectDropped(const QString &effect, const QModelIndex &parent)
{
    if (parent.isValid()) {
        AbstractProjectItem *parentItem;
        parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        if (parentItem->itemType() != AbstractProjectItem::ClipItem) {
            // effect only supported on clip items
            return;
        }
        m_proxyModel->selectionModel()->clearSelection();
        int row = parent.row();
        const QModelIndex id = m_itemModel->index(row, 0, parent.parent());
        const QModelIndex id2 = m_itemModel->index(row, m_rootFolder->supportedDataCount() - 1, parent.parent());
        if (id.isValid() && id2.isValid()) {
            m_proxyModel->selectionModel()->select(QItemSelection(m_proxyModel->mapFromSource(id), m_proxyModel->mapFromSource(id2)), QItemSelectionModel::Select);
        }
        parentItem->setCurrent(true);
        QDomDocument doc;
        doc.setContent(effect);
        QDomElement e = doc.documentElement();
        AddBinEffectCommand *command = new AddBinEffectCommand(this, parentItem->clipId(), e);
        m_doc->commandStack()->push(command);
    }
}

void Bin::slotDeleteEffect(const QString &id, QDomElement effect)
{
    RemoveBinEffectCommand *command = new RemoveBinEffectCommand(this, id, effect);
    m_doc->commandStack()->push(command);
}

void Bin::slotMoveEffect(const QString &id, const QList<int> &currentPos, int newPos)
{
    MoveBinEffectCommand *command = new MoveBinEffectCommand(this, id, currentPos, newPos);
    m_doc->commandStack()->push(command);
}

void Bin::moveEffect(const QString &id, const QList<int> &oldPos, const QList<int> &newPos)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        return;
    }
    int new_position = newPos.at(0);
    ClipController *ctrl = clip->controller();
    int effectsCount = ctrl->effectsCount();
    if (new_position > effectsCount) {
        new_position = effectsCount;
    }
    int old_position = oldPos.at(0);
    for (int i = 0; i < newPos.count(); ++i) {
        if (old_position > new_position) {
            // Moving up, we need to adjust index
            old_position = oldPos.at(i);
            new_position = newPos.at(i);
        }
        ctrl->moveEffect(old_position, new_position);
    }
    emit masterClipUpdated(ctrl, m_monitor);
    m_monitor->refreshMonitorIfActive();
    ctrl->reloadTrackProducers();
}

void Bin::removeEffect(const QString &id, const QDomElement &effect)
{
    if (effect.isNull()) {
        qCWarning(KDENLIVE_LOG) << " / /ERROR, trying to remove empty effect";
        return;
    }
    ProjectClip *currentItem = m_rootFolder->clip(id);
    if (!currentItem) {
        return;
    }
    currentItem->removeEffect(effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
    m_monitor->refreshMonitorIfActive();
}

void Bin::addEffect(const QString &id, QDomElement &effect)
{
    ProjectClip *currentItem = m_rootFolder->clip(id);
    if (!currentItem) {
        return;
    }
    currentItem->addEffect(m_monitor->profileInfo(), effect);
    m_monitor->refreshMonitorIfActive();
}

void Bin::updateEffect(const QString &id, QDomElement &effect, int ix, bool refreshStackWidget)
{
    ProjectClip *currentItem = m_rootFolder->clip(id);
    if (!currentItem) {
        return;
    }
    currentItem->updateEffect(m_monitor->profileInfo(), effect, ix, refreshStackWidget);
    m_monitor->refreshMonitorIfActive();
}

void Bin::changeEffectState(const QString &id, const QList<int> &indexes, bool disable, bool refreshStack)
{
    ProjectClip *currentItem = m_rootFolder->clip(id);
    if (!currentItem) {
        return;
    }
    ClipController *ctl = currentItem->controller();
    if (!ctl) {
        return;
    }
    ctl->changeEffectState(indexes, disable);
    if (refreshStack) {
        updateMasterEffect(ctl);
    }
    m_monitor->refreshMonitorIfActive();
}

void Bin::editMasterEffect(ClipController *ctl)
{
    if (m_gainedFocus) {
        // Widget just gained focus, updating stack is managed in the eventfilter event, not from item
        return;
    }
    emit masterClipSelected(ctl, m_monitor);
}

void Bin::updateMasterEffect(ClipController *ctl)
{
    if (m_gainedFocus) {
        // Widget just gained focus, updating stack is managed in the eventfilter event, not from item
        return;
    }
    emit masterClipUpdated(ctl, m_monitor);
}

void Bin::slotGotFocus()
{
    m_gainedFocus = true;
}

void Bin::doMoveClip(const QString &id, const QString &newParentId)
{
    ProjectClip *currentItem = m_rootFolder->clip(id);
    if (!currentItem) {
        return;
    }
    AbstractProjectItem *currentParent = currentItem->parent();
    ProjectFolder *newParent = m_rootFolder->folder(newParentId);
    currentParent->removeChild(currentItem);
    currentItem->setParent(newParent);
    currentItem->updateParentInfo(newParentId, newParent->name());
}

void Bin::doMoveFolder(const QString &id, const QString &newParentId)
{
    ProjectFolder *currentItem = m_rootFolder->folder(id);
    AbstractProjectItem *currentParent = currentItem->parent();
    ProjectFolder *newParent = m_rootFolder->folder(newParentId);
    currentParent->removeChild(currentItem);
    currentItem->setParent(newParent);
    emit storeFolder(id, newParent->clipId(), currentParent->clipId(), currentItem->name());
}

void Bin::droppedUrls(const QList<QUrl> &urls, const QStringList &folderInfo)
{
    QModelIndex current;
    if (folderInfo.isEmpty()) {
        current = m_proxyModel->mapToSource(m_proxyModel->selectionModel()->currentIndex());
    } else {
        // get index for folder
        current = getIndexForId(folderInfo.first(), true);
    }
    slotItemDropped(urls, current);
}

void Bin::slotAddClipToProject(const QUrl &url)
{
    QList<QUrl> urls;
    urls << url;
    QModelIndex current = m_proxyModel->mapToSource(m_proxyModel->selectionModel()->currentIndex());
    slotItemDropped(urls, current);
}

void Bin::slotItemDropped(const QList<QUrl> &urls, const QModelIndex &parent)
{
    QStringList folderInfo;
    if (parent.isValid()) {
        // Check if drop occured on a folder
        AbstractProjectItem *parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        while (parentItem->itemType() != AbstractProjectItem::FolderItem) {
            parentItem = parentItem->parent();
        }
        if (parentItem != m_rootFolder) {
            folderInfo << parentItem->clipId();
        }
    }
    //TODO: verify if urls exist
    QList<QUrl> clipsToAdd = urls;
    QMimeDatabase db;
    foreach (const QUrl &file, clipsToAdd) {
        // Check there is no folder here
        QMimeType type = db.mimeTypeForUrl(file);
        if (type.inherits(QStringLiteral("inode/directory"))) {
            // user dropped a folder, import its files
            clipsToAdd.removeAll(file);
            QDir dir(file.toLocalFile());
            const QStringList result = dir.entryList(QDir::Files);
            QList<QUrl> folderFiles;
            folderFiles.reserve(result.count());
            for (const QString &path : result) {
                folderFiles.append(QUrl::fromLocalFile(dir.absoluteFilePath(path)));
            }
            if (!folderFiles.isEmpty()) {
                QString folderId = slotAddFolder(dir.dirName());
                QModelIndex ind = getIndexForId(folderId, true);
                QStringList newFolderInfo;
                if (ind.isValid()) {
                    newFolderInfo = getFolderInfo(m_proxyModel->mapFromSource(ind));
                }
                ClipCreationDialog::createClipsCommand(m_doc, folderFiles, newFolderInfo, this);
            }
        }
    }
    if (!clipsToAdd.isEmpty()) {
        ClipCreationDialog::createClipsCommand(m_doc, clipsToAdd, folderInfo, this);
    }
}

void Bin::slotExpandUrl(const ItemInfo &info, const QString &url, QUndoCommand *command)
{
    // Create folder to hold imported clips
    QString folderName = QFileInfo(url).fileName().section(QLatin1Char('.'), 0, 0);
    QString folderId = QString::number(getFreeFolderId());
    new AddBinFolderCommand(this, folderId, folderName.isEmpty() ? i18n("Folder") : folderName, m_rootFolder->clipId(), false, command);

    // Parse playlist clips
    QDomDocument doc;
    QFile file(url);
    doc.setContent(&file, false);
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
        doDisplayMessage(i18n("Playlist clip %1 has too many tracks (%2) to be imported. Add new tracks to your project.", QFileInfo(url).fileName(), tracks.count()), KMessageWidget::Warning);
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
        QString mltService = EffectsList::property(prod, QStringLiteral("mlt_service"));
        if (mltService == QLatin1String("pixbuf")
                || mltService == QLatin1String("qimage")
                || mltService == QLatin1String("kdenlivetitle")
                || mltService == QLatin1String("color")
                || mltService == QLatin1String("colour")) {
            hash = mltService + QLatin1Char(':')
                   + EffectsList::property(prod, QStringLiteral("kdenlive:clipname")) + QLatin1Char(':')
                   + EffectsList::property(prod, QStringLiteral("kdenlive:folderid")) + QLatin1Char(':');
            if (mltService == QLatin1String("kdenlivetitle")) {
                // Calculate hash based on title contents.
                hash.append(QString(QCryptographicHash::hash(EffectsList::property(prod, QStringLiteral("xmldata")).toUtf8(),
                                    QCryptographicHash::Md5
                                                            ).toHex()));
            } else if (mltService == QLatin1String("pixbuf")
                       || mltService == QLatin1String("qimage")
                       || mltService == QLatin1String("color")
                       || mltService == QLatin1String("colour")) {
                hash.append(EffectsList::property(prod, QStringLiteral("resource")));
            }

            QString singletonId = hashToIdMap.value(hash, QString());
            if (singletonId.length()) {
                // map duplicate producer ID to single bin clip producer ID.
                qCDebug(KDENLIVE_LOG) << "found duplicate producer:" << hash << ", reusing newID:" << singletonId;
                idMap.insert(originalId, singletonId);
                continue;
            }
        }

        // First occurence of a producer, so allocate new bin clip producer ID.
        QString newId = QString::number(getFreeClipId());
        idMap.insert(originalId, newId);
        qCDebug(KDENLIVE_LOG) << "originalId: " << originalId << ", newId: " << newId;

        // Ensure to register new bin clip producer ID in hash hashmap for
        // those clips that MLT likes to serialize multiple times. This is
        // indicated by having a hash "value" unqual "". See also above.
        if (hash.length()) {
            hashToIdMap.insert(hash, newId);
        }

        // Add clip
        QDomElement clone = prod.cloneNode(true).toElement();
        EffectsList::setProperty(clone, QStringLiteral("kdenlive:folderid"), folderId);
        // Do we have a producer that uses a resource property that contains a path?
        if (mltService == QLatin1String("avformat-novalidate") // av clip
                || mltService == QLatin1String("avformat")     // av clip
                || mltService == QLatin1String("pixbuf")       // image (sequence) clip
                || mltService == QLatin1String("qimage")       // image (sequence) clip
                || mltService == QLatin1String("xml")          // MLT playlist clip, someone likes recursion :)
           ) {
            // Make sure to correctly resolve relative resource paths based on
            // the playlist's root, not on this project's root
            QString resource = EffectsList::property(clone, QStringLiteral("resource"));
            if (QFileInfo(resource).isRelative()) {
                QFileInfo rootedResource(mltRoot, resource);
                qCDebug(KDENLIVE_LOG) << "fixed resource path for producer, newId:" << newId << "resource:" << rootedResource.absoluteFilePath();
                EffectsList::setProperty(clone, QStringLiteral("resource"), rootedResource.absoluteFilePath());
            }
        }

        ClipCreationDialog::createClipsCommand(this, clone, newId, command);
    }
    pCore->projectManager()->currentTimeline()->importPlaylist(info, idMap, doc, command);
}

void Bin::slotItemEdited(const QModelIndex &ix, const QModelIndex &, const QVector<int> &)
{
    if (ix.isValid()) {
        // Clip renamed
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(ix.internalPointer());
        ProjectClip *clip = qobject_cast<ProjectClip *>(item);
        if (clip) {
            emit clipNameChanged(clip->clipId());
        }
    }
}

void Bin::renameFolderCommand(const QString &id, const QString &newName, const QString &oldName)
{
    RenameBinFolderCommand *command = new RenameBinFolderCommand(this, id, newName, oldName);
    m_doc->commandStack()->push(command);
}

void Bin::renameSubClipCommand(const QString &id, const QString &newName, const QString &oldName, int in, int out)
{
    RenameBinSubClipCommand *command = new RenameBinSubClipCommand(this, id, newName, oldName, in, out);
    m_doc->commandStack()->push(command);
}

void Bin::renameSubClip(const QString &id, const QString &newName, const QString &oldName, int in, int out)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        return;
    }
    ProjectSubClip *sub = clip->getSubClip(in, out);
    if (!sub) {
        return;
    }
    sub->setName(newName);
    clip->setProducerProperty("kdenlive:clipzone." + oldName, QString());
    clip->setProducerProperty("kdenlive:clipzone." + newName, QString::number(in) + QLatin1Char(';') +  QString::number(out));
    emit itemUpdated(sub);
}

void Bin::slotStartClipJob(bool enable)
{
    Q_UNUSED(enable)

    QAction *act = qobject_cast<QAction *>(sender());
    if (act == nullptr) {
        // Cannot access triggering action, something is wrong
        qCDebug(KDENLIVE_LOG) << "// Error in clip job action";
        return;
    }
    startClipJob(act->data().toStringList());
}

void Bin::startClipJob(const QStringList &params)
{
    QStringList data = params;
    if (data.isEmpty()) {
        qCDebug(KDENLIVE_LOG) << "// Error in clip job action";
        return;
    }
    AbstractClipJob::JOBTYPE jobType = (AbstractClipJob::JOBTYPE) data.takeFirst().toInt();
    QList<ProjectClip *>clips = selectedClips();
    m_jobManager->prepareJobs(clips, m_doc->fps(), jobType, data);
}

void Bin::slotCancelRunningJob(const QString &id, const QMap<QString, QString> &newProps)
{
    if (newProps.isEmpty()) {
        return;
    }
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    QMap<QString, QString> oldProps;
    QMapIterator<QString, QString> i(newProps);
    while (i.hasNext()) {
        i.next();
        QString value = newProps.value(i.key());
        oldProps.insert(i.key(), value);
    }
    if (newProps == oldProps) {
        return;
    }
    EditClipCommand *command = new EditClipCommand(this, id, oldProps, newProps, true);
    m_doc->commandStack()->push(command);
}

void Bin::slotPrepareJobsMenu()
{
    ProjectClip *item = getFirstSelectedClip();
    if (item) {
        QString id = item->clipId();
        m_discardCurrentClipJobs->setData(id);
        QStringList jobs = m_jobManager->getPendingJobs(id);
        m_discardCurrentClipJobs->setEnabled(!jobs.isEmpty());
    } else {
        m_discardCurrentClipJobs->setData(QString());
        m_discardCurrentClipJobs->setEnabled(false);
    }
}

void Bin::slotAddClipCut(const QString &id, int in, int out)
{
    AddBinClipCutCommand *command = new AddBinClipCutCommand(this, id, in, out, true);
    m_doc->commandStack()->push(command);
}

void Bin::loadSubClips(const QString &id, const QMap<QString, QString> &data)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    QMapIterator<QString, QString> i(data);
    QList<int> missingThumbs;
    int maxFrame = clip->duration().frames(m_doc->fps()) - 1;
    while (i.hasNext()) {
        i.next();
        if (!i.value().contains(QLatin1Char(';'))) {
            // Problem, the zone has no in/out points
            continue;
        }
        int in = i.value().section(QLatin1Char(';'), 0, 0).toInt();
        int out = i.value().section(QLatin1Char(';'), 1, 1).toInt();
        if (maxFrame > 0) {
            out = qMin(out, maxFrame);
        }
        missingThumbs << in;
        new ProjectSubClip(clip, in, out, m_doc->timecode().getDisplayTimecodeFromFrames(in, KdenliveSettings::frametimecode()), i.key());
    }
    if (!missingThumbs.isEmpty()) {
        // generate missing subclip thumbnails
        clip->slotExtractImage(missingThumbs);
    }
}

void Bin::addClipCut(const QString &id, int in, int out)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    // Check that we don't already have that subclip
    ProjectSubClip *sub = clip->getSubClip(in, out);
    if (sub) {
        // A subclip with same zone already exists
        return;
    }
    sub = new ProjectSubClip(clip, in, out, m_doc->timecode().getDisplayTimecodeFromFrames(in, KdenliveSettings::frametimecode()));
    QStringList markersComment = clip->markersText(GenTime(in, m_doc->fps()), GenTime(out, m_doc->fps()));
    sub->setDescription(markersComment.join(QLatin1Char(';')));
    QList<int> missingThumbs;
    missingThumbs << in;
    clip->slotExtractImage(missingThumbs);
}

void Bin::removeClipCut(const QString &id, int in, int out)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    ProjectSubClip *sub = clip->getSubClip(in, out);
    if (sub) {
        clip->removeChild(sub);
        sub->discard();
        delete sub;
    }
}

Timecode Bin::projectTimecode() const
{
    return m_doc->timecode();
}

void Bin::slotStartFilterJob(const ItemInfo &info, const QString &id, QMap<QString, QString> &filterParams, QMap<QString, QString> &consumerParams, QMap<QString, QString> &extraParams)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }

    QMap<QString, QString> producerParams = QMap<QString, QString> ();
    producerParams.insert(QStringLiteral("producer"), clip->url());
    if (info.cropDuration != GenTime()) {
        producerParams.insert(QStringLiteral("in"), QString::number((int) info.cropStart.frames(m_doc->fps())));
        producerParams.insert(QStringLiteral("out"), QString::number((int)(info.cropStart + info.cropDuration).frames(m_doc->fps())));
        extraParams.insert(QStringLiteral("clipStartPos"), QString::number((int) info.startPos.frames(m_doc->fps())));
        extraParams.insert(QStringLiteral("clipTrack"), QString::number(info.track));
    } else {
        // We want to process whole clip
        producerParams.insert(QStringLiteral("in"), QString::number(0));
        producerParams.insert(QStringLiteral("out"), QString::number(-1));
    }
    m_jobManager->prepareJobFromTimeline(clip, producerParams, filterParams, consumerParams, extraParams);
}

void Bin::focusBinView() const
{
    m_itemView->setFocus();
}

void Bin::slotOpenClip()
{
    ProjectClip *clip = getFirstSelectedClip();
    if (!clip) {
        return;
    }
    switch (clip->clipType()) {
    case Text:
    case TextTemplate:
        showTitleWidget(clip);
        break;
    case Image:
        if (KdenliveSettings::defaultimageapp().isEmpty()) {
            KMessageBox::sorry(QApplication::activeWindow(), i18n("Please set a default application to open images in the Settings dialog"));
        } else {
            QProcess::startDetached(KdenliveSettings::defaultimageapp(), QStringList() << clip->url());
        }
        break;
    case Audio:
        if (KdenliveSettings::defaultaudioapp().isEmpty()) {
            KMessageBox::sorry(QApplication::activeWindow(), i18n("Please set a default application to open audio files in the Settings dialog"));
        } else {
            QProcess::startDetached(KdenliveSettings::defaultaudioapp(), QStringList() << clip->url());
        }
        break;
    default:
        break;
    }
}

void Bin::updateTimecodeFormat()
{
    emit refreshTimeCode();
}

void Bin::slotGotFilterJobResults(const QString &id, int startPos, int track, const stringMap &results, const stringMap &filterInfo)
{
    if (filterInfo.contains(QStringLiteral("finalfilter"))) {
        if (filterInfo.contains(QStringLiteral("storedata"))) {
            // Store returned data as clip extra data
            ProjectClip *clip = getBinClip(id);
            if (clip) {
                QString key = filterInfo.value(QStringLiteral("key"));
                QStringList newValue = clip->updatedAnalysisData(key, results.value(key), filterInfo.value(QStringLiteral("offset")).toInt());
                slotAddClipExtraData(id, newValue.at(0), newValue.at(1));
            }
        }
        if (startPos == -1) {
            // Processing bin clip
            ProjectClip *currentItem = m_rootFolder->clip(id);
            if (!currentItem) {
                return;
            }
            ClipController *ctl = currentItem->controller();
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
                ctl->updateEffect(pCore->monitorManager()->projectMonitor()->profileInfo(), newEffect, effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
                emit masterClipUpdated(ctl, m_monitor);
                // TODO use undo / redo for bin clip edit effect
                /*EditEffectCommand *command = new EditEffectCommand(this, clip->track(), clip->startPos(), effect, newEffect, clip->selectedEffectIndex(), true, true);
                m_commandStack->push(command);
                emit clipItemSelected(clip);*/
            }

            //emit gotFilterJobResults(id, startPos, track, results, filterInfo);*/
            return;
        } else {
            // This is a timeline filter, forward results
            emit gotFilterJobResults(id, startPos, track, results, filterInfo);
            return;
        }
    }
    // Currently, only the first value of results is used
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    // Check for return value
    int markersType = -1;
    if (filterInfo.contains(QStringLiteral("addmarkers"))) {
        markersType = filterInfo.value(QStringLiteral("addmarkers")).toInt();
    }
    if (results.isEmpty()) {
        emit displayBinMessage(i18n("No data returned from clip analysis"), KMessageWidget::Warning);
        return;
    }
    bool dataProcessed = false;
    QString label = filterInfo.value(QStringLiteral("label"));
    QString key = filterInfo.value(QStringLiteral("key"));
    int offset = filterInfo.value(QStringLiteral("offset")).toInt();
    QStringList value = results.value(key).split(QLatin1Char(';'), QString::SkipEmptyParts);
    //qCDebug(KDENLIVE_LOG)<<"// RESULT; "<<key<<" = "<<value;
    if (filterInfo.contains(QStringLiteral("resultmessage"))) {
        QString mess = filterInfo.value(QStringLiteral("resultmessage"));
        mess.replace(QLatin1String("%count"), QString::number(value.count()));
        emit displayBinMessage(mess, KMessageWidget::Information);
    } else {
        emit displayBinMessage(i18n("Processing data analysis"), KMessageWidget::Information);
    }
    if (filterInfo.contains(QStringLiteral("cutscenes"))) {
        // Check if we want to cut scenes from returned data
        dataProcessed = true;
        int cutPos = 0;
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Auto Split Clip"));
        foreach (const QString &pos, value) {
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
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Add Markers"));
        QList<CommentedTime> markersList;
        int index = 1;
        bool simpleList = false;
        double sourceFps = clip->getOriginalFps();
        if (sourceFps == 0) {
            sourceFps = m_doc->fps();
        }
        if (filterInfo.contains(QStringLiteral("simplelist"))) {
            // simple list
            simpleList = true;
        }
        foreach (const QString &pos, value) {
            if (simpleList) {
                CommentedTime m(GenTime((int)(pos.toInt() * m_doc->fps() / sourceFps), m_doc->fps()), label + pos, markersType);
                markersList << m;
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
            CommentedTime m(GenTime(newPos + offset, m_doc->fps()), label + QString::number(index), markersType);
            markersList << m;
            index++;
            cutPos = newPos;
        }
        slotAddClipMarker(id, markersList);
    }
    if (!dataProcessed || filterInfo.contains(QStringLiteral("storedata"))) {
        // Store returned data as clip extra data
        QStringList newValue = clip->updatedAnalysisData(key, results.value(key), offset);
        slotAddClipExtraData(id, newValue.at(0), newValue.at(1));
    }
}

void Bin::slotAddClipMarker(const QString &id, const QList<CommentedTime> &newMarkers, QUndoCommand *groupCommand)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    if (groupCommand == nullptr) {
        groupCommand = new QUndoCommand;
        groupCommand->setText(i18np("Add marker", "Add markers", newMarkers.count()));
    }
    clip->addClipMarker(newMarkers, groupCommand);
    if (groupCommand->childCount() > 0) {
        m_doc->commandStack()->push(groupCommand);
    } else {
        delete groupCommand;
    }
}

void Bin::slotLoadClipMarkers(const QString &id)
{
    KComboBox *cbox = new KComboBox;
    for (int i = 0; i < 5; ++i) {
        cbox->insertItem(i, i18n("Category %1", i));
        cbox->setItemData(i, CommentedTime::markerColor(i), Qt::DecorationRole);
    }
    cbox->setCurrentIndex(KdenliveSettings::default_marker_type());
    //TODO KF5 how to add custom cbox to Qfiledialog
    QScopedPointer<QFileDialog> fd(new QFileDialog(this, i18n("Load Clip Markers"), m_doc->projectDataFolder()));
    fd->setMimeTypeFilters(QStringList() << QStringLiteral("text/plain"));
    fd->setFileMode(QFileDialog::ExistingFile);
    if (fd->exec() != QDialog::Accepted) {
        return;
    }
    QStringList selection = fd->selectedFiles();
    QString url;
    if (!selection.isEmpty()) {
        url = selection.first();
    }

    //QUrl url = KFileDialog::getOpenUrl(QUrl("kfiledialog:///projectfolder"), "text/plain", this, i18n("Load marker file"));
    if (url.isEmpty()) {
        return;
    }
    int category = cbox->currentIndex();
    delete cbox;
    QFile file(url);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit displayBinMessage(i18n("Cannot open file %1", QUrl::fromLocalFile(url).fileName()), KMessageWidget::Warning);
        return;
    }
    QString data = QString::fromUtf8(file.readAll());
    file.close();
    QStringList lines = data.split('\n', QString::SkipEmptyParts);
    QStringList values;
    bool ok;
    QUndoCommand *command = new QUndoCommand();
    command->setText(QStringLiteral("Load markers"));
    QString markerText;
    QList<CommentedTime> markersList;
    foreach (const QString &line, lines) {
        markerText.clear();
        values = line.split(QLatin1Char('\t'), QString::SkipEmptyParts);
        double time1 = values.at(0).toDouble(&ok);
        double time2 = -1;
        if (!ok) {
            continue;
        }
        if (values.count() > 1) {
            time2 = values.at(1).toDouble(&ok);
            if (values.count() == 2) {
                // Check if second value is a number or text
                if (!ok) {
                    time2 = -1;
                    markerText = values.at(1);
                } else {
                    markerText = i18n("Marker");
                }
            } else {
                // We assume 3 values per line: in out name
                if (!ok) {
                    // 2nd value is not a number, drop
                } else {
                    markerText = values.at(2);
                }
            }
        }
        if (!markerText.isEmpty()) {
            // Marker found, add it
            //TODO: allow user to set a marker category
            CommentedTime marker1(GenTime(time1), markerText, category);
            markersList << marker1;
            if (time2 > 0 && time2 != time1) {
                CommentedTime marker2(GenTime(time2), markerText, category);
                markersList << marker2;
            }
        }
    }
    if (!markersList.isEmpty()) {
        slotAddClipMarker(id, markersList, command);
    }
}

void Bin::slotSaveClipMarkers(const QString &id)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    QList< CommentedTime > markers = clip->commentedSnapMarkers();
    if (!markers.isEmpty()) {
        // Set  up categories
        KComboBox *cbox = new KComboBox;
        cbox->insertItem(0, i18n("All categories"));
        for (int i = 0; i < 5; ++i) {
            cbox->insertItem(i + 1, i18n("Category %1", i));
            cbox->setItemData(i + 1, CommentedTime::markerColor(i), Qt::DecorationRole);
        }
        cbox->setCurrentIndex(0);
        //TODO KF5 how to add custom cbox to Qfiledialog
        QScopedPointer<QFileDialog> fd(new QFileDialog(this, i18n("Save Clip Markers"), m_doc->projectDataFolder()));
        fd->setMimeTypeFilters(QStringList() << QStringLiteral("text/plain"));
        fd->setFileMode(QFileDialog::AnyFile);
        fd->setAcceptMode(QFileDialog::AcceptSave);
        if (fd->exec() != QDialog::Accepted) {
            return;
        }
        QStringList selection = fd->selectedFiles();
        QString url;
        if (!selection.isEmpty()) {
            url = selection.first();
        }
        //QString url = KFileDialog::getSaveFileName(QUrl("kfiledialog:///projectfolder"), "text/plain", this, i18n("Save markers"));
        if (url.isEmpty()) {
            return;
        }

        QString data;
        int category = cbox->currentIndex() - 1;
        for (int i = 0; i < markers.count(); ++i) {
            if (category >= 0) {
                // Save only the markers in selected category
                if (markers.at(i).markerType() != category) {
                    continue;
                }
            }
            data.append(QString::number(markers.at(i).time().seconds()));
            data.append("\t");
            data.append(QString::number(markers.at(i).time().seconds()));
            data.append("\t");
            data.append(markers.at(i).comment());
            data.append("\n");
        }
        delete cbox;

        QFile file(url);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit displayBinMessage(i18n("Cannot open file %1", url), KMessageWidget::Error);
            return;
        }
        file.write(data.toUtf8());
        file.close();
    }
}

void Bin::deleteClipMarker(const QString &comment, const QString &id, const GenTime &position)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    QUndoCommand *command = new QUndoCommand;
    command->setText(i18n("Delete marker"));
    CommentedTime marker(position, comment);
    marker.setMarkerType(-1);
    QList<CommentedTime> markers;
    markers << marker;
    clip->addClipMarker(markers, command);
    if (command->childCount() > 0) {
        m_doc->commandStack()->push(command);
    } else {
        delete command;
    }
}

void Bin::deleteAllClipMarkers(const QString &id)
{
    ProjectClip *clip = getBinClip(id);
    if (!clip) {
        return;
    }
    QUndoCommand *command = new QUndoCommand;
    command->setText(i18n("Delete clip markers"));
    if (!clip->deleteClipMarkers(command)) {
        doDisplayMessage(i18n("Clip has no markers"), KMessageWidget::Warning);
    }
    if (command->childCount() > 0) {
        m_doc->commandStack()->push(command);
    } else {
        delete command;
    }
}

void Bin::slotGetCurrentProjectImage(const QString &clipId, bool request)
{
    if (!clipId.isEmpty()) {
        (pCore->projectManager()->currentTimeline()->hideClip(clipId, request));
    }
    pCore->monitorManager()->projectMonitor()->slotGetCurrentImage(request);
}

// TODO: move title editing into a better place...
void Bin::showTitleWidget(ProjectClip *clip)
{
    QString path = clip->getProducerProperty(QStringLiteral("resource"));
    QDir titleFolder(m_doc->projectDataFolder() + QStringLiteral("/titles"));
    titleFolder.mkpath(QStringLiteral("."));
    TitleWidget dia_ui(QUrl(), m_doc->timecode(), titleFolder.absolutePath(), pCore->monitorManager()->projectMonitor()->render, pCore->window());
    connect(&dia_ui, &TitleWidget::requestBackgroundFrame, this, &Bin::slotGetCurrentProjectImage);
    QDomDocument doc;
    QString xmldata = clip->getProducerProperty(QStringLiteral("xmldata"));
    if (xmldata.isEmpty() && QFile::exists(path)) {
        QFile file(path);
        doc.setContent(&file, false);
        file.close();
    } else {
        doc.setContent(xmldata);
    }
    dia_ui.setXml(doc, clip->clipId());
    if (dia_ui.exec() == QDialog::Accepted) {
        QMap<QString, QString> newprops;
        newprops.insert(QStringLiteral("xmldata"), dia_ui.xml().toString());
        if (dia_ui.duration() != clip->duration().frames(m_doc->fps())) {
            // duration changed, we need to update duration
            newprops.insert(QStringLiteral("out"), QString::number(dia_ui.duration() - 1));
            int currentLength = clip->getProducerIntProperty(QStringLiteral("kdenlive:duration"));
            if (currentLength != dia_ui.duration()) {
                newprops.insert(QStringLiteral("kdenlive:duration"), QString::number(dia_ui.duration()));
            }
        }
        // trigger producer reload
        newprops.insert(QStringLiteral("force_reload"), QStringLiteral("2"));
        if (!path.isEmpty()) {
            // we are editing an external file, asked if we want to detach from that file or save the result to that title file.
            if (KMessageBox::questionYesNo(pCore->window(), i18n("You are editing an external title clip (%1). Do you want to save your changes to the title file or save the changes for this project only?", path), i18n("Save Title"), KGuiItem(i18n("Save to title file")), KGuiItem(i18n("Save in project only"))) == KMessageBox::Yes) {
                // save to external file
                dia_ui.saveTitle(QUrl::fromLocalFile(path));
            } else {
                newprops.insert(QStringLiteral("resource"), QString());
            }
        }
        slotEditClipCommand(clip->clipId(), clip->currentProperties(newprops), newprops);
    }
}

void Bin::slotResetInfoMessage()
{
    m_errorLog.clear();
    QList<QAction *> actions = m_infoMessage->actions();
    for (int i = 0; i < actions.count(); ++i) {
        m_infoMessage->removeAction(actions.at(i));
    }
}

void Bin::emitMessage(const QString &text, int progress, MessageType type)
{
    emit displayMessage(text, progress, type);
}

void Bin::slotCreateAudioThumb(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        return;
    }
    clip->createAudioThumbs();
}

void Bin::slotSetSorting()
{
    QTreeView *view = qobject_cast<QTreeView *>(m_itemView);
    if (view) {
        int ix = view->header()->sortIndicatorSection();
        m_proxyModel->setFilterKeyColumn(ix);
    }
}

void Bin::slotShowDateColumn(bool show)
{
    QTreeView *view = qobject_cast<QTreeView *>(m_itemView);
    if (view) {
        view->setColumnHidden(1, !show);
    }
}

void Bin::slotShowDescColumn(bool show)
{
    QTreeView *view = qobject_cast<QTreeView *>(m_itemView);
    if (view) {
        view->setColumnHidden(2, !show);
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
        for (const QString &i : ids) {
            deleteClip(i);
        }
    }
    delete m_invalidClipDialog;
    m_invalidClipDialog = nullptr;
}

void Bin::slotRefreshClipThumbnail(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        return;
    }
    clip->reloadProducer(true);
}

void Bin::slotAddClipExtraData(const QString &id, const QString &key, const QString &data, QUndoCommand *groupCommand)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) {
        return;
    }
    QString oldValue = clip->getProducerProperty(key);
    QMap<QString, QString> oldProps;
    oldProps.insert(key, oldValue);
    QMap<QString, QString> newProps;
    newProps.insert(key, data);
    EditClipCommand *command = new EditClipCommand(this, id, oldProps, newProps, true, groupCommand);
    if (!groupCommand) {
        m_doc->commandStack()->push(command);
    }
}

void Bin::slotUpdateClipProperties(const QString &id, const QMap<QString, QString> &properties, bool refreshPropertiesPanel)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setProperties(properties, refreshPropertiesPanel);
    }
}

void Bin::updateTimelineProducers(const QString &id, const QMap<QString, QString> &passProperties)
{
    pCore->projectManager()->currentTimeline()->updateClipProperties(id, passProperties);
    m_doc->renderer()->updateSlowMotionProducers(id, passProperties);
}

void Bin::showSlideshowWidget(ProjectClip *clip)
{
    QString folder = QFileInfo(clip->url()).absolutePath();
    qCDebug(KDENLIVE_LOG)<<" ** * CLIP ABS PATH: "<<clip->url()<<" = "<<folder;
    SlideshowClip *dia = new SlideshowClip(m_doc->timecode(), folder, clip, this);
    if (dia->exec() == QDialog::Accepted) {
        // edit clip properties
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("out"), QString::number(m_doc->getFramePos(dia->clipDuration()) * dia->imageCount() - 1));
        properties.insert(QStringLiteral("kdenlive:duration"), QString::number(m_doc->getFramePos(dia->clipDuration()) * dia->imageCount()));
        properties.insert(QStringLiteral("kdenlive:clipname"), dia->clipName());
        properties.insert(QStringLiteral("ttl"), QString::number(m_doc->getFramePos(dia->clipDuration())));
        properties.insert(QStringLiteral("loop"), QString::number(dia->loop()));
        properties.insert(QStringLiteral("crop"), QString::number(dia->crop()));
        properties.insert(QStringLiteral("fade"), QString::number(dia->fade()));
        properties.insert(QStringLiteral("luma_duration"), QString::number(m_doc->getFramePos(dia->lumaDuration())));
        properties.insert(QStringLiteral("luma_file"), dia->lumaFile());
        properties.insert(QStringLiteral("softness"), QString::number(dia->softness()));
        properties.insert(QStringLiteral("animation"), dia->animation());

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
        slotEditClipCommand(clip->clipId(), oldProperties, properties);
    }
    delete dia;
}

void Bin::slotDisableEffects(bool disable)
{
    m_rootFolder->disableEffects(disable);
    pCore->projectManager()->disableBinEffects(disable);
    m_monitor->refreshMonitorIfActive();
}

void Bin::setBinEffectsDisabledStatus(bool disabled)
{
    QAction *disableEffects = pCore->window()->actionCollection()->action(QStringLiteral("disable_bin_effects"));
    if (disableEffects) {
        if (disabled == disableEffects->isChecked()) {
            return;
        }
        disableEffects->blockSignals(true);
        disableEffects->setChecked(disabled);
        disableEffects->blockSignals(false);
    }
    pCore->projectManager()->disableBinEffects(disabled);
}

void Bin::slotRenameItem()
{
    const QModelIndexList indexes = m_proxyModel->selectionModel()->selectedRows(0);
    for (const QModelIndex &ix : indexes) {
        if (!ix.isValid()) {
            continue;
        }
        m_itemView->setCurrentIndex(ix);
        m_itemView->edit(ix);
        return;
    }
}

void Bin::refreshProxySettings()
{
    QList<ProjectClip *> clipList = m_rootFolder->childClips();
    QUndoCommand *masterCommand = new QUndoCommand();
    masterCommand->setText(m_doc->useProxy() ? i18n("Enable proxies") : i18n("Disable proxies"));
    if (!m_doc->useProxy()) {
        // Disable all proxies
        m_doc->slotProxyCurrentItem(false, clipList, false, masterCommand);
    } else {
        QList<ProjectClip *> toProxy;
        foreach (ProjectClip *clp, clipList) {
            ClipType t = clp->clipType();
            if (t == Playlist) {
                toProxy << clp;
                continue;
            } else if ((t == AV || t == Video)
                       && m_doc->autoGenerateProxy(clp->getProducerIntProperty(QStringLiteral("meta.media.width")))) {
                // Start proxy
                toProxy << clp;
                continue;
            } else if (t == Image
                       && m_doc->autoGenerateImageProxy(clp->getProducerIntProperty(QStringLiteral("meta.media.width")))) {
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
    QList<ProjectClip *> clipList = m_rootFolder->childClips();
    foreach (ProjectClip *clp, clipList) {
        if (clp->clipType() == AV || clp->clipType() == Video || clp->clipType() == Playlist) {
            list << clp->hash();
        }
    }
    return list;
}

void Bin::slotSendAudioThumb(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip && clip->audioThumbCreated()) {
        m_monitor->prepareAudioThumb(clip->audioChannels(), clip->audioFrameCache);
    } else {
        QVariantList list;
        m_monitor->prepareAudioThumb(0, list);
    }
}

bool Bin::isEmpty() const
{
    // TODO: return true if we only have folders
    if (m_clipCounter == 1 || m_rootFolder == nullptr) {
        return true;
    }
    return m_rootFolder->isEmpty();
}

void Bin::reloadAllProducers()
{
    if (m_rootFolder == nullptr || m_rootFolder->isEmpty() || !isEnabled()) {
        return;
    }
    QList<ProjectClip *> clipList = m_rootFolder->childClips();
    emit openClip(nullptr);
    foreach (ProjectClip *clip, clipList) {
        QDomDocument doc;
        QDomElement xml = clip->toXml(doc);
        // Make sure we reload clip length
        xml.removeAttribute(QStringLiteral("out"));
        EffectsList::removeProperty(xml, QStringLiteral("length"));
        if (!xml.isNull()) {
            clip->setClipStatus(AbstractProjectItem::StatusWaiting);
            clip->discardAudioThumb();
            // We need to set a temporary id before all outdated producers are replaced;
            m_doc->getFileProperties(xml, clip->clipId(), 150, true);
        }
    }
}

void Bin::slotMessageActionTriggered()
{
    m_infoMessage->animatedHide();
}

void Bin::resetUsageCount()
{
    const QList<ProjectClip *> clipList = m_rootFolder->childClips();
    for (ProjectClip *clip : clipList) {
        clip->setRefCount(0);
    }
}

void Bin::cleanup()
{
    QList<ProjectClip *> clipList = m_rootFolder->childClips();
    QStringList ids;
    QStringList subIds;
    foreach (ProjectClip *clip, clipList) {
        if (clip->refCount() == 0) {
            ids << clip->clipId();
            subIds << clip->subClipIds();
        }
    }
    QUndoCommand *command = new QUndoCommand();
    command->setText(i18n("Clean Project"));
    m_doc->clipManager()->doDeleteClips(ids, QStringList(), subIds, command, true);
}

void Bin::getBinStats(uint *used, uint *unused, qint64 *usedSize, qint64 *unusedSize)
{
    QList<ProjectClip *> clipList = m_rootFolder->childClips();
    foreach (ProjectClip *clip, clipList) {
        if (clip->refCount() == 0) {
            *unused += 1;
            *unusedSize += clip->getProducerInt64Property(QStringLiteral("kdenlive:file_size"));
        } else {
            *used += 1;
            *usedSize += clip->getProducerInt64Property(QStringLiteral("kdenlive:file_size"));
        }
    }
}

QImage Bin::findCachedPixmap(const QString &path)
{
    QImage img;
    m_doc->clipManager()->pixmapCache->findImage(path, &img);
    return img;
}

void Bin::cachePixmap(const QString &path, const QImage &img)
{
    if (!m_doc->clipManager()->pixmapCache->contains(path)) {
        m_doc->clipManager()->pixmapCache->insertImage(path, img);
    }
}

QDir Bin::getCacheDir(CacheType type, bool *ok) const
{
    return m_doc->getCacheDir(type, ok);
}

bool Bin::addClip(QDomElement elem, const QString &clipId)
{
    const QString producerId = clipId.section(QLatin1Char('_'), 0, 0);
    elem.setAttribute(QStringLiteral("id"), producerId);
    if ((KdenliveSettings::default_profile().isEmpty() || KdenliveSettings::checkfirstprojectclip()) && isEmpty()) {
        elem.setAttribute(QStringLiteral("checkProfile"), 1);
    }
    createClip(elem);
    m_doc->getFileProperties(elem, producerId, 150, true);
    return true;
}

void Bin::rebuildProxies()
{
    QList<ProjectClip *> clipList = m_rootFolder->childClips();
    QList<ProjectClip *> toProxy;
    foreach (ProjectClip *clp, clipList) {
        if (clp->hasProxy()) {
            toProxy << clp;
        }
    }
    if (toProxy.isEmpty()) {
        return;
    }
    QUndoCommand *masterCommand = new QUndoCommand();
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
    ProjectClip *clip = getBinClip(info.first());
    if (clip && clip->controller()) {
        QPoint zone(info.at(1).toInt(), info.at(2).toInt());
        clip->controller()->saveZone(zone, dir);
    }
}
