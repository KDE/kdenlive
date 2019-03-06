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

#ifndef DVDWIZARDCHAPTERS_H
#define DVDWIZARDCHAPTERS_H

#include "dvdwizardvob.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "ui_dvdwizardchapters_ui.h"

#include <QWizardPage>

class DvdWizardChapters : public QWizardPage
{
    Q_OBJECT

public:
    explicit DvdWizardChapters(MonitorManager *manager, DVDFORMAT format, QWidget *parent = nullptr);
    ~DvdWizardChapters() override;
    void changeProfile(DVDFORMAT format);
    void setPal(bool isPal);
    void setVobFiles(DVDFORMAT format, const QStringList &movies, const QStringList &durations, const QStringList &chapters);
    QStringList selectedTitles() const;
    QStringList selectedTargets() const;
    QStringList chapters(int ix) const;
    QDomElement toXml() const;
    QMap<QString, QString> chaptersData() const;
    void stopMonitor();
    void createMonitor(DVDFORMAT format);

private:
    Ui::DvdWizardChapters_UI m_view;
    DVDFORMAT m_format;
    Monitor *m_monitor;
    MonitorManager *m_manager;
    Timecode m_tc;
    void updateMonitorMarkers();

private slots:
    void slotUpdateChaptersList();
    void slotAddChapter();
    void slotRemoveChapter();
    void slotGoToChapter();
    void slotEnableChapters(int state);
};

#endif
