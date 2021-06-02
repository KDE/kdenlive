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

#include "proxytest.h"

#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"

#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QPushButton>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QWheelEvent>
#include <QProcess>
#include <QtConcurrent>

#include "klocalizedstring.h"
#include <kio/directorysizejob.h>

ProxyTest::ProxyTest(QWidget *parent)
    : QDialog(parent)
    ,m_closing(false)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Compare proxy profile"));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Test Proxy profiles"));
    infoWidget->hide();
    paramBox->setVisible(false);
    m_failedProfiles = new MyTreeWidgetItem (resultList, {i18n("Failing profiles")});
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
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, [this]() {
        infoWidget->setText(i18n("Starting process"));
        infoWidget->animatedShow();
        resultList->setCursor(Qt::BusyCursor);
        QtConcurrent::run(this, &ProxyTest::startTest);
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
        //m_failedProfiles->addChild(item);
    } else {
        MyTreeWidgetItem *item = new MyTreeWidgetItem(resultList, itemData);
        qDebug()<<"=== ADDING ANALYSIS: "<<itemData;
        item->setData(0, Qt::UserRole, params);
        item->setData(1, Qt::UserRole, itemData.at(1).toInt());
        QTime t = QTime(0,0).addMSecs(itemData.at(1).toInt());
        qDebug()<<"TIME: "<<itemData.at(1).toInt()<<" = "<<t.toString(QStringLiteral("mm:ss:z"));
        item->setText(1, t.toString(QStringLiteral("mm:ss:z")));
        item->setText(2, KIO::convertSize(itemData.at(2).toInt()));
        item->setData(2, Qt::UserRole, itemData.at(2).toInt());
        //resultList->addTopLevelItem(item);
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
    QMetaObject::invokeMethod(m_process.get(), "kill");
    // Wait until concurrent tread is finished
    QMutexLocker lk(&m_locker);
}

void ProxyTest::startTest()
{
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
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
        QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(const QString&,i18n("Cannot create temporary files")));
        return;
    }
    m_process.reset(new QProcess());
    QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(const QString&,i18n("Generating a 60 seconds test video %1", src.fileName())));
    QStringList source = {QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("testsrc=duration=60:size=1920x1080:rate=25"),QStringLiteral("-c:v"),QStringLiteral("libx264"),QStringLiteral("-pix_fmt"),QStringLiteral("yuv420p"),src.fileName()};
    m_process->start(KdenliveSettings::ffmpegpath(), source);
    m_process->waitForStarted();
    if (m_process->waitForFinished(-1)) {
    }

    while (k.hasNext()) {
        QMutexLocker lk(&m_locker);
        k.next();
        if (!k.key().isEmpty()) {
            QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(const QString&,i18n("Processing %1", k.key())));
            QString params = k.value().section(QLatin1Char(';'), 0, 0);
            QString extension = k.value().section(QLatin1Char(';'), 1, 1);
            // Testing vaapi support
            QTemporaryFile tmp(QDir::temp().absoluteFilePath(QString("XXXXXX.%1").arg(extension)));
            if (!tmp.open()) {
                // Something went wrong
                QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(const QString&,i18n("Cannot create temporary files")));
                return;
            }
            tmp.close();
            params.replace(QStringLiteral("%width"), QString::number(proxyResize));
            m_process.reset(new QProcess());
            QStringList parameters = {QStringLiteral("-hide_banner"), QStringLiteral("-y"), QStringLiteral("-stats"), QStringLiteral("-v"), QStringLiteral("error")};
            if (params.contains(QLatin1String("-i "))) {
                // we have some pre-filename parameters, filename will be inserted later
            } else {
                parameters << QStringLiteral("-i") << src.fileName();
            }
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            for (const QString &s : params.split(QLatin1Char(' '), QString::SkipEmptyParts)) {
#else
            for (const QString &s : params.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
#endif
                QString t = s.simplified();
                if (t != QLatin1String("-noautorotate")) {
                    parameters << t;
                    if (t == QLatin1String("-i")) {
                        parameters << src.fileName();
                    }
                }
            }
            parameters << tmp.fileName();
            qDebug()<<"==== STARTING TEST PROFILE : "<<k.key()<<" = "<<parameters;
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
                    results << QStringList({k.key(),QString::number(elapsed), QString::number(size)});
                }
            }
            if (!success) {
                if (m_closing) {
                    return;
                }
                qDebug()<<"==== PROFILE FAILED: "<<k.key()<<" !!!!!!!!!!!!";
                results << QStringList({k.key()});
            }
            QMetaObject::invokeMethod(this, "addAnalysis", Qt::QueuedConnection, Q_ARG(const QStringList&,results));
        }
        if (m_closing) {
            return;
        }
    }
    QMetaObject::invokeMethod(this, "showMessage", Qt::QueuedConnection, Q_ARG(const QString&,QString()));
}


