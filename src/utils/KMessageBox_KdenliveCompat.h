/*
    SPDX-FileCopyrightText: 2022 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KMESSAGEBOX_KDEVCOMPAT
#define KMESSAGEBOX_KDEVCOMPAT

#include <KMessageBox>
#include <kwidgetsaddons_version.h>

// Temporary private porting helper to avoid if-else cluttering of the codebase.
// Once KF5_DEP_VERSION >= 5.100 is reached:
// * rm all #include <KMessageBox_KDevCompat>
// * git rm KMessageBox_KDevCompat
#if KWIDGETSADDONS_VERSION < QT_VERSION_CHECK(5, 100, 0)
namespace KMessageBox {

inline constexpr auto PrimaryAction = KMessageBox::Yes;
inline constexpr auto SecondaryAction = KMessageBox::No;

inline ButtonCode questionTwoActions(QWidget *parent, const QString &text, const QString &title, const KGuiItem &primaryAction, const KGuiItem &secondaryAction,
                                     const QString &dontAskAgainName = QString(), Options options = Notify)
{
    return questionYesNo(parent, text, title, primaryAction, secondaryAction, dontAskAgainName, options);
}

inline ButtonCode questionTwoActionsCancel(QWidget *parent, const QString &text, const QString &title, const KGuiItem &primaryAction,
                                           const KGuiItem &secondaryAction, const KGuiItem &cancelAction = KStandardGuiItem::cancel(),
                                           const QString &dontAskAgainName = QString(), Options options = Notify)
{
    return questionYesNoCancel(parent, text, title, primaryAction, secondaryAction, cancelAction, dontAskAgainName, options);
}

inline ButtonCode warningTwoActions(QWidget *parent, const QString &text, const QString &title, const KGuiItem &primaryAction, const KGuiItem &secondaryAction,
                                    const QString &dontAskAgainName = QString(), Options options = Options(Notify | Dangerous))
{
    return warningYesNo(parent, text, title, primaryAction, secondaryAction, dontAskAgainName, options);
}

inline ButtonCode warningTwoActionsList(QWidget *parent, const QString &text, const QStringList &strlist, const QString &title, const KGuiItem &primaryAction,
                                        const KGuiItem &secondaryAction, const QString &dontAskAgainName = QString(),
                                        Options options = Options(Notify | Dangerous))
{
    return warningYesNoList(parent, text, strlist, title, primaryAction, secondaryAction, dontAskAgainName, options);
}

inline ButtonCode warningTwoActionsCancel(QWidget *parent, const QString &text, const QString &title, const KGuiItem &primaryAction,
                                          const KGuiItem &secondaryAction, const KGuiItem &cancelAction = KStandardGuiItem::cancel(),
                                          const QString &dontAskAgainName = QString(), Options options = Options(Notify | Dangerous))
{
    return warningYesNoCancel(parent, text, title, primaryAction, secondaryAction, cancelAction, dontAskAgainName, options);
}

} // namespace KMessageBox
#endif

#endif // KMESSAGEBOX_KDEVCOMPAT
