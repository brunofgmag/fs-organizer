cmake_minimum_required(VERSION 3.21)

project(fs-organizer LANGUAGES CXX)

include(cmake/GitHooks.cmake)
include(cmake/ProjectOptions.cmake)
include(cmake/ToolchainGuards.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Sources.cmake)

if (NOT FSORG_TESTS_ONLY)
    include(cmake/AppTarget.cmake)
endif ()

if (BUILD_TESTING)
    include(cmake/Tests.cmake)
endif ()

if (NOT FSORG_TESTS_ONLY)
    include(cmake/Deployment.cmake)
endif ()
