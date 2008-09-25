/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef WIZARD_H
#define WIZARD_H

#include <QWizard>
#include <QVBoxLayout>
#include <KDebug>

#include "ui_wizardstandard_ui.h"
#include "ui_wizardextra_ui.h"


class Wizard : public QWizard {
    Q_OBJECT
public:
    Wizard(QWidget * parent = 0);
    void installExtraMimes(QString baseName, QStringList globs);
    void runUpdateMimeDatabase();
    void adjustSettings();
    bool isOk() const;

private:
    Ui::WizardStandard_UI m_standard;
    Ui::WizardExtra_UI m_extra;
    QVBoxLayout *m_startLayout;
    bool m_systemCheckIsOk;

private slots:
    void slotCheckThumbs();
    void slotCheckStandard();
    void slotCheckSelectedItem();
    void slotCheckMlt();
};

#endif

