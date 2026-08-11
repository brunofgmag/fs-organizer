if (NOT WIN32)
    message(FATAL_ERROR
            "fs-organizer targets Windows and only Windows: NTFS reparse points have no portable "
            "equivalent, and the domain tests are built and run here like every other target.")
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
