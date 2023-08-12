# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Vincent Pinon <vpinon@kde.org>

if(HAVE_LINUX_INPUT_H)
  include_directories(${CMAKE_BINARY_DIR})
  add_library(media_ctrl STATIC mediactrl.c)
  set_property(TARGET media_ctrl PROPERTY C_STANDARD 99)
endif()
