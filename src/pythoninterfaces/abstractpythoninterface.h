/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#ifndef ABSTRACTPYTHONINTERFACE_H
#define ABSTRACTPYTHONINTERFACE_H

#include <KMessageWidget>
#include <QObject>
#include <QString>
#include <QMap>
#include <QPair>

class AbstractPythonInterface : public QObject
{
    Q_OBJECT
public:
    explicit AbstractPythonInterface(QObject *parent = nullptr);
    bool checkSetup();
    void checkDependencies();
    void checkVersions();
    void updateDependencies();
    QStringList missingDependencies(const QStringList &filter = {});
    QString runScript(const QString &scriptpath, QStringList args = {}, const QString &firstarg = {});
    QString pythonExec() { return m_pyExec; };

    friend class PythonDependencyMessage;

private:
    QString m_pyExec;
    QString m_pip3Exec;
    QMap<QString, QString> m_dependencies;
    QStringList m_missing;

    void installMissingDependencies();
    QString locateScript(const QString &script);
    QString runPackageScript(const QString &mode);

protected:
    QMap<QString, QString> *m_scripts;
    void addDependency(const QString &pipname, const QString &purpose);
    void addScript(const QString &script);
    virtual QString featureName() { return {}; };

signals:
    void setupError(const QString &message);
    void checkVersionsResult(const QStringList &versions);
    void dependenciesMissing(const QStringList &messages);
    void dependenciesAvailable();

};

class PythonDependencyMessage : public KMessageWidget {
    Q_OBJECT

public:
    PythonDependencyMessage(QWidget *parent, AbstractPythonInterface *interface);

private slots:
    void doShowMessage(const QString &message, KMessageWidget::MessageType messageType = KMessageWidget::Information);

private:
    AbstractPythonInterface * m_interface;
    QAction *m_installAction;
    bool m_updated;

};

#endif
