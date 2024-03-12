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
#include "timeline2/view/timelinecontroller.h"
#include <KMessageBox>
#include <QFontDatabase>
#include <QMenu>

ManageSubtitles::ManageSubtitles(std::shared_ptr<SubtitleModel> model, TimelineController *controller, int currentIx, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_controller(controller)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18nc("@title:window", "Manage Subtitles"));
    subtitlesList->hideColumn(1);
    messageWidget->hide();
    parseList(currentIx);
    connect(subtitlesList, &QTreeWidget::itemChanged, this, &ManageSubtitles::updateSubtitle);
    connect(subtitlesList, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        int ix = subtitlesList->indexOfTopLevelItem(current);
        if (ix > -1) {
            m_controller->subtitlesMenuActivated(ix);
        }
    });
    connect(button_new, &QPushButton::clicked, this, [this]() { addSubtitle(); });
    connect(button_duplicate, &QPushButton::clicked, this, &ManageSubtitles::duplicateSubtitle);
    connect(button_delete, &QPushButton::clicked, this, &ManageSubtitles::deleteSubtitle);
    // Import/Export menu
    QMenu *menu = new QMenu(this);
    QAction *importSub = new QAction(QIcon::fromTheme(QStringLiteral("document-import")), i18nc("@action:inmenu", "Import Subtitle"), this);
    QAction *exportSub = new QAction(QIcon::fromTheme(QStringLiteral("document-export")), i18nc("@action:inmenu", "Export Subtitle"), this);
    menu->addAction(importSub);
    menu->addAction(exportSub);
    connect(importSub, &QAction::triggered, this, &ManageSubtitles::importSubtitle);
    connect(exportSub, &QAction::triggered, pCore->window(), &MainWindow::slotExportSubtitle);
    button_menu->setMenu(menu);
}

ManageSubtitles::~ManageSubtitles()
{
    QSignalBlocker bk(subtitlesList);
    delete subtitlesList;
}

void ManageSubtitles::parseList(int ix)
{
    QSignalBlocker bk(subtitlesList);
    subtitlesList->clear();
    QMap<std::pair<int, QString>, QString> currentSubs = m_model->getSubtitlesList();
    QMapIterator<std::pair<int, QString>, QString> i(currentSubs);
    while (i.hasNext()) {
        i.next();
        QStringList texts = {i.key().second, QString(), i.value()};
        QTreeWidgetItem *item = new QTreeWidgetItem(subtitlesList, {i.key().second, i.value()});
        item->setData(0, Qt::UserRole, i.key().first);
        new QTreeWidgetItem(item, {i.value()});
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if (ix > -1 && ix == i.key().first) {
            subtitlesList->setCurrentItem(item);
        }
    }
    button_delete->setEnabled(subtitlesList->topLevelItemCount() > 1);
}

void ManageSubtitles::updateSubtitle(QTreeWidgetItem *item, int column)
{
    if (column == 0) {
        // An item was renamed
        m_model->updateModelName(item->data(0, Qt::UserRole).toInt(), item->text(0));
        m_controller->refreshSubtitlesComboIndex();
    }
}

void ManageSubtitles::addSubtitle(const QString name)
{
    m_model->createNewSubtitle(name);
    parseList();
    // Makes last item active
    subtitlesList->setCurrentItem(subtitlesList->topLevelItem(subtitlesList->topLevelItemCount() - 1));
}

void ManageSubtitles::duplicateSubtitle()
{
    m_model->createNewSubtitle(QString(), subtitlesList->currentItem()->data(0, Qt::UserRole).toInt());
    parseList();
    // Makes last item active
    subtitlesList->setCurrentItem(subtitlesList->topLevelItem(subtitlesList->topLevelItemCount() - 1));
}

void ManageSubtitles::deleteSubtitle()
{
    const QString name = subtitlesList->currentItem()->text(0);
    const QString path = subtitlesList->currentItem()->text(1);
    if (KMessageBox::warningContinueCancel(
            this, i18n("This will delete all subtitle entries in <b>%1</b>,<br/>as well as the subtitle file: %2 ", name, path)) != KMessageBox::Continue) {
        return;
    }
    qDebug() << ":::: DELETING SUBTITLE WIT TREE INDEX: " << subtitlesList->indexOfTopLevelItem(subtitlesList->currentItem());
    int ix = subtitlesList->indexOfTopLevelItem(subtitlesList->currentItem());
    // Makes last item active
    int nextId = -1;
    int id = subtitlesList->currentItem()->data(0, Qt::UserRole).toInt();
    if (ix == 0) {
        nextId = subtitlesList->topLevelItem(1)->data(0, Qt::UserRole).toInt();
    } else {
        nextId = subtitlesList->topLevelItem(ix - 1)->data(0, Qt::UserRole).toInt();
    }
    if (m_model->deleteSubtitle(id)) {
        parseList(nextId);
        int ix = subtitlesList->indexOfTopLevelItem(subtitlesList->currentItem());
        if (ix > -1) {
            m_controller->subtitlesMenuActivated(ix);
        }
    } else {
        subtitlesList->setCurrentItem(subtitlesList->topLevelItem(qMax(0, ix)));
    }
}

void ManageSubtitles::importSubtitle()
{
    QScopedPointer<ImportSubtitle> d(new ImportSubtitle(QString(), this));
    d->create_track->setChecked(true);
    if (d->exec() == QDialog::Accepted && !d->subtitle_url->url().isEmpty()) {
        if (d->create_track->isChecked()) {
            // Create a new subtitle entry
            addSubtitle(d->track_name->text());
        }
        int offset = 0, startFramerate = 30.00, targetFramerate = 30.00;
        if (d->cursor_pos->isChecked()) {
            offset = pCore->getMonitorPosition();
        }
        if (d->transform_framerate_check_box->isChecked()) {
            startFramerate = d->caption_original_framerate->value();
            targetFramerate = d->caption_target_framerate->value();
        }
        m_model->importSubtitle(d->subtitle_url->url().toLocalFile(), offset, true, startFramerate, targetFramerate, d->codecs_list->currentText().toUtf8());
    }
}
