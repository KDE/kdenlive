/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "managesubtitles.h"
#include "bin/model/subtitlemodel.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include "timeline2/view/timelinecontroller.h"
#include <KMessageBox>
#include <QFontDatabase>

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
    connect(button_new, &QPushButton::clicked, this, &ManageSubtitles::addSubtitle);
    connect(button_duplicate, &QPushButton::clicked, this, &ManageSubtitles::duplicateSubtitle);
    connect(button_delete, &QPushButton::clicked, this, &ManageSubtitles::deleteSubtitle);
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
        m_controller->subtitlesListChanged();
        m_controller->refreshSubtitlesComboIndex();
    }
}

void ManageSubtitles::addSubtitle()
{
    m_model->createNewSubtitle();
    m_controller->subtitlesListChanged();
    parseList();
    // Makes last item active
    subtitlesList->setCurrentItem(subtitlesList->topLevelItem(subtitlesList->topLevelItemCount() - 1));
}

void ManageSubtitles::duplicateSubtitle()
{
    m_model->createNewSubtitle(subtitlesList->currentItem()->data(0, Qt::UserRole).toInt());
    m_controller->subtitlesListChanged();
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
        m_controller->subtitlesListChanged();
        parseList(nextId);
    } else {
        subtitlesList->setCurrentItem(subtitlesList->topLevelItem(qMax(0, ix)));
    }
}
