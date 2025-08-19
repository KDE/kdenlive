/*
   SPDX-FileCopyrightText: 2019 (c) Matthieu Gallien <matthieu_gallien@yahoo.fr>
   SPDX-FileCopyrightText: 2025 (c) Jean-Baptiste Mardelle <jb@kdenlive.org>
   SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef POWERMANAGEMENTINTERFACE_H
#define POWERMANAGEMENTINTERFACE_H

#include <QObject>

#include <memory>

class QDBusPendingCallWatcher;
class PowerManagementInterfacePrivate;

class PowerManagementInterface : public QObject
{

    Q_OBJECT

    Q_PROPERTY(bool preventSleep READ preventSleep WRITE setPreventSleep NOTIFY preventSleepChanged)
    Q_PROPERTY(bool preventDim READ preventDim WRITE setPreventDim NOTIFY preventDimChanged)

    Q_PROPERTY(bool sleepInhibited READ sleepInhibited NOTIFY sleepInhibitedChanged)
    Q_PROPERTY(bool dimInhibited READ dimInhibited NOTIFY dimInhibitedChanged)

public:
    explicit PowerManagementInterface(QObject *parent = nullptr);

    ~PowerManagementInterface() override;

    [[nodiscard]] bool preventSleep() const;
    [[nodiscard]] bool preventDim() const;

    [[nodiscard]] bool sleepInhibited() const;
    [[nodiscard]] bool dimInhibited() const;

Q_SIGNALS:

    void preventSleepChanged();
    void sleepInhibitedChanged();
    void preventDimChanged();
    void dimInhibitedChanged();

public Q_SLOTS:

    void setPreventSleep(bool value);
    void setPreventDim(bool value);

    void retryInhibitingSleep();
    void retryInhibitingDim();

private Q_SLOTS:

    void hostSleepInhibitChanged();

    void inhibitDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher);

    void uninhibitDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher);

    void inhibitDimDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher);

    void uninhibitDimDBusCallFinishedPlasmaWorkspace(QDBusPendingCallWatcher *aWatcher);

    void inhibitDBusCallFinishedGnomeWorkspace(QDBusPendingCallWatcher *aWatcher);

    void uninhibitDBusCallFinishedGnomeWorkspace(QDBusPendingCallWatcher *aWatcher);

private:
    void inhibitDim();
    void uninhibitDim();
    void inhibitSleepPlasmaWorkspace();

    void uninhibitSleepPlasmaWorkspace();

    void inhibitSleepGnomeWorkspace();

    void uninhibitSleepGnomeWorkspace();

    void inhibitSleepWindowsWorkspace();

    void uninhibitSleepWindowsWorkspace();

    void inhibitDimWindowsWorkspace();

    void uninhibitDimWindowsWorkspace();

    std::unique_ptr<PowerManagementInterfacePrivate> d;
};

#endif // POWERMANAGEMENTINTERFACE_H
