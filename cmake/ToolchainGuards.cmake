if (FSORG_TESTS_ONLY)
    return()
endif ()

if (NOT WIN32)
    message(FATAL_ERROR
            "fs-organizer targets Windows: NTFS reparse points have no portable equivalent. "
            "Configure with -DFSORG_TESTS_ONLY=ON to build the domain tests on a host platform.")
endif ()

if (MSVC)
    if (MSVC_VERSION LESS 1930)
        message(FATAL_ERROR "Visual Studio 2022 (MSVC 19.30 or newer) is required.")
    endif ()
elseif (MINGW)
    message(STATUS "Configuring MinGW toolchain (cross-compile path).")
else ()
    message(FATAL_ERROR
            "This project requires MSVC 2022 x64 (native) or MinGW (cross-compile); "
            "the selected compiler is neither.")
endif ()
