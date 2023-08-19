# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Nicolas Carion <french.ebook.lover@gmail.com>

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  transitions/transitionsrepository.cpp
  transitions/transitionlist/view/transitionlistwidget.cpp
  transitions/transitionlist/model/transitiontreemodel.cpp
  transitions/transitionlist/model/transitionfilter.cpp
  transitions/view/transitionstackview.cpp
  transitions/view/mixstackview.cpp
  PARENT_SCOPE)

