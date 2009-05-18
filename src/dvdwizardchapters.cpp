/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "dvdwizardchapters.h"

#include <KDebug>

#include <QFile>

DvdWizardChapters::DvdWizardChapters(bool isPal, QWidget *parent) :
        QWizardPage(parent),
        m_isPal(isPal)

{
    m_view.setupUi(this);
    connect(m_view.vob_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateChaptersList()));
    connect(m_view.button_add, SIGNAL(clicked()), this, SLOT(slotAddChapter()));
    connect(m_view.button_delete, SIGNAL(clicked()), this, SLOT(slotRemoveChapter()));
    connect(m_view.button_save, SIGNAL(clicked()), this, SLOT(slotSaveChapter()));
    connect(m_view.chapters_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotGoToChapter()));

    // Build monitor for chapters

    if (m_isPal) m_tc.setFormat(25);
    else m_tc.setFormat(30, true);

    m_manager = new MonitorManager(this);
    m_manager->resetProfiles(m_tc);
    m_monitor = new Monitor("chapter", m_manager, this);
    m_monitor->start();

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(m_monitor);
    m_view.monitor_frame->setLayout(vbox);


}

DvdWizardChapters::~DvdWizardChapters()
{
    delete m_monitor;
    delete m_manager;
}

// virtual

bool DvdWizardChapters::isComplete() const
{
    return true;
}

void DvdWizardChapters::slotUpdateChaptersList()
{
    m_monitor->slotOpenFile(m_view.vob_list->currentText());
    m_monitor->adjustRulerSize(m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole).toInt());
    QStringList currentChaps = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 1).toStringList();

    // insert chapters
    QStringList chaptersString;
    for (int i = 0; i < currentChaps.count(); i++) {
        chaptersString.append(Timecode::getStringTimecode(currentChaps.at(i).toInt(), m_tc.fps()));
    }
    m_view.chapters_list->clear();
    m_view.chapters_list->addItems(chaptersString);

    bool modified = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 2).toInt();
    m_view.button_save->setEnabled(modified);
}

void DvdWizardChapters::slotAddChapter()
{
    int pos = m_monitor->position().frames(m_tc.fps());
    QStringList currentChaps = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 1).toStringList();
    if (currentChaps.contains(QString::number(pos))) return;
    else currentChaps.append(QString::number(pos));
    QList <int> chapterTimes;
    for (int i = 0; i < currentChaps.count(); i++)
        chapterTimes.append(currentChaps.at(i).toInt());
    qSort(chapterTimes);

    // rebuild chapters
    QStringList chaptersString;
    currentChaps.clear();
    for (int i = 0; i < chapterTimes.count(); i++) {
        chaptersString.append(Timecode::getStringTimecode(chapterTimes.at(i), m_tc.fps()));
        currentChaps.append(QString::number(chapterTimes.at(i)));
    }
    // Save item chapters
    m_view.vob_list->setItemData(m_view.vob_list->currentIndex(), currentChaps, Qt::UserRole + 1);
    // Mark item as modified
    m_view.vob_list->setItemData(m_view.vob_list->currentIndex(), 1, Qt::UserRole + 2);
    m_view.chapters_list->clear();
    m_view.chapters_list->addItems(chaptersString);
    m_view.button_save->setEnabled(true);
}

void DvdWizardChapters::slotRemoveChapter()
{
    int ix = m_view.chapters_list->currentRow();
    QStringList currentChaps = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 1).toStringList();
    currentChaps.removeAt(ix);

    // Save item chapters
    m_view.vob_list->setItemData(m_view.vob_list->currentIndex(), currentChaps, Qt::UserRole + 1);
    // Mark item as modified
    m_view.vob_list->setItemData(m_view.vob_list->currentIndex(), 1, Qt::UserRole + 2);

    // rebuild chapters
    QStringList chaptersString;
    for (int i = 0; i < currentChaps.count(); i++) {
        chaptersString.append(Timecode::getStringTimecode(currentChaps.at(i).toInt(), m_tc.fps()));
    }
    m_view.chapters_list->clear();
    m_view.chapters_list->addItems(chaptersString);
    m_view.button_save->setEnabled(true);
}

void DvdWizardChapters::slotGoToChapter()
{
    m_monitor->setTimePos(m_view.chapters_list->currentItem()->text() + ":00");
}

void DvdWizardChapters::slotGetChaptersList(int ix)
{
    QString url = m_view.vob_list->itemText(ix);
    if (QFile::exists(url + ".dvdchapter")) {
        // insert chapters as children
        QFile file(url + ".dvdchapter");
        if (file.open(QIODevice::ReadOnly)) {
            QDomDocument doc;
            doc.setContent(&file);
            file.close();
            QDomNodeList chapters = doc.elementsByTagName("chapter");
            QStringList chaptersList;
            for (int j = 0; j < chapters.count(); j++) {
                chaptersList.append(QString::number(chapters.at(j).toElement().attribute("time").toInt()));
            }
            m_view.vob_list->setItemData(ix, chaptersList, Qt::UserRole + 1);
        }
    }
}

void DvdWizardChapters::setVobFiles(bool isPal, const QStringList movies, const QStringList durations)
{
    m_isPal = isPal;
    if (m_isPal) m_tc.setFormat(25);
    else m_tc.setFormat(30, true);
    m_manager->resetProfiles(m_tc);
    m_monitor->resetProfile();

    if (m_view.vob_list->count() == movies.count()) {
        bool equal = true;
        for (int i = 0; i < movies.count(); i++) {
            if (movies.at(i) != m_view.vob_list->itemText(i)) {
                equal = false;
                break;
            }
        }
        if (equal) return;
    }
    m_view.vob_list->clear();
    for (int i = 0; i < movies.count(); i++) {
        m_view.vob_list->addItem(movies.at(i), durations.at(i));
        slotGetChaptersList(i);
    }
    slotUpdateChaptersList();
}

QStringList DvdWizardChapters::selectedTitles() const
{
    QStringList result;
    int max = m_view.vob_list->count();
    for (int i = 0; i < max; i++) {
        result.append(m_view.vob_list->itemText(i));
        QStringList chapters = m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList();
        for (int j = 0; j < chapters.count(); j++) {
            result.append(Timecode::getStringTimecode(chapters.at(j).toInt(), m_tc.fps()));
        }
    }
    return result;
}

QStringList DvdWizardChapters::chapters(int ix) const
{
    QStringList result;
    QStringList chapters = m_view.vob_list->itemData(ix, Qt::UserRole + 1).toStringList();
    for (int j = 0; j < chapters.count(); j++) {
        result.append(Timecode::getStringTimecode(chapters.at(j).toInt(), m_tc.fps()));
    }
    return result;
}

QStringList DvdWizardChapters::selectedTargets() const
{
    QStringList result;
    int max = m_view.vob_list->count();
    for (int i = 0; i < max; i++) {
        result.append("jump title " + QString::number(i + 1));
        QStringList chapters = m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList();
        for (int j = 0; j < chapters.count(); j++) {
            result.append("jump title " + QString::number(i + 1) + " chapter " + QString::number(j + 1));
        }
    }
    return result;
}

void DvdWizardChapters::slotSaveChapter()
{
    QDomDocument doc;
    QDomElement chapters = doc.createElement("chapters");
    chapters.setAttribute("fps", m_tc.fps());
    doc.appendChild(chapters);

    QStringList chaptersList = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 1).toStringList();

    for (int i = 0; i < chaptersList.count(); i++) {
        QDomElement chapter = doc.createElement("chapter");
        chapters.appendChild(chapter);
        chapter.setAttribute("title", i18n("Chapter %1", i));
        chapter.setAttribute("time", chaptersList.at(i));
    }
    // save chapters file
    QFile file(m_view.vob_list->currentText() + ".dvdchapter");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kWarning() << "//////  ERROR writing DVD CHAPTER file: " << m_view.vob_list->currentText() + ".dvdchapter";
    } else {
        file.write(doc.toString().toUtf8());
        if (file.error() != QFile::NoError)
            kWarning() << "//////  ERROR writing DVD CHAPTER file: " << m_view.vob_list->currentText() + ".dvdchapter";
        else {
            m_view.vob_list->setItemData(m_view.vob_list->currentIndex(), 0, Qt::UserRole + 2);
            m_view.button_save->setEnabled(false);
        }
        file.close();
    }
}
