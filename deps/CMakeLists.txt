# SPDX-FileCopyrightText: 2025 Darby Johnston <darbyjohnston@yahoo.com>
# SPDX-License-Identifier: BSD-2-Clause

# OpenTimelineIO
#
# TODO: Use CMake's FetchContent to include OpenTimelineIO until a system
# package is available.
if (FETCH_OTIO)
    include(FetchContent)
    set(OTIO_INSTALL_COMMANDLINE_TOOLS OFF)
    set(OTIO_SHARED_LIBS OFF)
    set(OTIO_CXX_INSTALL OFF)
    set(BUILD_TESTING OFF)
    FetchContent_Declare(
        OTIO
        GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/OpenTimelineIO.git
        GIT_TAG v0.17.0)
    FetchContent_MakeAvailable(OTIO)
    set(otio_SOURCE_DIR ${otio_SOURCE_DIR} PARENT_SCOPE)
endif()
