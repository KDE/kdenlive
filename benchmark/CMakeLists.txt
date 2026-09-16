# SPDX-FileCopyrightText: 2026 Kdenlive contributors
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

kde_enable_exceptions()

add_executable(timelinemodelbenchmark
    timelinemodelbenchmark.cpp
)
target_link_libraries(timelinemodelbenchmark PRIVATE kdenliveLib)
target_compile_definitions(timelinemodelbenchmark PRIVATE QT_FORCE_ASSERTS)

target_include_directories(timelinemodelbenchmark PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
set_property(TARGET timelinemodelbenchmark PROPERTY CXX_STANDARD 14)
