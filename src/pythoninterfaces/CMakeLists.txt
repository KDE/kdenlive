# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Julius Künzel <jk.kdedev@smartlab.uber.space>

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  pythoninterfaces/otioconvertions.cpp
  pythoninterfaces/speechtotext.cpp
  pythoninterfaces/abstractpythoninterface.cpp
  PARENT_SCOPE
)
