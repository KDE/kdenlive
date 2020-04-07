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

DvdWizardChapters::DvdWizardChapters(MonitorManager *manager, DVDFORMAT format, QWidget *parent)
    : QWizardPage(parent)
    , m_format(format)
    , m_monitor(nullptr)
    , m_manager(manager)

{
    m_view.setupUi(this);
    connect(m_view.vob_list, static_cast<void (KComboBox::*)(int)>(&KComboBox::currentIndexChanged), this, &DvdWizardChapters::slotUpdateChaptersList);
    connect(m_view.button_add, &QAbstractButton::clicked, this, &DvdWizardChapters::slotAddChapter);
    connect(m_view.button_delete, &QAbstractButton::clicked, this, &DvdWizardChapters::slotRemoveChapter);
    connect(m_view.chapters_list, &QListWidget::itemSelectionChanged, this, &DvdWizardChapters::slotGoToChapter);
    connect(m_view.chapters_box, &QCheckBox::stateChanged, this, &DvdWizardChapters::slotEnableChapters);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    // Build monitor for chapters
    auto *vbox = new QVBoxLayout;
    m_view.video_frame->setLayout(vbox);

    if (m_format == PAL || m_format == PAL_WIDE) {
        m_tc.setFormat(25);
    } else {
        m_tc.setFormat(30000.0 / 1001);
    }
}

DvdWizardChapters::~DvdWizardChapters()
{
    if (m_monitor) {
        m_monitor->stop();
        m_manager->removeMonitor(m_monitor);
        delete m_monitor;
    }
}

void DvdWizardChapters::stopMonitor()
{
    if (m_monitor) {
        m_monitor->pause();
    }
}

void DvdWizardChapters::slotUpdateChaptersList()
{
    m_monitor->show();
    m_manager->activateMonitor(Kdenlive::DvdMonitor);
    m_monitor->start();
    m_monitor->slotOpenDvdFile(m_view.vob_list->currentText());
    m_monitor->adjustRulerSize(m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole).toInt());
    QStringList currentChaps = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 1).toStringList();
    // insert chapters
    QStringList chaptersString;
    for (int i = 0; i < currentChaps.count(); ++i) {
        chaptersString.append(Timecode::getStringTimecode(currentChaps.at(i).toInt(), m_tc.fps(), true));
    }
    m_view.chapters_list->clear();
    m_view.chapters_list->addItems(chaptersString);
    updateMonitorMarkers();
    m_monitor->refreshMonitorIfActive();
    // bool modified = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 2).toInt();
}

void DvdWizardChapters::slotAddChapter()
{
    int pos = m_monitor->position();
    QStringList currentChaps = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 1).toStringList();
    if (currentChaps.contains(QString::number(pos))) {
        return;
    }
    currentChaps.append(QString::number(pos));

    QList<int> chapterTimes;
    chapterTimes.reserve(currentChaps.count());
    for (int i = 0; i < currentChaps.count(); ++i) {
        chapterTimes.append(currentChaps.at(i).toInt());
    }
    std::sort(chapterTimes.begin(), chapterTimes.end());

    // rebuild chapters
    QStringList chaptersString;
    currentChaps.clear();
    for (int i = 0; i < chapterTimes.count(); ++i) {
        chaptersString.append(Timecode::getStringTimecode(chapterTimes.at(i), m_tc.fps(), true));
        currentChaps.append(QString::number(chapterTimes.at(i)));
    }
    // Save item chapters
    m_view.vob_list->setItemData(m_view.vob_list->currentIndex(), currentChaps, Qt::UserRole + 1);
    // Mark item as modified
    m_view.vob_list->setItemData(m_view.vob_list->currentIndex(), 1, Qt::UserRole + 2);
    m_view.chapters_list->clear();
    m_view.chapters_list->addItems(chaptersString);
    updateMonitorMarkers();
}

void DvdWizardChapters::updateMonitorMarkers()
{
    const QStringList chapters = m_view.vob_list->itemData(m_view.vob_list->currentIndex(), Qt::UserRole + 1).toStringList();
    QList<CommentedTime> markers;
    markers.reserve(chapters.count());
    for (const QString &frame : chapters) {
        markers << CommentedTime(GenTime(frame.toInt(), m_tc.fps()), QString());
    }
    // TODO: reimplement makers as model
    // m_monitor->setMarkers(markers);
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
    for (int i = 0; i < currentChaps.count(); ++i) {
        chaptersString.append(Timecode::getStringTimecode(currentChaps.at(i).toInt(), m_tc.fps(), true));
    }
    m_view.chapters_list->clear();
    m_view.chapters_list->addItems(chaptersString);
    updateMonitorMarkers();
}

void DvdWizardChapters::slotGoToChapter()
{
    if (m_view.chapters_list->currentItem()) {
        m_monitor->setTimePos(m_tc.reformatSeparators(m_view.chapters_list->currentItem()->text()));
    }
}

void DvdWizardChapters::createMonitor(DVDFORMAT format)
{
    if (m_monitor == nullptr) {
        // TODO: allow monitor with different profile for DVD
        QString profile = DvdWizardVob::getDvdProfile(format);
        m_monitor = new Monitor(Kdenlive::DvdMonitor, m_manager, this);
        m_monitor->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        m_view.video_frame->layout()->addWidget(m_monitor);
        m_manager->appendMonitor(m_monitor);
        m_monitor->setCustomProfile(profile, m_tc);
        m_manager->activateMonitor(Kdenlive::DvdMonitor);
        m_monitor->start();
    }
}

void DvdWizardChapters::setVobFiles(DVDFORMAT format, const QStringList &movies, const QStringList &durations, const QStringList &chapters)
{
    m_format = format;
    if (m_format == PAL || m_format == PAL_WIDE) {
        m_tc.setFormat(25);
    } else {
        m_tc.setFormat(30000.0 / 1001);
    }
    m_view.vob_list->blockSignals(true);
    m_view.vob_list->clear();
    for (int i = 0; i < movies.count(); ++i) {
        m_view.vob_list->addItem(movies.at(i), durations.at(i));
        m_view.vob_list->setItemData(i, chapters.at(i).split(QLatin1Char(';')), Qt::UserRole + 1);
    }
    m_view.vob_list->blockSignals(false);
    if (m_view.chapters_box->checkState() == Qt::Checked) {
        slotUpdateChaptersList();
    }
}

QMap<QString, QString> DvdWizardChapters::chaptersData() const
{
    QMap<QString, QString> result;
    int max = m_view.vob_list->count();
    for (int i = 0; i < max; ++i) {
        QString chapters = m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList().join(QLatin1Char(';'));
        result.insert(m_view.vob_list->itemText(i), chapters);
    }
    return result;
}

QStringList DvdWizardChapters::selectedTitles() const
{
    QStringList result;
    int max = m_view.vob_list->count();
    for (int i = 0; i < max; ++i) {
        result.append(m_view.vob_list->itemText(i));
        if (!m_view.chapters_box->isChecked()) {
            continue;
        }
        QStringList chapters = m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList();
        for (int j = 0; j < chapters.count(); ++j) {
            result.append(Timecode::getStringTimecode(chapters.at(j).toInt(), m_tc.fps(), true));
        }
    }
    return result;
}

QStringList DvdWizardChapters::chapters(int ix) const
{
    QStringList result;
    QStringList chapters = m_view.vob_list->itemData(ix, Qt::UserRole + 1).toStringList();
    for (int j = 0; j < chapters.count(); ++j) {
        result.append(Timecode::getStringTimecode(chapters.at(j).toInt(), m_tc.fps(), true));
    }
    return result;
}

QStringList DvdWizardChapters::selectedTargets() const
{
    QStringList result;
    int max = m_view.vob_list->count();
    for (int i = 0; i < max; ++i) {
        // rightJustified: fill with 0s to make menus with more than 9 buttons work (now up to 99 buttons possible)
        result.append(QStringLiteral("jump title ") + QString::number(i + 1).rightJustified(2, '0'));
        if (!m_view.chapters_box->isChecked()) {
            continue;
        }
        QStringList chapters = m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList();
        for (int j = 0; j < chapters.count(); ++j) {
            result.append(QStringLiteral("jump title ") + QString::number(i + 1).rightJustified(2, '0') + QStringLiteral(" chapter ") +
                          QString::number(j + 1).rightJustified(2, '0'));
        }
    }
    return result;
}

QDomElement DvdWizardChapters::toXml() const
{
    QDomDocument doc;
    QDomElement xml = doc.createElement(QStringLiteral("xml"));
    doc.appendChild(xml);
    if (!m_view.chapters_box->isChecked()) {
        return doc.documentElement();
    }
    for (int i = 0; i < m_view.vob_list->count(); ++i) {
        QDomElement vob = doc.createElement(QStringLiteral("vob"));
        vob.setAttribute(QStringLiteral("file"), m_view.vob_list->itemText(i));
        vob.setAttribute(QStringLiteral("chapters"), m_view.vob_list->itemData(i, Qt::UserRole + 1).toStringList().join(QLatin1Char(';')));
        xml.appendChild(vob);
    }
    return doc.documentElement();
}

void DvdWizardChapters::slotEnableChapters(int state)
{
    m_view.chapters_frame->setEnabled(state == Qt::Checked);
    if (state == Qt::Checked) {
        createMonitor(m_format);
        slotUpdateChaptersList();
    }
}
