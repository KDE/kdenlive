/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Matthias Kretz <kretz@kde.org>

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KDENLIVECORE_EXPORT_H
#define KDENLIVECORE_EXPORT_H


#ifndef KDENLIVECORE_EXPORT
#if defined(MAKE_KDENLIVECORE_LIB)
/* We are building this library */
#define KDENLIVECORE_EXPORT Q_DECL_EXPORT
#else
/* We are using this library */
#define KDENLIVECORE_EXPORT Q_DECL_IMPORT
#endif
#endif

#endif
