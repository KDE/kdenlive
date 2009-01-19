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


#ifndef DVDWIZARD_H
#define DVDWIZARD_H

#include <QWizard>
#include <QVBoxLayout>
#include <QItemDelegate>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QProcess>


#include <KDebug>
#include <KTemporaryFile>

#include "ui_dvdwizardvob_ui.h"
#include "ui_dvdwizardmenu_ui.h"
#include "ui_dvdwizardiso_ui.h"
#include "ui_dvdwizardstatus_ui.h"

class DvdWizard : public QWizard {
    Q_OBJECT
public:
    DvdWizard(const QString &url = QString(), const QString &profile = "dv_pal", QWidget * parent = 0);
    virtual ~DvdWizard();

private:
    Ui::DvdWizardVob_UI m_vob;
    Ui::DvdWizardMenu_UI m_menu;
    Ui::DvdWizardIso_UI m_iso;
    Ui::DvdWizardStatus_UI m_status;

    QString m_profile;
    QGraphicsScene *m_scene;
    QGraphicsTextItem *m_button;
    QGraphicsPixmapItem *m_background;
    QGraphicsRectItem *m_color;
    QGraphicsRectItem *m_safeRect;
    int m_width;
    int m_height;
    KTemporaryFile m_menuFile;
    KTemporaryFile m_authorFile;

private slots:
    void slotCheckVobList(const QString &text);
    void buildButton();
    void buildColor();
    void buildImage();
    void checkBackground();
    void slotPageChanged(int page);
    void slotRenderFinished(int exitCode, QProcess::ExitStatus status);
    void slotIsoFinished(int exitCode, QProcess::ExitStatus status);
    void generateDvd();
};

#endif

