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

DvdWizardChapters::DvdWizardChapters(DVDFORMAT format, QWidget *parent) :
        QWizardPage(parent),
        m_format(format),
        m_monitor(NULL)

{
    m_view.setupUi(this);
    connect(m_view.vob_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateChaptersList()));
    connect(m_view.button_add, SIGNAL(clicked()), this, SLOT(slotAddChapter()));
    connect(m_view.button_delete, SIGNAL(clicked()), this, SLOT(slotRemoveChapter()));
    connect(m_view.chapters_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotGoToChapter()));

    // Build monitor for chapters

    if (m_format == PAL || m_format == PAL_WIDE) m_tc.setFormat(25);
    else m_tc.setFormat(30000.0 / 1001);

    m_manager = new MonitorManager(this);
    m_manager->resetProfiles(m_tc);
    //m_view.monitor_frame->setVisible(false);
}

DvdWizardChapters::~DvdWizardChapters()
{
    if (m_monitor) {
        m_monitor->stop();
        delete m_monitor;
    }
    delete m_manager;
}

// virtual

bool DvdWizardChapters::isComplete() const
{
    return true;
}

void DvdWizardChapters::stopMonitor()
{
    if (m_monitor) m_monitor->stop();
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

    //bool modified = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 2).toInt();
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
}

void DvdWizardChapters::slotGoToChapter()
{
    if (m_view.chapters_list->currentItem()) m_monitor->setTimePos(m_tc.reformatSeparators(m_view.chapters_list->currentItem()->text() + ":00"));
}

void DvdWizardChapters::setVobFiles(DVDFORMAT format, const QStringList &movies, const QStringList &durations, const QStringList &chapters)
{
    m_format = format;
    QString profile = DvdWizardVob::getDvdProfile(format);
    if (m_format == PAL || m_format == PAL_WIDE) {
        m_tc.setFormat(25);
    } else {
        m_tc.setFormat(30000.0 / 1001);
    }
    m_manager->resetProfiles(m_tc);
    if (m_monitor == NULL) {
        m_monitor = new Monitor(Kdenlive::dvdMonitor, m_manager, profile, this);
        //m_monitor->start();
        QVBoxLayout *vbox = new QVBoxLayout;
        vbox->addWidget(m_monitor);
        m_view.monitor_frame->setLayout(vbox);
        /*updateGeometry();
        m_view.monitor_frame->adjustSize();*/
    } else m_monitor->resetProfile(profile);

    m_view.vob_list->blockSignals(true);
    m_view.vob_list->clear();
    for (int i = 0; i < movies.count(); i++) {
        m_view.vob_list->addItem(movies.at(i), durations.at(i));
        m_view.vob_list->setItemData(i, chapters.at(i).split(';'), Qt::UserRole + 1);
    }
    m_view.vob_list->blockSignals(false);
    slotUpdateChaptersList();
}

QMap <QString, QString> DvdWizardChapters::chaptersData() const
{
    QMap <QString, QString> result;
    int max = m_view.vob_list->count();
    for (int i = 0; i < max; i++) {
        QString chapters = m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList().join(";");
        result.insert(m_view.vob_list->itemText(i), chapters);
    }
    return result;
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
        // rightJustified: fill with 0s to make menus with more than 9 buttons work (now up to 99 buttons possible)
        result.append("jump title " + QString::number(i + 1).rightJustified(2, '0'));
        QStringList chapters = m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList();
        for (int j = 0; j < chapters.count(); j++) {
            result.append("jump title " + QString::number(i + 1).rightJustified(2, '0') + " chapter " + QString::number(j + 1).rightJustified(2, '0'));
        }
    }
    return result;
}


QDomElement DvdWizardChapters::toXml() const
{
    QDomDocument doc;
    QDomElement xml = doc.createElement("xml");
    doc.appendChild(xml);
    for (int i = 0; i < m_view.vob_list->count(); i++) {
        QDomElement vob = doc.createElement("vob");
        vob.setAttribute("file", m_view.vob_list->itemText(i));
        vob.setAttribute("chapters", m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList().join(";"));
        xml.appendChild(vob);
    }
    return doc.documentElement();
}
