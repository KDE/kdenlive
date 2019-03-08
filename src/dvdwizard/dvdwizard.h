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

#include "dvdwizard/dvdwizardchapters.h"
#include "dvdwizard/dvdwizardmenu.h"
#include "dvdwizard/dvdwizardvob.h"
#include "ui_dvdwizardchapters_ui.h"
#include "ui_dvdwizardstatus_ui.h"

#include <QProcess>
#include <QWizard>

#include <QTemporaryFile>

typedef QMap<QString, QRect> stringRectMap;

class DvdWizard : public QWizard
{
    Q_OBJECT
public:
    explicit DvdWizard(MonitorManager *manager, const QString &url = QString(), QWidget *parent = nullptr);
    ~DvdWizard() override;
    void processSpumux();

private:
    DvdWizardVob *m_pageVob;
    DvdWizardMenu *m_pageMenu;
    Ui::DvdWizardStatus_UI m_status;
    KMessageWidget *m_isoMessage;

    DvdWizardChapters *m_pageChapters;
    QTemporaryFile m_authorFile;
    QTemporaryFile m_menuFile;
    QTemporaryFile m_menuVobFile;
    QTemporaryFile m_letterboxMovie;
    QProcess *m_dvdauthor;
    QProcess *m_mkiso;
    QProcess m_menuJob;
    QString m_creationLog;
    QListWidgetItem *m_vobitem;
    QTemporaryFile m_selectedImage;
    QTemporaryFile m_selectedLetterImage;
    QTemporaryFile m_highlightedImage;
    QTemporaryFile m_highlightedLetterImage;
    QTemporaryFile m_menuVideo;
    QTemporaryFile m_menuFinalVideo;
    QTemporaryFile m_menuImageBackground;
    QMenu *m_burnMenu;
    int m_previousPage;
    void cleanup();
    void errorMessage(const QString &text);
    void infoMessage(const QString &text);
    void processDvdauthor(const QString &menuMovieUrl = QString(), const stringRectMap &buttons = stringRectMap(),
                          const QStringList &buttonsTarget = QStringList());

private slots:
    void slotPageChanged(int page);
    void slotRenderFinished(int exitCode, QProcess::ExitStatus status);
    void slotIsoFinished(int exitCode, QProcess::ExitStatus status);
    void generateDvd();
    void slotPreview();
    void slotBurn();
    void slotGenerate();
    void slotAbort();
    void slotLoad();
    void slotSave();
    void slotShowRenderInfo();
    void slotShowIsoInfo();
    void slotProcessMenuStatus(int, QProcess::ExitStatus status);
};

#endif
