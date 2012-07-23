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
#include <kcapacitybar.h>
#include <KUrl>

#include <QWizardPage>

class DvdWizardVob : public QWizardPage
{
    Q_OBJECT

public:
    explicit DvdWizardVob(const QString &profile, QWidget * parent = 0);
    virtual ~DvdWizardVob();
    virtual bool isComplete() const;
    QStringList selectedUrls() const;
    void setUrl(const QString &url);
    QString introMovie() const;
    bool isPal() const;
    bool isWide() const;
    int duration(int ix) const;
    QStringList durations() const;
    QStringList chapters() const;
    void setProfile(const QString& profile);
    void clear();
    void updateChapters(QMap <QString, QString> chaptersdata);
    void setIntroMovie(const QString& path);

private:
    Ui::DvdWizardVob_UI m_view;
    QString m_errorMessage;
    KCapacityBar *m_capacityBar;

public slots:
    void slotAddVobFile(KUrl url = KUrl(), const QString &chapters = QString());

private slots:
    void slotCheckVobList();
    void slotDeleteVobFile();
    void slotItemUp();
    void slotItemDown();
    void changeFormat();
};

#endif

