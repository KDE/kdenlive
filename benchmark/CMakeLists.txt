# SPDX-FileCopyrightText: 2026 Kdenlive contributors
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

include(ECMAddTests)
kde_enable_exceptions()

ecm_add_test(
    timelinemodelbenchmark.cpp
    TEST_NAME timelinemodelbenchmark
    LINK_LIBRARIES kdenliveLib
)

target_include_directories(timelinemodelbenchmark PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
set_property(TARGET timelinemodelbenchmark PROPERTY CXX_STANDARD 14)
set_tests_properties(timelinemodelbenchmark PROPERTIES TIMEOUT 600 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
