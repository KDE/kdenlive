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


#ifndef DVDWIZARDVOB_H
#define DVDWIZARDVOB_H

#include "ui_dvdwizardvob_ui.h"

#include <kdeversion.h>

#if KDE_IS_VERSION(4,2,0)
#include <kcapacitybar.h>
#endif

#include <KUrl>

#include <QWizardPage>

class DvdWizardVob : public QWizardPage
{
    Q_OBJECT

public:
    DvdWizardVob(const QString &profile, QWidget * parent = 0);
    virtual ~DvdWizardVob();
    virtual bool isComplete() const;
    QStringList selectedUrls() const;
    QStringList selectedTitles() const;
    QStringList selectedTargets() const;
    void setUrl(const QString &url);
    QString introMovie() const;
    bool useChapters() const;
    bool isPal() const;
    QStringList chapter(int ix) const;

private:
    Ui::DvdWizardVob_UI m_view;
    QString m_errorMessage;

#if KDE_IS_VERSION(4,2,0)
    KCapacityBar *m_capacityBar;
#endif

private slots:
    void slotCheckVobList();
    void slotAddVobFile(KUrl url = KUrl());
    void slotDeleteVobFile();
    void slotItemUp();
    void slotItemDown();
    void changeFormat();
};

#endif

