# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Julius Künzel <jk.kdedev@smartlab.uber.space>

add_subdirectory(dialogs)
set(kdenlive_SRCS
  ${kdenlive_SRCS}
  pythoninterfaces/otioconvertions.cpp
  pythoninterfaces/speechtotext.cpp
  pythoninterfaces/speechtotextvosk.cpp
  pythoninterfaces/speechtotextwhisper.cpp
  pythoninterfaces/abstractpythoninterface.cpp
  PARENT_SCOPE
)
