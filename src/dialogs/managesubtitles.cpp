/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "managesubtitles.h"
#include "bin/model/subtitlemodel.hpp"
#include "core.h"
#include "dialogs/importsubtitle.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include "mainwindow.h"
#include "subtitlestyleedit.h"
#include "timeline2/view/timelinecontroller.h"
#include <KMessageBox>
#include <QFontDatabase>
#include <QListWidget>
#include <QMenu>
#include <QTableWidget>
#include <qinputdialog.h>
#include <qmimedata.h>

SideBarDropFilter::SideBarDropFilter(QObject *parent)
    : QObject(parent)
{
}

bool SideBarDropFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::DragEnter) {
        QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent *>(event);
        dragEvent->acceptProposedAction();
    }
    if (event->type() == QEvent::Drop) {
        QDropEvent *dropEvent = static_cast<QDropEvent *>(event);
        Q_EMIT drop(obj, dropEvent);
        return true;
    }
    return QObject::eventFilter(obj, event);
}

ManageSubtitles::ManageSubtitles(std::shared_ptr<SubtitleModel> model, TimelineController *controller, int currentIx, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_controller(controller)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18nc("@title:window", "Manage Subtitles"));
    Qt::WindowFlags flags = Qt::Dialog;
    flags |= Qt::WindowMaximizeButtonHint;
    flags |= Qt::WindowCloseButtonHint;
    setWindowFlags(flags);
    messageWidget->hide();

    m_activeSubFile = currentIx;

    parseFileList();
    parseEventList();
    parseStyleList(false);
    parseInfoList();
    // parseSideBar();

    SideBarDropFilter *dropFilter = new SideBarDropFilter(this);
    eventFileSideBar->viewport()->installEventFilter(dropFilter);
    styleFileSideBar->viewport()->installEventFilter(dropFilter);

    connect(dropFilter, &SideBarDropFilter::drop, this, [&](QObject *obj, QDropEvent *event) {
        if (obj->parent() == eventFileSideBar) {
            if (!eventList->currentItem()->text(0).contains(i18n("Layer"))) {
                return;
            }
            int ix = event->position().y() / eventFileSideBar->sizeHintForRow(0);
            if (ix > eventFileSideBar->count() - 1) {
                return;
            }
            int id = eventFileSideBar->item(ix)->data(Qt::UserRole).toInt();

            auto copyMoveLayer = [this, id](bool isMove) {
                int currentIx = -1;
                int newIx = -1;
                QStringList subKeys;
                QMap<std::pair<int, QString>, QString> subs = this->m_model->getSubtitlesList();
                for (auto &sub : subs.keys()) {
                    subKeys.append(QString::number(sub.first) + " - " + sub.second);
                    if (sub.first == m_activeSubFile) {
                        currentIx = subKeys.size() - 1;
                    }
                    if (sub.first == id) {
                        newIx = subKeys.size() - 1;
                    }
                }

                bool ok;
                QString sub;
                if (isMove) {
                    sub = QInputDialog::getItem(this, i18n("Move Layer"), i18n("Select the subtitle file to move the layer to:"), subKeys, newIx, false, &ok);
                } else {
                    sub = QInputDialog::getItem(this, i18n("Copy Layer"), i18n("Select the subtitle file to copy the layer to:"), subKeys, newIx, false, &ok);
                }
                if (ok) {
                    // events
                    int layer = eventList->currentItem()->text(0).section(" ", 1, 1).toInt();
                    QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> events;
                    for (auto &event : m_model->getAllSubtitles()) {
                        if (event.first.first == layer) {
                            events.append(event);
                        }
                    }

                    if (isMove) {
                        m_model->requestDeleteLayer(layer);
                    }
                    m_controller->subtitlesMenuActivated(newIx);
                    m_model->requestCreateLayer();
                    int newLayer = m_model->getMaxLayer();
                    for (auto &event : events) {
                        // TODO: undo/redo
                        Fun undo = []() { return true; };
                        Fun redo = []() { return true; };
                        event.first.first = newLayer;
                        m_model->addSubtitle(event.first, event.second, undo, redo);
                    }
                    m_controller->subtitlesMenuActivated(currentIx);
                    parseEventList();
                }
            };

            QAction *actionMoveLayer = new QAction(i18n("Move To..."), this);
            actionMoveLayer->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
            connect(actionMoveLayer, &QAction::triggered, this, [&copyMoveLayer]() { copyMoveLayer(true); });
            QAction *actionCopyLayer = new QAction(i18n("Copy To..."), this);
            actionCopyLayer->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
            connect(actionCopyLayer, &QAction::triggered, this, [&copyMoveLayer]() { copyMoveLayer(false); });
            QMenu *layerMenu = new QMenu(this);
            layerMenu->addAction(actionCopyLayer);
            if (eventList->topLevelItemCount() > 1) {
                layerMenu->addAction(actionMoveLayer);
            }
            layerMenu->exec(eventFileSideBar->mapToGlobal(event->position()).toPoint());
        } else if (obj->parent() == styleFileSideBar) {
            bool fromGlobal = styleFileSideBar->currentItem()->data(Qt::UserRole).toInt() == -1;
            bool toGlobal = false;
            int ix = event->position().y() / styleFileSideBar->sizeHintForRow(0);
            if (ix > styleFileSideBar->count() - 1) {
                return;
            }
            if (ix == styleFileSideBar->count() - 1) {
                toGlobal = true;
            }
            int id = styleFileSideBar->item(ix)->data(Qt::UserRole).toInt();

            auto copyMoveStyle = [this, fromGlobal, toGlobal, id](bool isMove) {
                int currentIx = -1;
                int newIx = -1;
                QStringList subKeys;
                QMap<std::pair<int, QString>, QString> subs = this->m_model->getSubtitlesList();
                for (auto &sub : subs.keys()) {
                    subKeys.append(QString::number(sub.first) + " - " + sub.second);
                    if (sub.first == m_activeSubFile) {
                        currentIx = subKeys.size() - 1;
                    }
                    if (sub.first == id) {
                        newIx = subKeys.size() - 1;
                    }
                }
                subKeys.append(QString::number(subKeys.count()) + " - " + i18n("Global"));
                if (toGlobal) {
                    newIx = subKeys.size() - 1;
                }

                SubtitleStyle style = m_model->getSubtitleStyle(styleList->currentItem()->text(0), fromGlobal);
                QString styleName = styleList->currentItem()->text(0);
                bool ok;
                QString sub;
                if (isMove) {
                    sub = QInputDialog::getItem(this, i18n("Move Style"), i18n("Select the subtitle file to move the style to:"), subKeys, newIx, false, &ok);
                } else {
                    sub = QInputDialog::getItem(this, i18n("Copy Style"), i18n("Select the subtitle file to copy the style to:"), subKeys, newIx, false, &ok);
                }
                if (ok) {
                    if (isMove) {
                        m_model->deleteSubtitleStyle(styleName, fromGlobal);
                    }
                    if (!toGlobal) {
                        m_controller->subtitlesMenuActivated(newIx);
                    }
                    // avoid conflict with existing style
                    QStringList existingStyleNames;
                    for (auto &style : m_model->getAllSubtitleStyles(toGlobal)) {
                        existingStyleNames.append(style.first);
                    }
                    if (existingStyleNames.contains(styleName)) {
                        int suffix = 1;
                        while (true) {
                            if (!existingStyleNames.contains(styleName + " " + QString::number(suffix))) {
                                styleName = styleName + " " + QString::number(suffix);
                                break;
                            }
                            suffix++;
                        }
                    }

                    m_model->setSubtitleStyle(styleName, style, toGlobal);
                    m_controller->subtitlesMenuActivated(currentIx);
                    parseStyleList(fromGlobal);
                }
            };

            QAction *actionMoveStyle = new QAction(i18n("Move To..."), this);
            actionMoveStyle->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
            connect(actionMoveStyle, &QAction::triggered, this, [&copyMoveStyle]() { copyMoveStyle(true); });
            QAction *actionCopyStyle = new QAction(i18n("Copy To..."), this);
            actionCopyStyle->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
            connect(actionCopyStyle, &QAction::triggered, this, [&copyMoveStyle]() { copyMoveStyle(false); });
            QMenu *styleMenu = new QMenu(this);
            styleMenu->addAction(actionCopyStyle);
            if (fromGlobal || (styleList->currentItem() && styleList->currentItem()->text(0) != "Default")) {
                styleMenu->addAction(actionMoveStyle);
            }
            styleMenu->exec(styleFileSideBar->mapToGlobal(event->position()).toPoint());
        }
    });

    connect(fileList, &QTreeWidget::itemChanged, this, &ManageSubtitles::updateSubtitle);
    connect(fileList, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current, QTreeWidgetItem *previous) {
        QSignalBlocker bk(eventFileSideBar);
        QSignalBlocker bk2(styleFileSideBar);
        QSignalBlocker bk3(infoFileSideBar);
        buttonDeleteFile->setEnabled(!(fileList->topLevelItemCount() == 1));
        int id = current->data(0, Qt::UserRole).toInt();
        if (id > -1) {
            int ix = fileList->indexOfTopLevelItem(current);
            m_controller->subtitlesMenuActivated(ix);

            m_activeSubFile = id;
            parseEventList();
            parseStyleList(false);
            parseInfoList();
            eventFileSideBar->setCurrentRow(fileList->indexOfTopLevelItem(current));
            styleFileSideBar->setCurrentRow(fileList->indexOfTopLevelItem(current));
            infoFileSideBar->setCurrentRow(fileList->indexOfTopLevelItem(current));
        } else {
            QSignalBlocker bk4(fileList);
            fileList->setCurrentItem(previous);
        }
    });
    connect(eventList, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *current, int column) {
        if (current->parent() != nullptr) {
            return;
        }
        if (column != 2) {
            return;
        }

        int layer = current->text(0).section(" ", 1, 1).toInt();
        QString currentStyle = current->text(2);
        QMenu *menu = new QMenu(this);
        for (const auto &style : m_model->getAllSubtitleStyles(false)) {
            menu->addAction(style.first);
        }
        QAction *action = menu->exec(QCursor::pos());
        if (action) {
            QString styleName = action->toolTip();
            m_model->setLayerDefaultStyle(layer, styleName);
            current->setText(2, styleName);
        }
    });
    connect(styleList, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        if (styleFileSideBar->currentItem()->data(Qt::UserRole).toInt() == -1) {
            buttonDeleteStyle->setEnabled(styleList->topLevelItemCount() > 0);
        } else {
            buttonDeleteStyle->setEnabled(current->text(0) != "Default");
        }
    });
    connect(eventFileSideBar, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *previous) {
        QSignalBlocker bk(fileList);
        QSignalBlocker bk2(styleFileSideBar);
        QSignalBlocker bk3(infoFileSideBar);
        buttonDeleteLayer->setEnabled(!(eventList->topLevelItemCount() == 1));
        int id = current->data(Qt::UserRole).toInt();
        if (id > -1) {
            int ix = eventFileSideBar->row(current);
            m_controller->subtitlesMenuActivated(ix);

            m_activeSubFile = id;
            parseEventList();
            parseStyleList(false);
            parseInfoList();
            fileList->setCurrentItem(fileList->topLevelItem(eventFileSideBar->currentRow()));
            styleFileSideBar->setCurrentRow(eventFileSideBar->currentRow());
            infoFileSideBar->setCurrentRow(eventFileSideBar->currentRow());
        } else {
            QSignalBlocker bk4(eventFileSideBar);
            eventFileSideBar->setCurrentItem(previous);
        }
    });
    connect(styleFileSideBar, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *previous) {
        QSignalBlocker bk(fileList);
        QSignalBlocker bk2(eventFileSideBar);
        QSignalBlocker bk3(infoFileSideBar);
        if (current->data(Qt::UserRole).toInt() == -1) {
            parseStyleList(true);
        } else {
            int id = current->data(Qt::UserRole).toInt();
            if (id > -1) {
                int ix = styleFileSideBar->row(current);
                m_controller->subtitlesMenuActivated(ix);

                m_activeSubFile = id;
                parseEventList();
                parseStyleList(false);
                parseInfoList();
                fileList->setCurrentItem(fileList->topLevelItem(styleFileSideBar->currentRow()));
                eventFileSideBar->setCurrentRow(styleFileSideBar->currentRow());
                infoFileSideBar->setCurrentRow(styleFileSideBar->currentRow());
            } else {
                QSignalBlocker bk4(styleFileSideBar);
                styleFileSideBar->setCurrentItem(previous);
            }
        }
    });
    connect(infoFileSideBar, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *previous) {
        QSignalBlocker bk(fileList);
        QSignalBlocker bk2(eventFileSideBar);
        QSignalBlocker bk3(styleFileSideBar);
        int id = current->data(Qt::UserRole).toInt();
        if (id > -1) {
            int ix = infoFileSideBar->row(current);
            m_controller->subtitlesMenuActivated(ix);

            m_activeSubFile = id;
            parseEventList();
            parseStyleList(false);
            parseInfoList();
            fileList->setCurrentItem(fileList->topLevelItem(infoFileSideBar->currentRow()));
            eventFileSideBar->setCurrentRow(infoFileSideBar->currentRow());
            styleFileSideBar->setCurrentRow(infoFileSideBar->currentRow());
        } else {
            QSignalBlocker bk4(infoFileSideBar);
            infoFileSideBar->setCurrentItem(previous);
        }
    });
    connect(buttonNewFile, &QPushButton::clicked, this, [this]() { addSubtitleFile(); });
    connect(buttonDuplicateFile, &QPushButton::clicked, this, &ManageSubtitles::duplicateFile);
    connect(buttonDeleteFile, &QPushButton::clicked, this, &ManageSubtitles::deleteFile);
    connect(buttonNewLayer, &QPushButton::clicked, this, &ManageSubtitles::addLayer);
    connect(buttonDuplicateLayer, &QPushButton::clicked, this, &ManageSubtitles::duplicateLayer);
    connect(buttonDeleteLayer, &QPushButton::clicked, this, &ManageSubtitles::deleteLayer);
    connect(buttonNewStyle, &QPushButton::clicked, this, [this]() {
        if (styleFileSideBar->currentItem()->data(Qt::UserRole).toInt() == -1) {
            addStyle(true);
        } else {
            addStyle(false);
        }
    });

    connect(buttonDuplicateStyle, &QPushButton::clicked, this, [this]() {
        if (styleFileSideBar->currentItem()->data(Qt::UserRole).toInt() == -1) {
            duplicateStyle(true);
        } else {
            duplicateStyle(false);
        }
    });
    connect(buttonDeleteStyle, &QPushButton::clicked, this, [this]() {
        if (styleFileSideBar->currentItem()->data(Qt::UserRole).toInt() == -1) {
            deleteStyle(true);
        } else {
            deleteStyle(false);
        }
    });
    connect(buttonEditStyle, &QPushButton::clicked, this, [this]() {
        if (styleFileSideBar->currentItem()->data(Qt::UserRole).toInt() == -1) {
            editStyle(true);
        } else {
            editStyle(false);
        }
    });

    // Import/Export menu
    QMenu *menu = new QMenu(this);
    QAction *importSub = new QAction(QIcon::fromTheme(QStringLiteral("document-import")), i18nc("@action:inmenu", "Import Subtitle"), this);
    QAction *exportSub = new QAction(QIcon::fromTheme(QStringLiteral("document-export")), i18nc("@action:inmenu", "Export Subtitle"), this);
    menu->addAction(importSub);
    menu->addAction(exportSub);
    connect(importSub, &QAction::triggered, this, &ManageSubtitles::importSubtitleFile);
    connect(exportSub, &QAction::triggered, pCore->window(), &MainWindow::slotExportSubtitle);
    buttonMenuFile->setMenu(menu);
}

ManageSubtitles::~ManageSubtitles()
{
    QSignalBlocker bk(fileList);
    QSignalBlocker bk2(eventFileSideBar);
    QSignalBlocker bk3(styleFileSideBar);
    QSignalBlocker bk4(eventList);
    QSignalBlocker bk5(styleList);
}

void ManageSubtitles::parseFileList()
{
    QSignalBlocker bk(fileList);
    fileList->clear();
    QMap<std::pair<int, QString>, QString> subs = m_model->getSubtitlesList();
    QMapIterator<std::pair<int, QString>, QString> i(subs);
    QTreeWidgetItem *currentItem = nullptr;
    while (i.hasNext()) {
        i.next();

        // set subtitle
        QTreeWidgetItem *item = new QTreeWidgetItem(fileList, {i.key().second, i.value()});
        item->setData(0, Qt::UserRole, i.key().first);

        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if (m_activeSubFile > -1 && m_activeSubFile == i.key().first) {
            currentItem = item;
        }
    }
    if (currentItem) {
        fileList->setCurrentItem(currentItem);
    }
    buttonDeleteFile->setEnabled(fileList->topLevelItemCount() > 1);
    parseSideBar();
}

void ManageSubtitles::parseEventList()
{
    QSignalBlocker bk(eventList);
    eventList->clear();
    // m_model->activateSubtitle(i.key().first);
    const int maxLayer = m_model->getMaxLayer();
    const QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubs = m_model->getAllSubtitles();

    // add layer items
    for (int i = 0; i <= maxLayer; i++) {
        new QTreeWidgetItem(eventList,
                            {i18n("Layer ") + QString::number(i), i18n("with ") + QString::number(0) + i18n(" event"), m_model->getLayerDefaultStyle(i)});
    }

    // add subtitle events
    for (const auto &sub : allSubs) {
        int count = eventList->topLevelItem(sub.first.first)->text(1).section(" ", 1, 1).toInt() + 1;
        eventList->topLevelItem(sub.first.first)->setText(1, "with " + QString::number(count) + (count == 1 ? i18n(" event") : i18n(" events")));
        new QTreeWidgetItem(eventList->topLevelItem(sub.first.first),
                            {sub.first.second.toString(), sub.second.endTime().toString(), sub.second.styleName(), sub.second.text()});
    }

    eventList->setCurrentItem(eventList->topLevelItem(0));
    buttonDeleteLayer->setEnabled(eventList->topLevelItemCount() > 1);
}

void ManageSubtitles::parseStyleList(bool global)
{
    QSignalBlocker bk(styleList);
    styleList->clear();

    std::map<QString, SubtitleStyle> styles = m_model->getAllSubtitleStyles(global);
    for (const auto &style : styles) {
        QTreeWidgetItem *styleItem = new QTreeWidgetItem(styleList, {style.first,
                                                                     style.second.fontName(),
                                                                     QString::number(style.second.fontSize()),
                                                                     style.second.bold() ? "Yes" : "No",
                                                                     style.second.italic() ? "Yes" : "No",
                                                                     style.second.underline() ? "Yes" : "No",
                                                                     style.second.strikeOut() ? "Yes" : "No",
                                                                     "",
                                                                     "",
                                                                     "",
                                                                     "",
                                                                     QString::number(style.second.outline()),
                                                                     QString::number(style.second.shadow()),
                                                                     "",
                                                                     QString::number(style.second.scaleX()),
                                                                     QString::number(style.second.scaleY()),
                                                                     QString::number(style.second.spacing()),
                                                                     QString::number(style.second.angle()),
                                                                     "",
                                                                     QString::number(style.second.marginL()),
                                                                     QString::number(style.second.marginR()),
                                                                     QString::number(style.second.marginV()),
                                                                     QString::number(style.second.encoding())});
        QLabel *labelPrimary = new QLabel();
        labelPrimary->setStyleSheet(QStringLiteral("background-color: %1").arg(style.second.primaryColour().name()));
        styleList->setItemWidget(styleItem, 7, labelPrimary);
        QLabel *labelSecondary = new QLabel();
        labelSecondary->setStyleSheet(QStringLiteral("background-color: %1").arg(style.second.secondaryColour().name()));
        styleList->setItemWidget(styleItem, 8, labelSecondary);
        QLabel *labelOutline = new QLabel();
        labelOutline->setStyleSheet(QStringLiteral("background-color: %1").arg(style.second.outlineColour().name()));
        styleList->setItemWidget(styleItem, 9, labelOutline);
        QLabel *labelBack = new QLabel();
        labelBack->setStyleSheet(QStringLiteral("background-color: %1").arg(style.second.backColour().name()));
        styleList->setItemWidget(styleItem, 10, labelBack);
        switch (style.second.alignment()) {
        case 1:
            styleItem->setText(13, i18n("Bottom Left"));
            break;
        case 2:
            styleItem->setText(13, i18n("Bottom"));
            break;
        case 3:
            styleItem->setText(13, i18n("Bottom Right"));
            break;
        case 4:
            styleItem->setText(13, i18n("Left"));
            break;
        case 5:
            styleItem->setText(13, i18n("Center"));
            break;
        case 6:
            styleItem->setText(13, i18n("Right"));
            break;
        case 7:
            styleItem->setText(13, i18n("Top Left"));
            break;
        case 8:
            styleItem->setText(13, i18n("Top"));
            break;
        case 9:
            styleItem->setText(13, i18n("Top Right"));
            break;
        }

        switch (style.second.borderStyle()) {
        case 1:
            styleItem->setText(18, i18n("Default"));
            break;
        case 3:
            styleItem->setText(18, i18n("Box as Border"));
            break;
        case 4:
            styleItem->setText(18, i18n("Box as Shadow"));
            break;
        }
    }
    styleList->setCurrentItem(styleList->topLevelItem(0));

    if (global) {
        buttonDeleteStyle->setEnabled(styleList->topLevelItemCount() > 0);
        buttonEditStyle->setEnabled(styleList->topLevelItemCount() > 0);
        buttonDuplicateStyle->setEnabled(styleList->topLevelItemCount() > 0);
    } else {
        buttonDeleteStyle->setEnabled(styleList->currentItem() && styleList->currentItem()->text(0) != "Default");
        buttonEditStyle->setEnabled(styleList->currentItem());
        buttonDuplicateStyle->setEnabled(styleList->currentItem());
    }
}

void ManageSubtitles::parseInfoList()
{
    QSignalBlocker bk(infoList);
    infoList->clear();
    std::map<QString, QString> info = m_model->scriptInfo();
    for (const auto &i : info) {
        new QTreeWidgetItem(infoList, {i.first, i.second});
    }
}

void ManageSubtitles::parseSideBar()
{
    QSignalBlocker bk1(eventFileSideBar);
    QSignalBlocker bk2(styleFileSideBar);
    QSignalBlocker bk3(infoFileSideBar);
    eventFileSideBar->clear();
    styleFileSideBar->clear();
    infoFileSideBar->clear();
    for (int topLevelIndex = 0; topLevelIndex < fileList->topLevelItemCount(); topLevelIndex++) {
        QTreeWidgetItem *topLevelItem = fileList->topLevelItem(topLevelIndex);
        QListWidgetItem *newItem = new QListWidgetItem(eventFileSideBar);
        newItem->setText(topLevelItem->text(0));
        newItem->setData(Qt::UserRole, topLevelItem->data(0, Qt::UserRole));

        newItem = new QListWidgetItem(styleFileSideBar);
        newItem->setText(topLevelItem->text(0));
        newItem->setData(Qt::UserRole, topLevelItem->data(0, Qt::UserRole));

        newItem = new QListWidgetItem(infoFileSideBar);
        newItem->setText(topLevelItem->text(0));
        newItem->setData(Qt::UserRole, topLevelItem->data(0, Qt::UserRole));
    }
    QListWidgetItem *newItem = new QListWidgetItem(styleFileSideBar);
    newItem->setText(i18n("Global"));
    QFont font = newItem->font();
    font.setBold(true);
    newItem->setData(Qt::FontRole, font);
    newItem->setData(Qt::UserRole, -1);

    eventFileSideBar->setCurrentRow(fileList->indexOfTopLevelItem(fileList->currentItem()));
    styleFileSideBar->setCurrentRow(fileList->indexOfTopLevelItem(fileList->currentItem()));
    infoFileSideBar->setCurrentRow(fileList->indexOfTopLevelItem(fileList->currentItem()));
}

void ManageSubtitles::updateSubtitle(QTreeWidgetItem *item, int column)
{
    if (item->parent() == nullptr) {
        if (column == 0) {
            // An item was renamed
            m_model->updateModelName(item->data(0, Qt::UserRole).toInt(), item->text(0));
            m_controller->refreshSubtitlesComboIndex();
        }
        if (column == 1) {
            item->setText(1, m_model->getSubtitlesList()[{item->data(0, Qt::UserRole).toInt(), item->text(0)}]);
        }
    }
}

void ManageSubtitles::addSubtitleFile(const QString name)
{
    m_model->createNewSubtitle(name);
    parseFileList();
    // Makes last item active
    fileList->setCurrentItem(fileList->topLevelItem(fileList->topLevelItemCount() - 1));
}

void ManageSubtitles::addLayer()
{
    m_model->requestCreateLayer();
    parseEventList();
    eventList->setCurrentItem(eventList->topLevelItem(eventList->topLevelItemCount() - 1));
}

void ManageSubtitles::addStyle(bool global)
{
    bool ok;
    QString styleName = "";
    SubtitleStyle style = SubtitleStyleEdit::getStyle(this, SubtitleStyle(), styleName, m_model, global, &ok);
    if (ok) {
        m_model->setSubtitleStyle(styleName, style, global);
        parseStyleList(global);
        styleList->setCurrentItem(styleList->topLevelItem(styleList->topLevelItemCount() - 1));
    }
}

void ManageSubtitles::duplicateFile()
{
    m_model->createNewSubtitle(QString(), fileList->currentItem()->data(0, Qt::UserRole).toInt());
    parseFileList();
    // Makes last item active
    fileList->setCurrentItem(fileList->topLevelItem(fileList->topLevelItemCount() - 1));
}

void ManageSubtitles::duplicateLayer()
{
    QTreeWidgetItem *current = eventList->currentItem();
    while (current->parent() != nullptr) {
        current = current->parent();
    }
    m_model->requestCreateLayer(current->text(0).section(" ", 1, 1).toInt());
    parseEventList();
    eventList->setCurrentItem(eventList->topLevelItem(eventList->topLevelItemCount() - 1));
}

void ManageSubtitles::duplicateStyle(bool global)
{
    QTreeWidgetItem *current = styleList->currentItem();
    QString styleName = current->text(0);
    const SubtitleStyle style = m_model->getSubtitleStyle(styleName, global);
    QStringList existingStyleNames;
    for (auto &style : m_model->getAllSubtitleStyles(global)) {
        existingStyleNames.append(style.first);
    }

    int suffix = 1;
    while (true) {
        if (!existingStyleNames.contains("Style " + QString::number(suffix))) {
            styleName = "Style " + QString::number(suffix);
            break;
        }
        suffix++;
    }
    m_model->setSubtitleStyle(styleName, style, global);

    parseStyleList(global);
    styleList->setCurrentItem(styleList->topLevelItem(styleList->topLevelItemCount() - 1));
}

void ManageSubtitles::deleteFile()
{
    const QString name = fileList->currentItem()->text(0);
    const QString path = fileList->currentItem()->text(1);
    if (KMessageBox::warningContinueCancel(
            this, i18n("This will delete all subtitle entries in <b>%1</b>,<br/>as well as the subtitle file: %2", name, path)) != KMessageBox::Continue) {
        return;
    }
    qDebug() << ":::: DELETING SUBTITLE WIT TREE INDEX: " << fileList->indexOfTopLevelItem(fileList->currentItem());
    QTreeWidgetItem *currentItem = fileList->currentItem();
    int ix = fileList->indexOfTopLevelItem(currentItem);
    int id = currentItem->data(0, Qt::UserRole).toInt();
    m_model->deleteSubtitle(id);
    parseFileList();
    buttonDeleteFile->setEnabled(!(fileList->topLevelItemCount() == 1));
    if (ix < fileList->topLevelItemCount()) {
        fileList->setCurrentItem(fileList->topLevelItem(ix));
    } else {
        fileList->setCurrentItem(fileList->topLevelItem(ix - 1));
    }
}

void ManageSubtitles::deleteLayer()
{
    QTreeWidgetItem *currentItem = eventList->currentItem();
    // Layer
    int layer = currentItem->text(0).section(" ", 1, 1).toInt();
    if (KMessageBox::warningContinueCancel(this, i18n("This will delete all events in layer <b>%1</b>.", layer)) != KMessageBox::Continue) {
        return;
    }
    m_model->requestDeleteLayer(layer);
    parseEventList();
    buttonDeleteLayer->setEnabled(!(eventList->topLevelItemCount() == 1));
    if (layer < eventList->topLevelItemCount()) {
        eventList->setCurrentItem(eventList->topLevelItem(layer));
    } else {
        eventList->setCurrentItem(eventList->topLevelItem(layer - 1));
    }
}

void ManageSubtitles::deleteStyle(bool global)
{
    QTreeWidgetItem *current = styleList->currentItem();
    const QString name = current->text(0);
    if (!global && name == "Default") {
        KMessageBox::error(this, i18n("Cannot delete default style."));
        return;
    }
    if (global) {
        if (KMessageBox::warningContinueCancel(this, i18n("This will delete the style <b>%1</b> and all its properties.", name)) != KMessageBox::Continue) {
            return;
        }
    } else {
        if (KMessageBox::warningContinueCancel(
                this, i18n("This will delete the style <b>%1</b> and all its properties.\nSubtitles using this style will be reset to style <b>Default</b>.",
                           name)) != KMessageBox::Continue) {
            return;
        }
    }
    m_model->deleteSubtitleStyle(name, global);
    parseStyleList(global);
}

void ManageSubtitles::editStyle(bool global)
{
    QTreeWidgetItem *current = styleList->currentItem();
    QString styleName = current->text(0);
    SubtitleStyle style = m_model->getSubtitleStyle(styleName, global);
    bool ok;
    style = SubtitleStyleEdit::getStyle(this, style, styleName, m_model, global, &ok);
    if (ok) {
        m_model->setSubtitleStyle(styleName, style, global);
        parseStyleList(global);
        styleList->setCurrentItem(styleList->topLevelItem(styleList->topLevelItemCount() - 1));
    }
}

void ManageSubtitles::importSubtitleFile()
{
    QScopedPointer<ImportSubtitle> d(new ImportSubtitle(QString(), this));
    d->create_track->setChecked(true);
    if (d->exec() == QDialog::Accepted && !d->subtitle_url->url().isEmpty()) {
        if (d->create_track->isChecked()) {
            // Create a new subtitle entry
            addSubtitleFile(d->track_name->text());
        }
        int offset = 0, startFramerate = 30.00, targetFramerate = 30.00;
        if (d->cursor_pos->isChecked()) {
            offset = pCore->getMonitorPosition();
        }
        if (d->transform_framerate_check_box->isChecked()) {
            startFramerate = d->caption_original_framerate->value();
            targetFramerate = d->caption_target_framerate->value();
        }
        m_model->importSubtitle(d->subtitle_url->url().toLocalFile(), offset, true, startFramerate, targetFramerate,
                                d->codecs_list->currentData().toString().toUtf8());
        parseFileList();
        parseStyleList(false);
        parseEventList();
        parseInfoList();
    }
}
