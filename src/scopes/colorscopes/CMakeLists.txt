# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Alberto Villa <avilla@FreeBSD.org>, Jean-Baptiste Mardelle <jb@kdenlive.org>, Vincent PINON <vincent.pinon@laposte.net>

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  scopes/colorscopes/colorconstants.h
  scopes/colorscopes/abstractgfxscopewidget.cpp
  scopes/colorscopes/colorplaneexport.cpp
  scopes/colorscopes/histogram.cpp
  scopes/colorscopes/histogramgenerator.cpp
  scopes/colorscopes/rgbparade.cpp
  scopes/colorscopes/rgbparadegenerator.cpp
  scopes/colorscopes/vectorscope.cpp
  scopes/colorscopes/vectorscopegenerator.cpp
  scopes/colorscopes/waveform.cpp
  scopes/colorscopes/waveformgenerator.cpp
  PARENT_SCOPE
)
