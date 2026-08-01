if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

include("${FSORG_SOURCE_DIR}/cmake/Sources.cmake")

set(PORTABLE_SOURCES
        ${DOMAIN_SOURCES}
        ${APPLICATION_SOURCES}
        ${INFRASTRUCTURE_SOURCES}
        ${VIEWMODEL_SOURCES}
        ${VIEW_SOURCES}
)

set(WIN32_HEADERS "windows.h" "shlobj.h" "shlobj_core.h" "tlhelp32.h" "dwmapi.h" "winioctl.h" "objbase.h")

set(OFFENCES "")

foreach (PORTABLE_SOURCE IN LISTS PORTABLE_SOURCES)
    set(FULL_PATH "${FSORG_SOURCE_DIR}/${PORTABLE_SOURCE}")

    if (NOT EXISTS "${FULL_PATH}")
        list(APPEND OFFENCES "  ${PORTABLE_SOURCE}: listed in cmake/Sources.cmake but not on disk")
        continue ()
    endif ()

    file(STRINGS "${FULL_PATH}" LINES ENCODING UTF-8)

    foreach (LINE IN LISTS LINES)
        foreach (WIN32_HEADER IN LISTS WIN32_HEADERS)
            if (LINE MATCHES "^[ \t]*#[ \t]*include[ \t]*<${WIN32_HEADER}>")
                list(APPEND OFFENCES "  ${PORTABLE_SOURCE}: includes <${WIN32_HEADER}>")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (OFFENCES)
    list(REMOVE_DUPLICATES OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "A layer the portable configuration compiles reached for win32.\n"
            "Move the file to WINDOWS_INFRASTRUCTURE_SOURCES, or put the win32 call behind a port.\n"
            "${REPORT}")
endif ()

list(LENGTH PORTABLE_SOURCES HOW_MANY)
message(STATUS "No win32 header inside the ${HOW_MANY} portable sources.")
