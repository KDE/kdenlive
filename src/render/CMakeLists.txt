# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Julius Künzel <jk.kdedev@smartlab.uber.space>

if(NODBUS)
    list(APPEND kdenlive_SRCS
      render/renderserver.cpp)
endif()

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  render/renderrequest.cpp
  PARENT_SCOPE)
