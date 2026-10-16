if (WIN32)
    if (MSVC)
        if (MSVC_VERSION LESS 1930)
            message(FATAL_ERROR "Visual Studio 2022 (MSVC 19.30 or newer) is required.")
        endif ()
    elseif (MINGW)
        message(STATUS "Configuring MinGW toolchain (cross-compile path).")
    else ()
        message(FATAL_ERROR
                "This project requires MSVC 2022 x64 (native) or MinGW (cross-compile) on Windows; "
                "the selected compiler is neither.")
    endif ()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "This project requires GCC 13 or newer on Linux; the selected compiler is "
                "${CMAKE_CXX_COMPILER_ID}.")
    endif ()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
        message(FATAL_ERROR "GCC 13 or newer is required on Linux, found ${CMAKE_CXX_COMPILER_VERSION}.")
    endif ()
else ()
    message(FATAL_ERROR
            "fs-organizer builds for Windows (MSVC 2022 x64) and for Linux (GCC 13 or newer): "
            "NTFS reparse points and Proton prefixes have no equivalent elsewhere.")
endif ()
