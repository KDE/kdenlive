/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "core.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include "monitor/monitormanager.h"
#include "mltconnection.h"
#include "profiles/profilerepository.hpp"
#include "mltcontroller/bincontroller.h"
#include "mltcontroller/producerqueue.h"
#include "bin/bin.h"
#include "library/librarywidget.h"
#include "kdenlive_debug.h"

#include <QCoreApplication>
#include <KMessageBox>
#include <QInputDialog>

#include <mlt++/MltRepository.h>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

Core *Core::m_self = nullptr;

Core::Core() :
    m_mainWindow(nullptr)
    , m_projectManager(nullptr)
    , m_monitorManager(nullptr)
    , m_binController(nullptr)
    , m_producerQueue(nullptr)
    , m_binWidget(nullptr)
    , m_library(nullptr)
{
    connect(qApp, &QCoreApplication::aboutToQuit, this, &QObject::deleteLater);
}

Core::~Core()
{
    m_monitorManager->stopActiveMonitor();
    delete m_producerQueue;
    delete m_binWidget;
    delete m_projectManager;
    delete m_binController;
    delete m_monitorManager;
    m_self = nullptr;
}

void Core::build(const QString &MltPath, const QUrl &Url, const QString &clipsToLoad)
{
    m_self = new Core();
    m_self->initLocale();

    qRegisterMetaType<audioShortVector> ("audioShortVector");
    qRegisterMetaType< QVector<double> > ("QVector<double>");
    qRegisterMetaType<MessageType> ("MessageType");
    qRegisterMetaType<stringMap> ("stringMap");
    qRegisterMetaType<audioByteArray> ("audioByteArray");
    qRegisterMetaType< QList<ItemInfo> > ("QList<ItemInfo>");
    qRegisterMetaType< QVector<int> > ();
    qRegisterMetaType<QDomElement> ("QDomElement");
    qRegisterMetaType<requestClipInfo> ("requestClipInfo");
    qRegisterMetaType<MltVideoProfile> ("MltVideoProfile");

    m_self->initialize(MltPath);
    m_self->m_mainWindow->init(MltPath, Url, clipsToLoad);
    if (qApp->isSessionRestored()) {
        //NOTE: we are restoring only one window, because Kdenlive only uses one MainWindow
        m_self->m_mainWindow->restore(1, false);
    }
    m_self->m_mainWindow->show();
}

void Core::initialize(const QString &mltPath)
{
    m_mltConnection = std::unique_ptr<MltConnection>(new MltConnection(mltPath));
    m_mainWindow = new MainWindow();

    //loads the profile from disk
    ProfileRepository::get()->refresh();

    //load default profile and ask user to select one if not found.
    m_profile = KdenliveSettings::default_profile();
    if (m_profile.isEmpty()) {
        m_profile = ProjectManager::getDefaultProjectFormat();
        KdenliveSettings::setDefault_profile(m_profile);
    }
    if (!ProfileRepository::get()->profileExists(m_profile)) {
        KMessageBox::sorry(m_mainWindow, i18n("The default profile of Kdenlive is not set or invalid, press OK to set it to a correct value."));

        //TODO this simple widget should be improved and probably use profileWidget
        //we get the list of profiles
        QVector<QPair<QString, QString>> all_profiles = ProfileRepository::get()->getAllProfiles();
        QStringList all_descriptions;
        for (const auto& profile : all_profiles) {
            all_descriptions << profile.first;
        }

        //ask the user
        bool ok;
        QString item = QInputDialog::getItem(m_mainWindow, i18n("Select Default Profile"),
                                             i18n("Profile:"), all_descriptions, 0, false, &ok);
        if (ok) {
            ok = false;
            for (const auto& profile : all_profiles) {
                if (profile.first == item) {
                    m_profile = profile.second;
                    ok = true;
                }
            }
        }
        if (!ok) {
            KMessageBox::error(m_mainWindow, i18n("The given profile is invalid. We default to the profile \"dv_pal\", but you can change this from Kdenlive's settings panel"));
            m_profile = "dv_pal";
        }
        KdenliveSettings::setDefault_profile(m_profile);
    }

    m_projectManager = new ProjectManager(this);
    m_binWidget = new Bin();
    m_binController = new BinController();
    m_library = new LibraryWidget(m_projectManager);
    connect(m_library, SIGNAL(addProjectClips(QList<QUrl>)), m_binWidget, SLOT(droppedUrls(QList<QUrl>)));
    connect(this, &Core::updateLibraryPath, m_library, &LibraryWidget::slotUpdateLibraryPath);
    connect(m_binWidget, SIGNAL(storeFolder(QString, QString, QString, QString)), m_binController, SLOT(slotStoreFolder(QString, QString, QString, QString)));
    connect(m_binController, SIGNAL(loadFolders(QMap<QString, QString>)), m_binWidget, SLOT(slotLoadFolders(QMap<QString, QString>)));
    connect(m_binController, &BinController::requestAudioThumb, m_binWidget, &Bin::slotCreateAudioThumb);
    connect(m_binController, &BinController::abortAudioThumbs, m_binWidget, &Bin::abortAudioThumbs);
    connect(m_binController, SIGNAL(loadThumb(QString, QImage, bool)), m_binWidget, SLOT(slotThumbnailReady(QString, QImage, bool)));
    m_monitorManager = new MonitorManager(this);
    // Producer queue, creating MLT::Producers on request
    m_producerQueue = new ProducerQueue(m_binController);
    connect(m_producerQueue, SIGNAL(gotFileProperties(requestClipInfo, ClipController *)), m_binWidget, SLOT(slotProducerReady(requestClipInfo, ClipController *)), Qt::DirectConnection);
    connect(m_producerQueue, &ProducerQueue::replyGetImage, m_binWidget, &Bin::slotThumbnailReady);
    connect(m_producerQueue, &ProducerQueue::removeInvalidClip, m_binWidget, &Bin::slotRemoveInvalidClip, Qt::DirectConnection);
    connect(m_producerQueue, SIGNAL(addClip(QString, QMap<QString, QString>)), m_binWidget, SLOT(slotAddUrl(QString, QMap<QString, QString>)));
    connect(m_binController, SIGNAL(createThumb(QDomElement, QString, int)), m_producerQueue, SLOT(getFileProperties(QDomElement, QString, int)));
    connect(m_binWidget, &Bin::producerReady, m_producerQueue, &ProducerQueue::slotProcessingDone, Qt::DirectConnection);

    //TODO
    /*connect(m_producerQueue, SIGNAL(removeInvalidProxy(QString,bool)), m_binWidget, SLOT(slotRemoveInvalidProxy(QString,bool)));*/

}

Core *Core::self()
{
    return m_self;
}

MainWindow *Core::window()
{
    return m_mainWindow;
}

ProjectManager *Core::projectManager()
{
    return m_projectManager;
}

MonitorManager *Core::monitorManager()
{
    return m_monitorManager;
}

BinController *Core::binController()
{
    return m_binController;
}

Bin *Core::bin()
{
    return m_binWidget;
}

ProducerQueue *Core::producerQueue()
{
    return m_producerQueue;
}

LibraryWidget *Core::library()
{
    return m_library;
}

void Core::initLocale()
{
    QLocale systemLocale = QLocale();
#ifndef Q_OS_MAC
    setlocale(LC_NUMERIC, nullptr);
#else
    setlocale(LC_NUMERIC_MASK, nullptr);
#endif

// localeconv()->decimal_point does not give reliable results on Windows
#ifndef Q_OS_WIN
    char *separator = localeconv()->decimal_point;
    if (QString::fromUtf8(separator) != QChar(systemLocale.decimalPoint())) {
        //qCDebug(KDENLIVE_LOG)<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
        // HACK: There is a locale conflict, so set locale to C
        // Make sure to override exported values or it won't work
        qputenv("LANG", "C");
#ifndef Q_OS_MAC
        setlocale(LC_NUMERIC, "C");
#else
        setlocale(LC_NUMERIC_MASK, "C");
#endif
        systemLocale = QLocale::c();
    }
#endif

    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(systemLocale);
}


std::unique_ptr<Mlt::Repository>& Core::getMltRepository()
{
    return m_mltConnection->getMltRepository();
}
