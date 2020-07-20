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

#include <QVBoxLayout>
#include <QWizard>
#include <QWizardPage>

#include "ui_wizardcapture_ui.h"
#include "ui_wizardcheck_ui.h"
#include "ui_wizardextra_ui.h"
#include "ui_wizardmltcheck_ui.h"
#include "ui_wizardstandard_ui.h"

class KMessageWidget;

class MyWizardPage : public QWizardPage
{
public:
    explicit MyWizardPage(QWidget *parent = nullptr);
    void setComplete(bool complete);
    bool isComplete() const override;
    bool m_isComplete{false};
};

class Wizard : public QWizard
{
    Q_OBJECT
public:
    explicit Wizard(bool autoClose, bool appImageCheck, QWidget *parent = nullptr);
    void installExtraMimes(const QString &baseName, const QStringList &globs);
    void runUpdateMimeDatabase();
    void adjustSettings();
    bool isOk() const;
    static void testHwEncoders();
    static void slotCheckPrograms(QString &infos, QString &warnings);

private:
    Ui::WizardStandard_UI m_standard;
    Ui::WizardExtra_UI m_extra;
    Ui::WizardMltCheck_UI m_mltCheck;
    Ui::WizardCapture_UI m_capture;
    Ui::WizardCheck_UI m_check;
    QVBoxLayout *m_startLayout;
    MyWizardPage *m_page;
    KMessageWidget *m_errorWidget;
    bool m_systemCheckIsOk;
    bool m_brokenModule;
    QString m_errors;
    QString m_warnings;
    QString m_infos;
    QMap<QString, QString> m_dvProfiles;
    QMap<QString, QString> m_hdvProfiles;
    QMap<QString, QString> m_otherProfiles;
    void checkMltComponents();
    void checkMissingCodecs();
    void updateHwStatus();

private slots:
    void slotCheckStandard();
    void slotCheckSelectedItem();
    void slotCheckMlt();
    void slotDetectWebcam();
    void slotUpdateCaptureParameters();
    void slotSaveCaptureFormat();
    void slotUpdateDecklinkDevice(uint captureCard);
    void slotOpenManual();
};

#endif
