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

include(cmake/AppTarget.cmake)
include(cmake/Tools.cmake)

if (BUILD_TESTING)
    include(cmake/Tests.cmake)
endif ()

include(cmake/Deployment.cmake)
