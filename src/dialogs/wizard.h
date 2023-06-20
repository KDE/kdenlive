/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QVBoxLayout>
#include <QWizard>
#include <QWizardPage>

class KMessageWidget;
class QTemporaryFile;

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
    explicit Wizard(bool autoClose, QWidget *parent = nullptr);
    void installExtraMimes(const QString &baseName, const QStringList &globs);
    void runUpdateMimeDatabase();
    void adjustSettings();
    bool isOk() const;
    bool checkHwEncoder(const QString &name, const QStringList &args, const QTemporaryFile &file);
    void testHwEncoders();
    static void slotCheckPrograms(QString &infos, QString &warnings, QString &errors);
    static QStringList codecs();
    static const QString getHWCodecFriendlyName();

private:
    QVBoxLayout *m_startLayout;
    MyWizardPage *m_page;
    KMessageWidget *m_errorWidget;
    bool m_systemCheckIsOk;
    bool m_brokenModule;
    QString m_errors;
    QString m_warnings;
    QString m_infos;
    void checkMltComponents();
    void checkMissingCodecs();
    void updateHwStatus();

private Q_SLOTS:
    void slotCheckMlt();
    void slotOpenManual();
};
