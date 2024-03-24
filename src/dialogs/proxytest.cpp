/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "proxytest.h"

#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"

#include "kdenlive_debug.h"
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QPushButton>
#include <QTreeWidget>
#include <QWheelEvent>
#include <QtConcurrent>

#include "klocalizedstring.h"
#include <kio/directorysizejob.h>

ProxyTest::ProxyTest(QWidget *parent)
    : QDialog(parent)
    , m_closing(false)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18nc("@title:window", "Compare Proxy Profile"));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Test Proxy Profiles"));
    infoWidget->hide();
    paramBox->setVisible(false);
    m_failedProfiles = new MyTreeWidgetItem(resultList, {i18n("Failing profiles")});
    resultList->addTopLevelItem(m_failedProfiles);
    m_failedProfiles->setExpanded(false);
    m_failedProfiles->setHidden(true);
    connect(resultList, &QTreeWidget::itemSelectionChanged, resultList, [&]() {
        QTreeWidgetItem *item = resultList->currentItem();
        if (item) {
            paramBox->setPlainText(item->data(0, Qt::UserRole).toString());
            paramBox->setVisible(true);
        } else {
            paramBox->clear();
            paramBox->setVisible(false);
        }
    });
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        infoWidget->setText(i18n("Starting process"));
        infoWidget->animatedShow();
        resultList->setCursor(Qt::BusyCursor);
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QtConcurrent::run(this, &ProxyTest::startTest);
#else
        (void)QtConcurrent::run(&ProxyTest::startTest, this);
#endif
    });
}

void ProxyTest::addAnalysis(const QStringList &data)
{
    QStringList itemData = data;
    QString params = itemData.takeFirst();
    if (itemData.size() == 1) {
        // This proxy profile failed
        MyTreeWidgetItem *item = new MyTreeWidgetItem(m_failedProfiles, itemData);
        item->setData(0, Qt::UserRole, params);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("window-close")));
        m_failedProfiles->setHidden(false);
        // m_failedProfiles->addChild(item);
    } else {
        MyTreeWidgetItem *item = new MyTreeWidgetItem(resultList, itemData);
        qDebug() << "=== ADDING ANALYSIS: " << itemData;
        item->setData(0, Qt::UserRole, params);
        item->setData(1, Qt::UserRole, itemData.at(1).toInt());
        QTime t = QTime(0, 0).addMSecs(itemData.at(1).toInt());
        qDebug() << "TIME: " << itemData.at(1).toInt() << " = " << t.toString(QStringLiteral("mm:ss:z"));
        item->setText(1, t.toString(QStringLiteral("mm:ss:z")));
        item->setText(2, KIO::convertSize(itemData.at(2).toInt()));
        item->setData(2, Qt::UserRole, itemData.at(2).toInt());
        // resultList->addTopLevelItem(item);
    }
}

void ProxyTest::showMessage(const QString &message)
{
    infoWidget->setText(message);
    if (!message.isEmpty()) {
        infoWidget->animatedShow();
    } else {
        // Tests finished
        infoWidget->animatedHide();
        resultList->setCursor(Qt::ArrowCursor);
    }
}

ProxyTest::~ProxyTest()
{
    m_closing = true;
    Q_EMIT jobCanceled();
    // Wait until concurrent tread is finished
    QMutexLocker lk(&m_locker);
}

void ProxyTest::startTest()
{
    // load proxy profiles
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "proxy");
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> k(values);
    int proxyResize = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyresize")).toInt();
    QElapsedTimer timer;
    QTemporaryFile src(QDir::temp().absoluteFilePath(QString("XXXXXX.mov")));
    if (!src.open()) {
        // Something went wrong
        QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot create temporary files")));
        return;
    }
    QMutexLocker lk1(&m_locker);
    m_process.reset(new QProcess());
    connect(this, &ProxyTest::jobCanceled, m_process.get(), &QProcess::kill, Qt::DirectConnection);
    QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Generating a 60 seconds test video %1", src.fileName())));
    QStringList source = {QStringLiteral("-y"),
                          QStringLiteral("-f"),
                          QStringLiteral("lavfi"),
                          QStringLiteral("-i"),
                          QStringLiteral("testsrc=duration=60:size=1920x1080:rate=25"),
                          QStringLiteral("-c:v"),
                          QStringLiteral("libx264"),
                          QStringLiteral("-pix_fmt"),
                          QStringLiteral("yuv420p"),
                          src.fileName()};
    m_process->start(KdenliveSettings::ffmpegpath(), source);
    m_process->waitForStarted();
    if (m_closing) {
        return;
    }
    m_process->waitForFinished(-1);
    if (m_closing) {
        return;
    }

    while (k.hasNext() && !m_closing) {
        k.next();
        if (!k.key().isEmpty()) {
            QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Processing %1", k.key())));
            QString params = k.value().section(QLatin1Char(';'), 0, 0);
            QString extension = k.value().section(QLatin1Char(';'), 1, 1);
            // Testing vaapi support
            QTemporaryFile tmp(QDir::temp().absoluteFilePath(QString("XXXXXX.%1").arg(extension)));
            if (!tmp.open()) {
                // Something went wrong
                QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot create temporary files")));
                return;
            }
            tmp.close();
            params.replace(QStringLiteral("%width"), QString::number(proxyResize));
            if (params.contains(QStringLiteral("%frameSize"))) {
                int w = proxyResize;
                int h = w * 1080 / 1920;
                params.replace(QStringLiteral("%frameSize"), QString("%1x%2").arg(w).arg(h));
            }
            params.replace(QStringLiteral("%nvcodec"), QStringLiteral("h264_cuvid"));
            m_process.reset(new QProcess());
            connect(this, &ProxyTest::jobCanceled, m_process.get(), &QProcess::kill, Qt::DirectConnection);
            QStringList parameters = {QStringLiteral("-hide_banner"), QStringLiteral("-y"), QStringLiteral("-stats"), QStringLiteral("-v"),
                                      QStringLiteral("error")};
            if (params.contains(QLatin1String("-i "))) {
                // we have some pre-filename parameters, filename will be inserted later
            } else {
                parameters << QStringLiteral("-i") << src.fileName();
            }
            QStringList paramList = params.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &s : qAsConst(paramList)) {
                QString t = s.simplified();
                if (t != QLatin1String("-noautorotate")) {
                    parameters << t;
                    if (t == QLatin1String("-i")) {
                        parameters << src.fileName();
                    }
                }
            }
            parameters << tmp.fileName();
            qDebug() << "==== STARTING TEST PROFILE : " << k.key() << " = " << parameters;
            m_process->start(KdenliveSettings::ffmpegpath(), parameters);
            m_process->waitForStarted();
            timer.start();
            bool success = false;
            QStringList results = {params};
            if (m_process->waitForFinished(-1)) {
                if (m_closing) {
                    return;
                }
                qint64 elapsed = timer.elapsed();
                qint64 size = tmp.size();
                if (size > 0) {
                    success = true;
                    results << QStringList({k.key(), QString::number(elapsed), QString::number(size)});
                }
            }
            if (!success) {
                if (m_closing) {
                    return;
                }
                qDebug() << "==== PROFILE FAILED: " << k.key() << " !!!!!!!!!!!!";
                results << QStringList({k.key()});
            }
            QMetaObject::invokeMethod(this, "addAnalysis", Qt::QueuedConnection, Q_ARG(QStringList, results));
        }
    }
    if (m_closing) {
        return;
    }
    QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(QString, QString()));
}
