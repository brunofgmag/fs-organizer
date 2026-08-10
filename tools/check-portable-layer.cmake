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

set(WHOLLY_PORTABLE_TREES "src/domain" "src/application" "src/viewmodel")

set(WIN32_HEADERS "windows.h" "shlobj.h" "shlobj_core.h" "tlhelp32.h" "dwmapi.h" "winioctl.h" "objbase.h")

set(OFFENCES "")
set(PORTABLE_HEADERS "")

foreach (TREE IN LISTS WHOLLY_PORTABLE_TREES)
    file(GLOB_RECURSE FOUND_HEADERS "${FSORG_SOURCE_DIR}/${TREE}/*.h")

    foreach (FOUND_HEADER IN LISTS FOUND_HEADERS)
        file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${FOUND_HEADER}")
        list(APPEND PORTABLE_HEADERS "${SHOWN}")
    endforeach ()
endforeach ()

foreach (PORTABLE_SOURCE IN LISTS PORTABLE_SOURCES)
    string(REGEX REPLACE "\\.cpp$" ".h" BESIDE_IT "${PORTABLE_SOURCE}")

    if (NOT BESIDE_IT STREQUAL PORTABLE_SOURCE AND EXISTS "${FSORG_SOURCE_DIR}/${BESIDE_IT}")
        list(APPEND PORTABLE_HEADERS "${BESIDE_IT}")
    endif ()
endforeach ()

list(REMOVE_DUPLICATES PORTABLE_HEADERS)

foreach (PORTABLE_SOURCE IN LISTS PORTABLE_SOURCES)
    if (NOT EXISTS "${FSORG_SOURCE_DIR}/${PORTABLE_SOURCE}")
        list(APPEND OFFENCES "  ${PORTABLE_SOURCE}: listed in cmake/Sources.cmake but not on disk")
    endif ()
endforeach ()

set(SCANNED ${PORTABLE_SOURCES} ${PORTABLE_HEADERS})

foreach (SCANNED_FILE IN LISTS SCANNED)
    set(FULL_PATH "${FSORG_SOURCE_DIR}/${SCANNED_FILE}")

    if (NOT EXISTS "${FULL_PATH}")
        continue ()
    endif ()

    file(STRINGS "${FULL_PATH}" LINES ENCODING UTF-8)

    foreach (LINE IN LISTS LINES)
        foreach (WIN32_HEADER IN LISTS WIN32_HEADERS)
            if (LINE MATCHES "^[ \t]*#[ \t]*include[ \t]*<${WIN32_HEADER}>")
                list(APPEND OFFENCES "  ${SCANNED_FILE}: includes <${WIN32_HEADER}>")
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

list(LENGTH PORTABLE_SOURCES HOW_MANY_SOURCES)
list(LENGTH PORTABLE_HEADERS HOW_MANY_HEADERS)
message(STATUS "No win32 header inside the ${HOW_MANY_SOURCES} portable sources or the ${HOW_MANY_HEADERS} headers they carry.")
