/***************************************************************************
 *   SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   SPDX-License-Identifier: GPL-2.0-or-later
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
