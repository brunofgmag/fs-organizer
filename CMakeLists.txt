cmake_minimum_required(VERSION 3.21)

if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif ()

project(fs-organizer LANGUAGES CXX)

include(cmake/GitHooks.cmake)
include(cmake/ProjectOptions.cmake)
include(cmake/ToolchainGuards.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Sources.cmake)
include(cmake/Libraries.cmake)

if (NOT FSORG_TESTS_ONLY)
    include(cmake/AppTarget.cmake)
    include(cmake/Tools.cmake)
endif ()

if (BUILD_TESTING)
    include(cmake/Tests.cmake)
endif ()

if (NOT FSORG_TESTS_ONLY)
    include(cmake/Deployment.cmake)
endif ()
