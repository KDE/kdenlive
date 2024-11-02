/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "modeldownloadwidget.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <KLocalizedString>

#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QtConcurrent>

ModelDownloadWidget::ModelDownloadWidget(SpeechToText *engine, const QString &scriptPath, const QStringList &args, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_scriptPath(scriptPath)
    , m_args(args)
{
    auto *l = new QHBoxLayout;
    setLayout(l);
    m_tb = new QToolButton(this);
    m_tb->setAutoRaise(true);
    m_tb->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
    l->addWidget(m_tb);
    m_pb = new QProgressBar(this);
    l->addWidget(m_pb);
    m_pb->setRange(0, 0);
    connect(this, &ModelDownloadWidget::jobSuccess, this, [this]() { m_tb->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply"))); });
}

ModelDownloadWidget::~ModelDownloadWidget()
{
    Q_EMIT abortScript();
}

void ModelDownloadWidget::startDownload()
{
    m_pb->setVisible(true);
    (void)QtConcurrent::run(&ModelDownloadWidget::processDownload, this);
}

void ModelDownloadWidget::processDownload()
{
    m_args.prepend(m_scriptPath);
    QProcess scriptJob;

    connect(this, &ModelDownloadWidget::abortScript, &scriptJob, &QProcess::kill, Qt::DirectConnection);
    scriptJob.setProcessChannelMode(QProcess::MergedChannels);
    connect(&scriptJob, &QProcess::readyReadStandardOutput, [this, &scriptJob]() {
        const QString processData = QString::fromUtf8(scriptJob.readAllStandardOutput());
        Q_EMIT installFeedback(processData);
        if (processData.contains(QLatin1Char('%'))) {
            bool ok;
            int progress = processData.section(QLatin1Char('%'), 0, 0).section(QLatin1Char(' '), -1).simplified().toInt(&ok);
            if (ok && progress != m_downloadProgress) {
                if (m_downloadProgress == -1) {
                    QMetaObject::invokeMethod(m_pb, "setRange", Q_ARG(int, 0), Q_ARG(int, 100));
                }
                // Display download progress
                m_downloadProgress = progress;
                QMetaObject::invokeMethod(m_pb, "setValue", Q_ARG(int, m_downloadProgress));
            }
        }
    });
    scriptJob.start(KdenliveSettings::pythonPath(), m_args);
    // Don't timeout
    scriptJob.waitForFinished(-1);
    QMetaObject::invokeMethod(m_pb, "setVisible", Q_ARG(bool, false));
    if (scriptJob.exitStatus() != QProcess::NormalExit || scriptJob.exitCode() != 0) {
        const QString errorMessage = scriptJob.readAllStandardError();
        Q_EMIT installFeedback(i18n("Error downloading model:\n %1\n%2", m_scriptPath, errorMessage));
        Q_EMIT jobDone(false);
        return;
    }
    Q_EMIT jobSuccess(m_scriptPath);
    Q_EMIT jobDone(true);
}
