/*
   SPDX-FileCopyrightText: 2019 (c) Matthieu Gallien <matthieu_gallien@yahoo.fr>
   SPDX-FileCopyrightText: 2025 (c) Jean-Baptiste Mardelle <jb@kdenlive.org>
   SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "powermanagementinterface.h"
#include "kdenlivesettings.h"
#include <KLocalizedString>

#ifndef NODBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

#if defined Q_OS_WIN
// clang-format off
#include <windows.h>
#include <winbase.h>
// clang-format on
#endif

#include <QString>

#include <QGuiApplication>

class PowerManagementInterfacePrivate
{
public:
    bool mPreventSleep = false;
    bool mPreventDim = false;

    bool mInhibitedSleep = false;
    bool mInhibitedDim = false;

    uint mInhibitSleepCookie = 0;
    uint mInhibitDimCookie = 0;
};

PowerManagementInterface::PowerManagementInterface(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<PowerManagementInterfacePrivate>())
{
    // Not needed for now
    /*
    #ifndef NODBUS
        auto sessionBus = QDBusConnection::sessionBus();

        sessionBus.connect(QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
                           QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("HasInhibitChanged"), this,
    SLOT(hostSleepInhibitChanged())); #endif
    */
}

PowerManagementInterface::~PowerManagementInterface() = default;

bool PowerManagementInterface::preventSleep() const
{
    return d->mPreventSleep;
}

bool PowerManagementInterface::preventDim() const
{
    return d->mPreventDim;
}

bool PowerManagementInterface::sleepInhibited() const
{
    return d->mInhibitedSleep;
}

bool PowerManagementInterface::dimInhibited() const
{
    return d->mInhibitedDim;
}

void PowerManagementInterface::setPreventSleep(bool value)
{
    if (d->mPreventSleep == value || !KdenliveSettings::usePowerManagement()) {
        return;
    }

    if (value) {
#if defined Q_OS_WIN
        inhibitSleepWindowsWorkspace();
#elif defined Q_OS_MAC
        // TODO?
#else
        inhibitSleepPlasmaWorkspace();
        inhibitSleepGnomeWorkspace();
#endif
        d->mPreventSleep = true;
    } else {
#if defined Q_OS_WIN
        uninhibitSleepWindowsWorkspace();
        if (d->mPreventDim) {
            inhibitDimWindowsWorkspace();
        }
#elif defined Q_OS_MAC
        // TODO?
#else
        uninhibitSleepPlasmaWorkspace();
        uninhibitSleepGnomeWorkspace();
#endif
        d->mPreventSleep = false;
    }

    Q_EMIT preventSleepChanged();
}

void PowerManagementInterface::setPreventDim(bool value)
{
    if (d->mPreventDim == value || !KdenliveSettings::usePowerManagement()) {
        return;
    }

    if (value) {
#if defined Q_OS_WIN
        inhibitDimWindowsWorkspace();
#elif defined Q_OS_MAC
        // TODO?
#else
        inhibitDim();
#endif
        d->mPreventDim = true;
    } else {
#if defined Q_OS_WIN
        uninhibitDimWindowsWorkspace();
        if (d->mPreventSleep) {
            inhibitSleepWindowsWorkspace();
        }
#elif defined Q_OS_MAC
        // TODO?
#else
        uninhibitDim();
#endif
        d->mPreventDim = false;
    }

    Q_EMIT preventDimChanged();
}

void PowerManagementInterface::retryInhibitingSleep()
{
    if (d->mPreventSleep && !d->mInhibitedSleep) {
        inhibitSleepPlasmaWorkspace();
        inhibitSleepGnomeWorkspace();
    }
}

void PowerManagementInterface::retryInhibitingDim()
{
    if (d->mPreventDim && !d->mInhibitedDim) {
        inhibitDim();
    }
}

void PowerManagementInterface::hostSleepInhibitChanged() {}

void PowerManagementInterface::inhibitDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher)
{
#ifndef NODBUS
    QDBusPendingReply<uint> reply = *aWatcher;
    if (reply.isError()) {
    } else {
        d->mInhibitSleepCookie = reply.argumentAt<0>();
        d->mInhibitedSleep = true;

        Q_EMIT sleepInhibitedChanged();
    }
    aWatcher->deleteLater();
#else
    Q_UNUSED(aWatcher)
#endif
}

void PowerManagementInterface::uninhibitDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher)
{
#ifndef NODBUS
    QDBusPendingReply<> reply = *aWatcher;
    if (reply.isError()) {
        qDebug() << "PowerManagementInterface::uninhibitDBusCallFinished" << reply.error();
    } else {
        d->mInhibitedSleep = false;

        Q_EMIT sleepInhibitedChanged();
    }
    aWatcher->deleteLater();
#else
    Q_UNUSED(aWatcher)
#endif
}

void PowerManagementInterface::inhibitDimDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher)
{
#ifndef NODBUS
    QDBusPendingReply<uint> reply = *aWatcher;
    if (reply.isError()) {
    } else {
        d->mInhibitDimCookie = reply.argumentAt<0>();
        d->mInhibitedDim = true;

        Q_EMIT dimInhibitedChanged();
    }
    aWatcher->deleteLater();
#else
    Q_UNUSED(aWatcher)
#endif
}

void PowerManagementInterface::uninhibitDimDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher)
{
#ifndef NODBUS
    QDBusPendingReply<> reply = *aWatcher;
    if (reply.isError()) {
        qDebug() << "PowerManagementInterface::uninhibitDBusCallFinished" << reply.error();
    } else {
        d->mInhibitedDim = false;

        Q_EMIT dimInhibitedChanged();
    }
    aWatcher->deleteLater();
#else
    Q_UNUSED(aWatcher)
#endif
}

void PowerManagementInterface::inhibitDBusCallFinishedGnomeWorkspace(QDBusPendingCallWatcher *aWatcher)
{
#ifndef NODBUS
    QDBusPendingReply<uint> reply = *aWatcher;
    if (reply.isError()) {
        qDebug() << "PowerManagementInterface::inhibitDBusCallFinishedGnomeWorkspace" << reply.error();
    } else {
        d->mInhibitSleepCookie = reply.argumentAt<0>();
        d->mInhibitedSleep = true;

        Q_EMIT sleepInhibitedChanged();
    }
    aWatcher->deleteLater();
#else
    Q_UNUSED(aWatcher)
#endif
}

void PowerManagementInterface::uninhibitDBusCallFinishedGnomeWorkspace(QDBusPendingCallWatcher *aWatcher)
{
#ifndef NODBUS
    QDBusPendingReply<> reply = *aWatcher;
    if (reply.isError()) {
        qDebug() << "PowerManagementInterface::uninhibitDBusCallFinished" << reply.error();
    } else {
        d->mInhibitedSleep = false;

        Q_EMIT sleepInhibitedChanged();
    }
    aWatcher->deleteLater();
#else
    Q_UNUSED(aWatcher)
#endif
}

void PowerManagementInterface::inhibitDim()
{
#ifndef NODBUS
    auto sessionBus = QDBusConnection::sessionBus();
    auto inhibitCall = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                                                      QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("Inhibit"));

    inhibitCall.setArguments({{QGuiApplication::desktopFileName()}, {i18nc("@info:status  explanation for sleep inhibit during playback", "Playing video")}});

    auto asyncReply = sessionBus.asyncCall(inhibitCall);

    auto replyWatcher = new QDBusPendingCallWatcher(asyncReply, this);

    QObject::connect(replyWatcher, &QDBusPendingCallWatcher::finished, this, &PowerManagementInterface::inhibitDimDBusCallFinishedPlasmaWorkspace);
#endif
}

void PowerManagementInterface::uninhibitDim()
{
#ifndef NODBUS
    auto sessionBus = QDBusConnection::sessionBus();

    auto uninhibitCall = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"),
                                                        QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("UnInhibit"));

    uninhibitCall.setArguments({{d->mInhibitDimCookie}});

    auto asyncReply = sessionBus.asyncCall(uninhibitCall);

    auto replyWatcher = new QDBusPendingCallWatcher(asyncReply, this);
    QObject::connect(replyWatcher, &QDBusPendingCallWatcher::finished, this, &PowerManagementInterface::uninhibitDimDBusCallFinishedPlasmaWorkspace);
#endif
}

void PowerManagementInterface::inhibitSleepPlasmaWorkspace()
{
#ifndef NODBUS
    auto sessionBus = QDBusConnection::sessionBus();
    auto inhibitCall =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
                                       QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("Inhibit"));

    inhibitCall.setArguments(
        {{QGuiApplication::desktopFileName()}, {i18nc("@info:status  explanation for sleep inhibit during rendering", "Rendering video")}});

    auto asyncReply = sessionBus.asyncCall(inhibitCall);

    auto replyWatcher = new QDBusPendingCallWatcher(asyncReply, this);

    QObject::connect(replyWatcher, &QDBusPendingCallWatcher::finished, this, &PowerManagementInterface::inhibitDBusCallFinishedPlasmaWorkspace);
#endif
}

void PowerManagementInterface::uninhibitSleepPlasmaWorkspace()
{
#ifndef NODBUS
    auto sessionBus = QDBusConnection::sessionBus();

    auto uninhibitCall =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
                                       QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("UnInhibit"));

    uninhibitCall.setArguments({{d->mInhibitSleepCookie}});

    auto asyncReply = sessionBus.asyncCall(uninhibitCall);

    auto replyWatcher = new QDBusPendingCallWatcher(asyncReply, this);
    QObject::connect(replyWatcher, &QDBusPendingCallWatcher::finished, this, &PowerManagementInterface::uninhibitDBusCallFinishedPlasmaWorkspace);
#endif
}

void PowerManagementInterface::inhibitSleepGnomeWorkspace()
{
#ifndef NODBUS
    auto sessionBus = QDBusConnection::sessionBus();

    auto inhibitCall = QDBusMessage::createMethodCall(QStringLiteral("org.gnome.SessionManager"), QStringLiteral("/org/gnome/SessionManager"),
                                                      QStringLiteral("org.gnome.SessionManager"), QStringLiteral("Inhibit"));

    // See: https://gitlab.gnome.org/GNOME/gnome-session/-/blob/master/gnome-session/org.gnome.SessionManager.xml
    // The last argument are flag settings to specify what should be inhibited:
    //    1  = Inhibit logging out
    //    2  = Inhibit user switching
    //    4  = Inhibit suspending the session or computer
    //    8  = Inhibit the session being marked as idle
    //    16 = Inhibit auto-mounting removable media for the session
    inhibitCall.setArguments({{QCoreApplication::applicationName()},
                              {uint(0)},
                              {i18nc("@info:status explanation for sleep inhibit during rendering", "Playing or rendering video")},
                              {uint(4)}});

    auto asyncReply = sessionBus.asyncCall(inhibitCall);

    auto replyWatcher = new QDBusPendingCallWatcher(asyncReply, this);

    QObject::connect(replyWatcher, &QDBusPendingCallWatcher::finished, this, &PowerManagementInterface::inhibitDBusCallFinishedGnomeWorkspace);
#endif
}

void PowerManagementInterface::uninhibitSleepGnomeWorkspace()
{
#ifndef NODBUS
    auto sessionBus = QDBusConnection::sessionBus();

    auto uninhibitCall = QDBusMessage::createMethodCall(QStringLiteral("org.gnome.SessionManager"), QStringLiteral("/org/gnome/SessionManager"),
                                                        QStringLiteral("org.gnome.SessionManager"), QStringLiteral("UnInhibit"));

    uninhibitCall.setArguments({{d->mInhibitSleepCookie}});

    auto asyncReply = sessionBus.asyncCall(uninhibitCall);

    auto replyWatcher = new QDBusPendingCallWatcher(asyncReply, this);

    QObject::connect(replyWatcher, &QDBusPendingCallWatcher::finished, this, &PowerManagementInterface::uninhibitDBusCallFinishedGnomeWorkspace);
#endif
}

void PowerManagementInterface::inhibitSleepWindowsWorkspace()
{
#if defined Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#endif
}

void PowerManagementInterface::uninhibitSleepWindowsWorkspace()
{
#if defined Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

void PowerManagementInterface::inhibitDimWindowsWorkspace()
{
#if defined Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
#endif
}

void PowerManagementInterface::uninhibitDimWindowsWorkspace()
{
#if defined Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

#include "moc_powermanagementinterface.cpp"
