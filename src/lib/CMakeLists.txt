# SPDX-FileCopyrightText: 2012 Simon A. Eugster (Granjow) <simon.eu@gmail.com>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

add_subdirectory(audio)
add_subdirectory(external)
set(kdenlive_SRCS
  ${kdenlive_SRCS}
  lib/qtimerWithTime.cpp
  PARENT_SCOPE)

